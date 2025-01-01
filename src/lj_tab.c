/*
 * Table handling.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_mem.h"
#include "uj_err.h"
#include "lj_tab.h"
#include "uj_sbuf.h"
#include "uj_str.h"
#include "utils/fp.h"

/*
 * The following naming conventions are used throughout this file:
 * - number: count/amount
 * - length: speaking of containers, number of elements in the container
 * - size:   number of elements which can be stored in allocated memory
 */

/*
 * According to Lua reference, tables as arrays are indexed from 1.
 * Due to iteration implementation reasons (see BC_ITERN implementation)
 * array buffer size is (length + 1).
 * Elements are allocated from this 1-offset on.
 * 'GCtab->asize' = length ? length + 1 : 0
 */
#define TAB_ARR_EL_START_IDX 1

static TValue* lj_tab_setinth(lua_State*, GCtab*, int32_t);


/* -- Object hashing ------------------------------------------------------ */

/* Hash values are masked with the table hash mask and used as an index.
*/
static LJ_AINLINE Node* hashmask(const GCtab *t, uint32_t hash) {
  Node *n = t->node;
  return &n[hash & t->hmask];
}

/* -- Uility functions ---------------------------------------------------- */

/* String hashes are precomputed when they are interned. */
#define hashstr(t, s)           hashmask(t, (s)->hash)

#define hashlohi(t, lo, hi)     hashmask((t), hashrot((lo), (hi)))
#define hashnum(t, o)           hashlohi((t), (o)->u32.lo, ((o)->u32.hi << 1))

/* Hash an arbitrary key and return its anchor position in the hash table.
*/
static Node* hashkey(const GCtab *t, const TValue *key) {
  if (tvisstr(key)) {
    return hashstr(t, strV(key));
  } else if (tvisnum(key)) {
    return hashnum(t, key);
  } else if (tvisbool(key)) {
    return hashmask(t, boolV(key));
  } else {
    /* Only least significant 32 bits of host ptr are hashed.
    ** TODO: review.
    */
    return hashlohi(t, key->u32.lo, key->u32.lo + HASH_BIAS);
  }
}

/* -- Table creation and destruction -------------------------------------- */

/*
** Q: Why all of these copies of t->hmask, t->node etc. to local variables?
** A: Because alias analysis for C is _really_ tough.
**    Even state-of-the-art C compilers won't produce good code without this.
*/

/* Clear hash part of table.
*/
static LJ_AINLINE void clearhpart(GCtab *t) {
  uint32_t i, hmask = t->hmask;
  Node *node = t->node;
  lua_assert(t->hmask != 0);
  for (i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    n->next = NULL;
    setnilV(&n->key);
    setnilV(&n->val);
  }
}

/* Create new hash part for table.
*/
static LJ_AINLINE void newhpart(lua_State *L, GCtab *t, uint32_t hbits) {
  if (hbits) { /* Allocate and clear new hash part. */
    uint32_t hsize;
    Node *node;
    if (hbits > LJ_MAX_HBITS) {
      uj_err(L, UJ_ERR_TABOV);
    }
    hsize = lj_pow2(hbits);
    node = (Node *)uj_mem_alloc(L, hsize * sizeof(Node));
    t->freetop = &node[hsize];
    t->node = node;
    t->hmask = hsize-1;
    clearhpart(t);
  } else { /* Reduce hash part to nilnode pointer. */
    global_State *g = G(L);
    t->node = &g->nilnode;
    t->hmask = 0;
    t->freetop = &g->nilnode;
  }
}

/* Clear array part of table.
*/
static LJ_AINLINE void clearapart(GCtab *t) {
  uint32_t i, asize = t->asize;
  TValue *array = t->array;
  for (i = 0; i < asize; i++) {
    setnilV(&array[i]);
  }
}

static LJ_AINLINE TValue *tab_apart_alloc(lua_State *L, size_t size) {
  return (TValue *)uj_mem_alloc(L, size * sizeof(TValue));
}

static LJ_AINLINE void tab_hpart_free(global_State *g, Node *hpart, size_t size) {
  uj_mem_free(MEM_G(g), hpart, size * sizeof(Node));
}

/* Create a new table. Note: the slots are not initialized (yet).
*/
static GCtab* newtab(lua_State *L, uint32_t asize, uint32_t hbits) {
  GCtab *t;

  /* Create table object and colocate array part if possible.
  */
  if (LJ_MAX_COLOSIZE != 0 && asize > 0 && asize <= LJ_MAX_COLOSIZE) {
    lua_assert((sizeof(GCtab) & 7) == 0);
    t = (GCtab *)uj_obj_new(L, sizetabcolo(asize));
    t->colo = (int8_t)asize;
    t->array = (TValue *)((char *)t + sizeof(GCtab));
    t->asize = asize;
  } else {  /* Otherwise separately allocate the array part. */
    t = (GCtab *)uj_obj_new(L, sizeof(GCtab));
    t->colo = 0;
    t->array = NULL;
    t->asize = 0;  /* In case the array allocation fails. */
    if (asize > 0) {
      if (asize > LJ_MAX_ASIZE) {
        uj_err(L, UJ_ERR_TABOV);
      }
      t->array = tab_apart_alloc(L, asize);
      t->asize = asize;
    }
  }
  clearapart(t);

  t->gct = ~LJ_TTAB;

  /* When created, table has no metatable and no metamethods.
  ** Fill metamethod cache accordingly.
  */
  t->metatable = NULL;
  t->nomm = (uint8_t)~0;

  /* Setup hash part.
  */
  newhpart(L, t, hbits);

  G(L)->gc.tabnum++;
  return t;
}

/* Create a new table.
**
** IMPORTANT NOTE: The API differs from lua_createtable()!
**
** The array size is non-inclusive. E.g. asize=128 creates array slots
** for 0..127, but not for 128. If you need slots 1..128, pass asize=129
** (slot 0 is wasted in this case).
**
** The hash size is given in hash bits. hbits=0 means no hash part.
** hbits=1 creates 2 hash slots, hbits=2 creates 4 hash slots and so on.
*/
GCtab* lj_tab_new(lua_State *L, uint32_t asize, uint32_t hbits) {
  return newtab(L, asize, hbits);
}

#if LJ_HASJIT
/* Compact version of lj_tab_new.
** asize resides in lower 24 bits of ahsize, hbits - in upper 8 bits.
*/
GCtab* lj_tab_new_jit(lua_State *L, uint32_t ahsize) {
  return newtab(L, ahsize & 0xffffff, ahsize >> 24);
}
#endif

/* Duplicate a table.
*/
GCtab* lj_tab_dup(lua_State *L, const GCtab *kt) {
  GCtab *t;
  uint32_t asize, hmask;
  t = newtab(L, kt->asize, kt->hmask > 0 ? lj_bsr(kt->hmask)+1 : 0);
  lua_assert(kt->asize == t->asize && kt->hmask == t->hmask);
  t->nomm = 0;  /* Keys with metamethod names may be present. */
  asize = kt->asize;
  if (asize > 0) {
    TValue *array = t->array;
    TValue *karray = kt->array;
    if (asize < 64) {  /* An inlined loop beats memcpy for < 512 bytes. */
      uint32_t i;
      for (i = 0; i < asize; i++) {
        copyTV(L, &array[i], &karray[i]);
      }
    } else {
      memcpy(array, karray, asize*sizeof(TValue));
    }
  }
  hmask = kt->hmask;
  if (hmask > 0) {
    uint32_t i;
    Node *node = t->node;
    Node *knode = kt->node;
    ptrdiff_t d = (char *)node - (char *)knode;
    t->freetop = (Node *)((char *)kt->freetop + d);
    for (i = 0; i <= hmask; i++) {
      Node *kn = &knode[i];
      Node *n = &node[i];
      Node *next = kn->next;
      /* Don't use copyTV here, since it asserts on a copy of a dead key. */
      n->val = kn->val; n->key = kn->key;
      n->next = (next == NULL) ? next : (Node *)((char *)next + d);
    }
  }
  return t;
}

/* Free a table.
*/
void lj_tab_free(global_State *g, GCtab *t) {
  if (t->hmask > 0) {
    tab_hpart_free(g, t->node, t->hmask + 1);
  }

  if (t->asize > 0 && LJ_MAX_COLOSIZE != 0 && t->colo <= 0) {
    uj_mem_free(MEM_G(g), t->array, t->asize * sizeof(TValue));
  }

  if (LJ_MAX_COLOSIZE != 0 && t->colo) {
    uj_mem_free(MEM_G(g), t, sizetabcolo((uint32_t)t->colo & 0x7f));
  } else {
    uj_mem_free(MEM_G(g), t, sizeof(*t));
  }

  g->gc.tabnum--;
}

/* Overall memory footprint of the table, including
** the table itself, array and hash parts.
*/
size_t lj_tab_sizeof(const GCtab *t) {
  size_t hpart_size = t->hmask ? sizeof(Node) * (t->hmask + 1) : 0;
  return sizeof(GCtab) + sizeof(TValue) * t->asize + hpart_size;
}

/* -- Table resizing ------------------------------------------------------ */

/* Resize a table to fit the new array/hash part sizes.
*/
static void resizetab(lua_State *L, GCtab *t, uint32_t asize, uint32_t hbits) {
  Node *oldnode = t->node;
  uint32_t oldasize = t->asize;
  uint32_t oldhmask = t->hmask;
  if (asize > oldasize) {  /* Array part grows? */
    TValue *array;
    uint32_t i;
    if (asize > LJ_MAX_ASIZE) {
      uj_err(L, UJ_ERR_TABOV);
    }
    if (LJ_MAX_COLOSIZE != 0 && t->colo > 0) {
      /* A colocated array must be separated and copied. */
      TValue *oarray = t->array;
      array = tab_apart_alloc(L, asize);
      t->colo = (int8_t)(t->colo | 0x80);  /* Mark as separated (colo < 0). */
      for (i = 0; i < oldasize; i++) {
        copyTV(L, &array[i], &oarray[i]);
      }
    } else {
      array = (TValue *)uj_mem_realloc(L, t->array,
                          oldasize*sizeof(TValue), asize*sizeof(TValue));
    }
    t->array = array;
    t->asize = asize;
    for (i = oldasize; i < asize; i++) { /* Clear newly allocated slots. */
      setnilV(&array[i]);
    }
  }
  /* Create new (empty) hash part. */
  newhpart(L, t, hbits);

  if (asize < oldasize) {  /* Array part shrinks? */
    TValue *array = t->array;
    uint32_t i;
    t->asize = asize;  /* Note: This 'shrinks' even colocated arrays. */
    for (i = asize; i < oldasize; i++) { /* Reinsert old array values. */
      if (!tvisnil(&array[i])) {
        copyTV(L, lj_tab_setinth(L, t, (int32_t)i), &array[i]);
      }
    }
    /* Physically shrink only separated arrays. */
    if (LJ_MAX_COLOSIZE != 0 && t->colo <= 0) {
      t->array = uj_mem_realloc(L, array, oldasize*sizeof(TValue), asize*sizeof(TValue));
    }
  }
  if (oldhmask > 0) {  /* Reinsert pairs from old hash part. */
    uint32_t i;
    for (i = 0; i <= oldhmask; i++) {
      Node *n = &oldnode[i];
      if (!tvisnil(&n->val)) {
        copyTV(L, lj_tab_set(L, t, &n->key), &n->val);
      }
    }
    tab_hpart_free(G(L), oldnode, oldhmask + 1);
  }
}

/*
 * If key is an int key and is less than LJ_MAX_SIZE which is the value of
 * maximum possible int value to be put in array part.
 */
static uint32_t countint(const TValue *key, uint32_t *bins) {
  if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if ((uint32_t)k < LJ_MAX_ASIZE && nk == (lua_Number)k) {
      bins[(k > 2 ? lj_bsr((uint32_t)(k-1)) : 0)]++;
      return 1;
    }
  }
  return 0;
}

/*
 * Counts non-nil elements in array part.
 * As a side-effect, fills in bins array where bins[i] is element count
 * of integer keys in (2^i, 2^(i+1)] for i >= 1.
 * For i = 0, [0, 2].
 *
 * For instance, for the array [el, nil, el, el, nil, nil, el, el, el]
 *   bins[0] = 1
 *   bins[1] = 1
 *   bins[2] = 3
 *
 * NB: even though array[0] is technically not used in most cases (array[0] is
 *   used when explictily assigned 'array[0] = 1'), it _is_ taken into account
 *   in elements counting.
 * See below: iteration is started from 0 (i=0).
*/
static uint32_t countarray(const GCtab *t, uint32_t *bins) {
  uint32_t na, b, i;
  if (t->asize == 0) { return 0; }
  for (na = i = b = 0; b < LJ_MAX_ABITS; b++) {
    uint32_t n, top = 2u << b;
    TValue *array;
    if (top >= t->asize) {
      top = t->asize-1;
      if (i > top) { break; }
    }
    array = t->array;
    for (n = 0; i <= top; i++) {
      if (!tvisnil(&array[i])) { n++; }
    }
    bins[b] += n;
    na += n;
  }
  return na;
}

/*
 * Counts non-nil integer-keyed values which are subject for putting into
 * array part (see countint for criterion).
 * The reason why such keys are in hash part rather in array is the following:
 * at the point of key insertion array part was of insufficient size;
 * at the same time there wasn't a reason for its enlargement due to low
 * 'element package ratio' (see 'bestasize' function for details).
 */
static uint32_t counthash(const GCtab *t, uint32_t *bins, uint32_t *narray) {
  uint32_t total, na, i, hmask = t->hmask;
  Node *node = t->node;
  for (total = na = 0, i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    if (!tvisnil(&n->val)) {
      na += countint(&n->key, bins);
      total++;
    }
  }
  *narray += na;
  return total;
}

/*
 * Counts integer-keyed non-nil values in array and hash parts.
 */
static LJ_AINLINE uint32_t tab_aux_total_intkey_count(const GCtab *t,
                                                      uint32_t *bins)
{
  uint32_t new_asize = countarray(t, bins);
  return new_asize + counthash(t, bins, &new_asize);
}

/*
 * Creates an array of size: non-nil array element count + hash element count.
 */
static LJ_AINLINE GCtab* tab_aux_new_apart(lua_State *L,
                                           const GCtab *t)
{
  uint32_t bins[LJ_MAX_ABITS] = {0};

  const uint32_t intkey_count = tab_aux_total_intkey_count(t, bins);
  const uint32_t nasize = intkey_count ? intkey_count + TAB_ARR_EL_START_IDX : 0;

  return lj_tab_new(L, nasize, 0);
}

static uint32_t bestasize(uint32_t bins[], uint32_t *narray) {
  uint32_t b, sum, na = 0, sz = 0, nn = *narray;
  for (b = 0, sum = 0; 2*nn > lj_pow2(b) && sum != nn; b++) {
    if (bins[b] > 0 && 2*(sum += bins[b]) > lj_pow2(b)) {
      sz = (2u<<b) + TAB_ARR_EL_START_IDX;
      na = sum;
    }
  }
  *narray = sz;
  return na;
}

/* When trying to add new key to the hash part
** of the table and there is no suitable place for it,
** table re-hashing is triggered. This might change configuration
** of both array and hash parts of the table.
*/
static void rehashtab(lua_State *L, GCtab *t, const TValue *key) {
  uint32_t new_hbits; /* New size of hash part. */
  uint32_t new_asize; /* New size of array part. */

  if (tvisint(key)) {
    /* Special case for integer keys. Possibly adjust array part. */

    uint32_t bins[LJ_MAX_ABITS] = {0};
    uint32_t total, na;
    new_asize = countarray(t, bins);
    total = TAB_ARR_EL_START_IDX + new_asize;
    total += counthash(t, bins, &new_asize);
    new_asize += countint(key, bins);
    na = bestasize(bins, &new_asize);
    total -= na;
    new_hbits = hsize2hbits(total);
  } else {
    /* Generic key. Only adjust hash part. */

    new_asize = t->asize;
    new_hbits = lj_ffs(t->hmask + 1);
  }

  resizetab(L, t, new_asize, new_hbits);
}

#if LJ_HASFFI
void lj_tab_rehash(lua_State *L, GCtab *t)
{
  rehashtab(L, t, niltv(L));
}
#endif

void lj_tab_reasize(lua_State *L, GCtab *t, uint32_t nasize)
{
  resizetab(L, t, nasize + TAB_ARR_EL_START_IDX,
            t->hmask > 0 ? lj_bsr(t->hmask) + 1 : 0);
}

/* -- Table getters ------------------------------------------------------- */

const TValue* lj_tab_getinth(const GCtab *t, int32_t key) {
  TValue k;
  Node *n;
  setintV(&k, key);
  n = hashnum(t, &k);
  do {
    TValue *nodekey = &n->key;
    if (tvisnum(nodekey) && numV(nodekey) == numV(&k)) {
      return &n->val;
    }
  } while ((n = n->next));
  return NULL;
}

const TValue* lj_tab_getstr(const GCtab *t, const GCstr *key) {
  Node *n = hashstr(t, key);
  do {
    if (tvisstr(&n->key) && strV(&n->key) == key) {
      return &n->val;
    }
  } while ((n = n->next));
  return NULL;
}

const TValue* lj_tab_get(lua_State *L, GCtab *t, const TValue *key) {
  if (tvisstr(key)) {
    const TValue *tv = lj_tab_getstr(t, strV(key));
    if (tv) {
      return tv;
    }
  } else if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if (nk == (lua_Number)k) {
      const TValue *tv = lj_tab_getint(t, k);
      if (tv) {
        return tv;
      }
    } else {
      goto genlookup;  /* Else use the generic lookup. */
    }
  } else if (!tvisnil(key)) {
    Node *n;
  genlookup:
    n = hashkey(t, key);
    do {
      if (uj_obj_equal(&n->key, key)) {
        return &n->val;
      }
    } while ((n = n->next));
  }
  return niltv(L);
}

/* -- Table setters ------------------------------------------------------- */

/* Insert new key. Use Brent's variation to optimize the chain length.
*/
TValue* lj_tab_newkey(lua_State *L, GCtab *t, const TValue *key) {
  Node *n = hashkey(t, key);
  if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t)))) {
    uj_err(L, UJ_ERR_IMMUT_MODIF);
  }
  if (!tvisnil(&n->val) || t->hmask == 0) {
    Node *nodebase = t->node;
    Node *collide, *freenode = t->freetop;
    lua_assert(freenode >= nodebase && freenode <= nodebase+t->hmask+1);
    do {
      if (freenode == nodebase) {  /* No free node found? */
        rehashtab(L, t, key);  /* Rehash table. */
        return lj_tab_set(L, t, key);  /* Retry key insertion. */
      }
      --freenode;
    } while (!tvisnil(&(freenode)->key));
    t->freetop = freenode;
    lua_assert(freenode != &G(L)->nilnode);
    collide = hashkey(t, &n->key);
    if (collide != n) {  /* Colliding node not the main node? */
      while (collide->next != n) { /* Find predecessor. */
        collide = collide->next;
      }
      collide->next = freenode;  /* Relink chain. */
      /* Copy colliding node into free node and free main node. */
      freenode->val = n->val;
      freenode->key = n->key;
      freenode->next = n->next;
      n->next = NULL;
      setnilV(&n->val);
      /* Rechain pseudo-resurrected string keys with colliding hashes. */
      while (freenode->next) {
        Node *nn = freenode->next;
        if (tvisstr(&nn->key) && !tvisnil(&nn->val) &&
            hashstr(t, strV(&nn->key)) == n) {
          freenode->next = nn->next;
          nn->next = n->next;
          n->next = nn;
        } else {
          freenode = nn;
        }
      }
    } else {  /* Otherwise use free node. */
      freenode->next = n->next;  /* Insert into chain. */
      n->next = freenode;
      n = freenode;
    }
  }
  copyTV(L, &n->key, key);
  if (LJ_UNLIKELY(tvismzero(&n->key))) { setrawV(&n->key, 0); }
  lj_gc_anybarriert(L, t);
  lua_assert(tvisnil(&n->val));
  return &n->val;
}

static TValue* lj_tab_setinth(lua_State *L, GCtab *t, int32_t key) {
  TValue k;
  Node *n;
  if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t)))) {
    uj_err(L, UJ_ERR_IMMUT_MODIF);
  }
  setintV(&k, key);
  n = hashnum(t, &k);
  do {
    TValue *nodekey = &n->key;
    if (tvisnum(nodekey) && numV(nodekey) == numV(&k)) {
      return &n->val;
    }
  } while ((n = n->next));
  return lj_tab_newkey(L, t, &k);
}

TValue* lj_tab_setint(lua_State *L, GCtab *t, int32_t key) {
  if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t)))) {
    uj_err(L, UJ_ERR_IMMUT_MODIF);
  }
  if (inarray(t, key)) {
    return arrayslot(t, key);
  } else {
    return lj_tab_setinth(L, t, key);
  }
}

TValue* lj_tab_setstr(lua_State *L, GCtab *t, GCstr *key) {
  TValue k;
  Node *n = hashstr(t, key);
  if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t)))) {
    uj_err(L, UJ_ERR_IMMUT_MODIF);
  }
  do {
    if (tvisstr(&n->key) && strV(&n->key) == key) {
      return &n->val;
    }
  } while ((n = n->next));
  setstrV(L, &k, key);
  return lj_tab_newkey(L, t, &k);
}

TValue* lj_tab_set(lua_State *L, GCtab *t, const TValue *key) {
  Node *n;
  if (LJ_UNLIKELY(uj_obj_is_immutable(obj2gco(t)))) {
    uj_err(L, UJ_ERR_IMMUT_MODIF);
  }
  t->nomm = 0;  /* Invalidate negative metamethod cache. */
  if (tvisstr(key)) {
    return lj_tab_setstr(L, t, strV(key));
  } else if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if (nk == (lua_Number)k) {
      return lj_tab_setint(L, t, k);
    }
    if (tvisnan(key)) {
      uj_err(L, UJ_ERR_NANIDX);
    }
    /* Else use the generic lookup. */
  } else if (tvisnil(key)) {
    uj_err(L, UJ_ERR_NILIDX);
  }
  n = hashkey(t, key);
  do {
    if (uj_obj_equal(&n->key, key)) {
      return &n->val;
    }
  } while ((n = n->next));
  return lj_tab_newkey(L, t, key);
}

/* -- Table traversal ----------------------------------------------------- */

uint32_t lj_tab_nexta(const GCtab *t, uint32_t key)
{
  do {
    key++;
  } while (key < t->asize && tvisnil(arrayslot(t, key)));
  return key;
}

const Node *lj_tab_nexth(lua_State *L, const GCtab *t, const Node *n)
{
  Node *last = t->node + t->hmask;
  for (n++; n <= last; n++) {
    if (!tvisnil(&n->val))
      return n;
  }
  return &G(L)->nilnode;
}

/* Low-level traversal. Fetches the next key-value pair from `t`. Traversal is
** started from the *internal* storage index `key`, results are stored in the
** `out` and `out` + 1 stack slots. If the entire `t` is traversed, stack is
** left intact and 0 is returned. Otherwise returns the next internal storage
** index for subsequent calls.
** NB! BC_ITERN in vm_x86.dasc implements exactly the same semantics.
**/
static uint32_t tab_next(lua_State *L, const GCtab *t, uint32_t key, TValue *out) {
  uint32_t i;

  /* First traverse the array keys. */
  for (i = key; i < t->asize; i++) {
    if (tvisnil(arrayslot(t, i))) {
      continue;
    }

    setintV(out, i);
    copyTV(L, out + 1, arrayslot(t, i));
    return i + 1;
  }

  /* Then traverse the hash keys. */
  for (i -= t->asize; i <= t->hmask; i++) {
    const Node *n = &t->node[i];
    if (tvisnil(&n->val)) {
      continue;
    }

    copyTV(L, out, &n->key);
    copyTV(L, out + 1, &n->val);
    return i + 1 + t->asize;
  }

  return 0;
}

uint32_t lj_tab_iterate_jit(const GCtab *t, uint32_t key)
{
  uint32_t i;

  /* First traverse the array keys. */
  for (i = key; i < t->asize; i++) {
    if (tvisnil(arrayslot(t, i)))
      continue;
    return i + 1;
  }

  /* Then traverse the hash keys. */
  for (i -= t->asize; i <= t->hmask; i++) {
    const Node *n = &t->node[i];
    if (tvisnil(&n->val))
      continue;
    return i + 1 + t->asize;
  }

  return 0;
}

/* Get the traversal index of a key. */
static uint32_t keyindex(lua_State *L, const GCtab *t, const TValue *key) {
  if (tvisnum(key)) {
    lua_Number nk = numV(key);
    int32_t k = lj_num2int(nk);
    if ((uint32_t)k < t->asize && nk == (lua_Number)k) {
      return (uint32_t)k;  /* Array key indexes: [0..t->asize-1] */
    }
  }
  if (!tvisnil(key)) {
    Node *n = hashkey(t, key);
    do {
      if (uj_obj_equal(&n->key, key)) {
        return t->asize + (uint32_t)(n - t->node);
        /* Hash key indexes: [t->asize..t->asize+t->nmask] */
      }
    } while ((n = n->next));
    if (key->u32.hi == LJ_ITERN_MARK) { /* ITERN was despecialized while running. */
      return key->u32.lo - 1;
    }
    uj_err(L, UJ_ERR_NEXTIDX);
    return 0;  /* unreachable */
  }
  return ~0u;  /* A nil key starts the traversal. */
}

int lj_tab_next(lua_State *L, const GCtab *t, TValue *key) {
  uint32_t next = tab_next(L, t, keyindex(L, t, key) + 1, key);
  return (0 != next)? 1 : 0;
}

uint32_t lj_tab_iterate(lua_State *L, const GCtab *t, uint32_t key) {
  uint32_t next = tab_next(L, t, key, L->top);
  if (0 != next) {
    L->top += 2;
  }
  return next;
}

/* -- Table length calculation -------------------------------------------- */

static size_t unbound_search(const GCtab *t, size_t j) {
  const TValue *tv;
  size_t i = j;  /* i is zero or a present index */
  j++;
  /* find `i' and `j' such that i is present and j is not */
  while ((tv = lj_tab_getint(t, (int32_t)j)) && !tvisnil(tv)) {
    i = j;
    j *= 2;
    if (j > (size_t)(INT_MAX-2)) {  /* overflow? */ /* TODO: review and fix. */
      /* table was built with bad purposes: resort to linear search */
      i = 1;
      while ((tv = lj_tab_getint(t, (int32_t)i)) && !tvisnil(tv)) { i++; }
      return i - 1;
    }
  }
  /* now do a binary search between them */
  while (j - i > 1) {
    size_t m = (i+j)/2;
    const TValue *tvb = lj_tab_getint(t, (int32_t)m);
    if (tvb && !tvisnil(tvb)) { i = m; } else { j = m; }
  }
  return i;
}

/* Try to find a boundary in table `t'. A `boundary' is an integer index
** such that t[i] is non-nil and t[i+1] is nil (and 0 if t[1] is nil).
*/
size_t lj_tab_len(const GCtab *t) {
  size_t j = (size_t)t->asize;
  if (j > 1 && tvisnil(arrayslot(t, j-1))) {
    size_t i = 1;
    while (j - i > 1) {
      size_t m = (i+j)/2;
      if (tvisnil(arrayslot(t, m-1))) { j = m; } else { i = m; }
    }
    return i-1;
  }
  if (j) { j--; }
  if (t->hmask <= 0) {
    return j;
  }
  return unbound_search(t, j);
}

static LJ_AINLINE size_t tab_asize(const GCtab *t)
{
  size_t size, i;

  size = 0;
  for (i = 0; i < t->asize; i++) {
    if (!tvisnil(arrayslot(t, i)))
      size++;
  }

  return size;
}

static LJ_AINLINE size_t tab_hsize(const GCtab *t)
{
  size_t size, i;

  if (t->hmask == 0)
    return 0;

  size = 0;
  for (i = 0; i <= t->hmask; i++) {
    if (!tvisnil(&t->node[i].val))
      size++;
  }

  return size;
}

/* Count the number of non-nil values in both array and hash part */
size_t lj_tab_size(const GCtab *t)
{
  return tab_asize(t) + tab_hsize(t);
}

/* -- Table traversal & marking ------------------------------------------- */

/*
 * Following aux routines are used for traversing a table and applying some
 * "mark" to its contents. Currently used for sealing and immutability.
 */

/* Traverse array part and perform some abstract marking. */
static void tab_mark_array(lua_State *L, GCtab *t, gco_mark_flipper marker)
{
  size_t i;
  for (i = 0; i < t->asize; i++) {
    TValue *tv = arrayslot(t, i);
    if (tvisgcv(tv)) {
      marker(L, gcV(tv));
    }
  }
}

/* Traverse hash part and perform some abstract marking. */
static void tab_mark_hash(lua_State *L, GCtab *t, gco_mark_flipper marker)
{
  size_t i;
  size_t hmask = t->hmask;
  Node *node;

  if (hmask == 0) {
    return;
  }

  node = t->node;
  for (i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    if (tvisnil(&n->val)) {
      continue;
    }

    lua_assert(!tvisnil(&n->key));

    if (tvisgcv(&n->key)) {
      marker(L, gcV(&n->key));
    }

    if (tvisgcv(&n->val)) {
      marker(L, gcV(&n->val));
    }
  }
}

static void tab_mark_metatable(lua_State *L, const GCtab *t, gco_mark_flipper marker)
{
  GCtab *mt = t->metatable;
  if (NULL == mt) {
    return;
  }

  marker(L, obj2gco(mt));
}

/* -- Table traversal ------------------------------------------------------- */

void lj_tab_traverse(lua_State *L, GCtab *t, gco_mark_flipper marker)
{
  tab_mark_metatable(L, t, marker);
  tab_mark_array(L, t, marker);
  tab_mark_hash(L, t, marker);
}

/* -- Table built-ins ----------------------------------------------------- */

static void tab_aux_copy_hpart(
  TValue *dst, const GCtab *src, size_t dst_idx,
  void(*copy_node_part)(TValue *value, const Node *))
{
  size_t i;
  const Node *node = src->node;
  /* GCtab::hmask is always 2^n - 1. */
  const size_t hsize = src->hmask ? src->hmask + 1 : 0;

  for (i = 0; i < hsize; i++)
  {
    const Node *n = &node[i];

    if (tvisnil(&n->val))
      continue;

    copy_node_part(&dst[dst_idx], n);
    dst_idx++;
  }
}

static LJ_AINLINE void tab_aux_copy_key(TValue *value, const Node *n)
{
  *value = n->key;
}

static LJ_AINLINE void tab_aux_copy_value(TValue *value, const Node *n)
{
  *value = n->val;
}

/*
 * NB: in both functions below array elements' iteration is started
 *   from the next element after first element in array buffer.
 *   See TAB_ARR_EL_START_IDX define description.
 */
GCtab* lj_tab_keys(lua_State *L, const GCtab *src)
{
  size_t i, dst_idx;
  GCtab *dst = tab_aux_new_apart(L, src);

  if (0 == dst->asize)
    return dst;

  dst_idx = TAB_ARR_EL_START_IDX;

  for (i = 0; i < src->asize; i++)
  {
    if (tvisnil(arrayslot(src, i)))
      continue;

    setintV(arrayslot(dst, dst_idx), (uint32_t)i);
    dst_idx++;
  }

  tab_aux_copy_hpart(dst->array, src, dst_idx, tab_aux_copy_key);

  return dst;
}

GCtab* lj_tab_values(lua_State *L, const GCtab *src)
{
  size_t i, dst_idx;
  GCtab *dst = tab_aux_new_apart(L, src);

  if (0 == dst->asize)
    return dst;

  dst_idx = TAB_ARR_EL_START_IDX;

  for (i = 0; i < src->asize; i++)
  {
    if (tvisnil(arrayslot(src, i)))
      continue;

    *arrayslot(dst, dst_idx) = *arrayslot(src, i);
    dst_idx++;
  }

  tab_aux_copy_hpart(dst->array, src, dst_idx, tab_aux_copy_value);

  return dst;
}

GCtab* lj_tab_toset(lua_State *L, const GCtab *src)
{
  int32_t i;

  /* NB: an alternative to using 'asize' is to count elements to the first
   *   non-nil in src array part as 'ipairs' does: it will save space.
   *   However, it's assumed that 'toset' is used for tables without holes
   *   (otherwise why using it?) so the iteration is saved.
   */
  uint32_t hbits = hsize2hbits(src->asize ? src->asize - TAB_ARR_EL_START_IDX : 0);
  GCtab *dst = newtab(L, 0, hbits);

  /* NB: 'ipairs' which used in stock impl iterates from 1. */
  for (i = TAB_ARR_EL_START_IDX; ; i++) {
    const TValue *src_v = lj_tab_getint(src, i);
    TValue *dst_v;

    if (src_v == NULL || tvisnil(src_v))
      break;

    dst_v = lj_tab_set(L, dst, src_v);
    setboolV(dst_v, 1);
  }

  return dst;
}

static void tab_deepcopy_map_add(lua_State *L, GCtab *map, GCtab *key,
                                 GCtab *val)
{
  TValue keytv, *valuetv;

  settabV(L, &keytv, key);
  /* NOBARRIER: map is anchored to stack, i.e. never black */
  valuetv = lj_tab_set(L, map, &keytv);
  settabV(L, valuetv, val);
}

static void tab_deepcopy_tv(lua_State *L, TValue *dst, TValue *src,
			    struct deepcopy_ctx *ctx)
{
  GCobj *obj;

  if (!tvisgcv(src)) {
    copyTV(L, dst, src);
    return;
  }

  obj = gcV(src);
  setgcV(L, dst, uj_obj_deepcopy(L, obj, ctx), ~obj->gch.gct);
}

static void tab_deepcopy_apart(lua_State *L, GCtab *dst, GCtab *src,
			       struct deepcopy_ctx *ctx)
{
  size_t i;

  for (i = 0; i < src->asize; i++)
    tab_deepcopy_tv(L, arrayslot(dst, i), arrayslot(src, i), ctx);
}

static void tab_deepcopy_hpart(lua_State *L, GCtab *dst, GCtab *src,
			       struct deepcopy_ctx *ctx)
{
  size_t i;
  const size_t hmask = src->hmask;
  Node *node;

  if (hmask == 0)
    return;

  node = src->node;
  for (i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    TValue key, *value;

    if (tvisnil(&n->val))
      continue;

    lua_assert(!tvisnil(&n->key));

    tab_deepcopy_tv(L, &key, &n->key, ctx);
    /* NOBARRIER: lj_tab_newkey handles the barrier */
    value = lj_tab_newkey(L, dst, &key);
    tab_deepcopy_tv(L, value, &n->val, ctx);
  }
}

GCtab *lj_tab_deepcopy(lua_State *L, GCtab *src, struct deepcopy_ctx *ctx)
{
  GCtab *dst;

  dst = newtab(L, src->asize, src->hmask > 0 ? lj_bsr(src->hmask) + 1 : 0);
  lua_assert(src->asize == dst->asize);
  lua_assert(src->hmask == dst->hmask);

  lj_obj_set_mark(obj2gco(src));
  tab_deepcopy_map_add(L, ctx->map, src, dst);

  dst->nomm = 0; /* Invalidate metamethod cache */

  tab_deepcopy_apart(L, dst, src, ctx);
  tab_deepcopy_hpart(L, dst, src, ctx);

  return dst;
}

/*
 * table.concat, but returns NULL instead of throwing an error
 * NB: NULL can be asserted in a trace in order to raise an error
 */
GCstr *lj_tab_concat(lua_State *L, const GCtab *t, const GCstr *sep,
                     int32_t start, int32_t end, int32_t *fail)
{
  struct sbuf *sb = uj_sbuf_reset_tmp(L);
  const size_t seplen = sep ? sep->len : 0;
  int32_t i;

  if (start > end)
    return G(L)->strempty;

  for (i = start; i <= end; ++i) {
    const TValue *tv = lj_tab_getint(t, i);

    if (tv == NULL) {
    badtype:
      if (fail != NULL)
        *fail = i;
      return NULL;
    }

    if (tvisstr(tv))
      uj_sbuf_push_str(sb, strV(tv));
    else if (tvisnum(tv))
      uj_sbuf_push_number(sb, numV(tv));
    else
      goto badtype;

    if (seplen && i != end)
      uj_sbuf_push_str(sb, sep);
  }

  return uj_str_frombuf(L, sb);
}

struct chains_info {
  size_t nchains; /* number of collision chains in a table */
  size_t maxchain; /* maximum collision chain length */
};

/* Mark that i-th node in the table does not start a chain. */
static LJ_AINLINE void nchains_clear(uint8_t *map, size_t i)
{
  map[i / CHAR_BIT] &= (uint8_t)~(1 << i % CHAR_BIT);
}

static LJ_AINLINE int nchains_test(const uint8_t *map, size_t i)
{
  return map[i / CHAR_BIT] & (1 << (i % CHAR_BIT));
}

/* Counts the number of collision chains in the hash part of table t. */
static struct chains_info tab_nchains(lua_State *L, const GCtab *t)
{
  struct chains_info info = {0};
  uint8_t *map;
  size_t mapsz, i;

  if (t->hmask == 0)
    return info;

  mapsz = (t->hmask + 1) / CHAR_BIT + (t->hmask <= 3);
  map = (uint8_t *)uj_sbuf_tmp_bytes(L, mapsz);

  if (t->hmask > 3)
    memset(map, 0xff, mapsz);
  else
    *map = (uint8_t)((1 << (t->hmask + 1)) - 1);

  for (i = 0; i <= t->hmask; i++) {
    const Node *n = &t->node[i];

    if (tvisnil(&n->key)) { /* n cannot start a chain */
      nchains_clear(map, i);
    } else {
      const Node *nn = n->next;

      if (nn != NULL) { /* nn cannot start a chain */
        nchains_clear(map, (size_t)(nn - t->node));
      }
    }
  }

  for (i = 0; i <= t->hmask; i++) {
    if (nchains_test(map, i)) {
      size_t chain = 0;
      const Node *n = &t->node[i];

      info.nchains++;

      do {
        chain++;
        n = n->next;
      } while (n != NULL);

      if (chain > info.maxchain)
        info.maxchain = chain;
    }
  }

  return info;
}

void lj_tab_getinfo(lua_State *L, const GCtab *t, struct tab_info *ti)
{
  memset(ti, 0, sizeof(*ti));

  ti->acapacity = t->asize;
  ti->asize = tab_asize(t);

  if (t->hmask > 0) {
    struct chains_info info = tab_nchains(L, t);

    ti->hcapacity = t->hmask + 1;
    ti->hsize = tab_hsize(t);
    ti->hnchains = info.nchains;
    ti->hmaxchain = info.maxchain;
  }
}

const TValue *lj_tab_rawrindex(lua_State *L, const TValue *base, size_t n)
{
  const TValue *tv = &base[0];
  size_t i;

  lua_assert(n > 1);
  lua_assert(tvistab(tv));

  for (i = 1; i < n; i++) {
    GCtab *t = tabV(tv);

    if (LJ_LIKELY(t->metatable == NULL || (t->metatable->nomm & (1u << MM_index)))) {
      tv = lj_tab_get(L, t, &base[i]);
      if (!tvistab(tv))
        break;
    } else {
      return NULL; /* fast path failed */
    }
  }

  return LJ_LIKELY(i >= n - 1) ? tv : niltv(L);
}

#if LJ_HASJIT
const TValue *lj_tab_rawrindex_jit(lua_State *L, const struct argbuf *ab)
{
  return lj_tab_rawrindex(L, ab->base, ab->n);
}
#endif /* LJ_HASJIT */
