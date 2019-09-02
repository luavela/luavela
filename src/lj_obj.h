/*
 * uJIT VM tags, values and objects.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#ifndef _LJ_OBJ_H
#define _LJ_OBJ_H

#include "lua.h"
#include "lj_def.h"
#include "uj_arch.h"
#include "lj_bcins.h"
#include "uj_funcid.h"
#include "uj_vmstate.h"
#include "uj_obj_marks.h"
#include "utils/fp.h"
#include "utils/strhash.h"
#include "uj_mem.h"
#include "profile/uj_iprof_iface.h"

/* Type predeclarations. */
typedef union GCobj GCobj;
typedef struct GCtab GCtab;
typedef struct CTState CTState; /* For FFI. */

/* Common GC header for all collectable objects. */
#define GCHeader        GCobj *nextgc; uint8_t marked; uint8_t gct
/* This occupies 10 bytes, so use the next 2 bytes for non-32 bit fields. */

#define gcnext(gcobj)   ((gcobj)->gch.nextgc)

/* IMPORTANT NOTE:
**
** All uses of the setgcref* macros MUST be accompanied with a write barrier.
**
** This is to ensure the integrity of the incremental GC. The invariant
** to preserve is that a black object never points to a white object.
** I.e. never store a white object into a field of a black object.
**
** It's ok to LEAVE OUT the write barrier ONLY in the following cases:
** - The source is not a GC object (NULL).
** - The target is a GC root. I.e. everything in global_State.
** - The target is a lua_State field (threads are never black).
** - The target is a stack slot, see setgcV et al.
** - The target is an open upvalue, i.e. pointing to a stack slot.
** - The target is a newly created object (i.e. marked white). But make
**   sure nothing invokes the GC inbetween.
** - The target and the source are the same object (self-reference).
** - The target already contains the object (e.g. moving elements around).
**
** The most common case is a store to a stack slot. All other cases where
** a barrier has been omitted are annotated with a NOBARRIER comment.
**
** The same logic applies for stores to table slots (array part or hash
** part). ALL uses of lj_tab_set* require a barrier for the stored value
** *and* the stored key, based on the above rules. In practice this means
** a barrier is needed if *either* of the key or value are a GC object.
**
** It's ok to LEAVE OUT the write barrier in the following special cases:
** - The stored value is nil. The key doesn't matter because it's either
**   not resurrected or lj_tab_newkey() will take care of the key barrier.
** - The key doesn't matter if the *previously* stored value is guaranteed
**   to be non-nil (because the key is kept alive in the table).
** - The key doesn't matter if it's guaranteed not to be part of the table,
**   since lj_tab_newkey() takes care of the key barrier. This applies
**   trivially to new tables, but watch out for resurrected keys. Storing
**   a nil value leaves the key in the table!
**
** In case of doubt use lj_gc_anybarriert() as it's rather cheap. It's used
** by the interpreter for all table stores.
**
** Note: In contrast to Lua's GC, LuaJIT's GC does *not* specially mark
** dead keys in tables. The reference is left in, but it's guaranteed to
** be never dereferenced as long as the value is nil. It's ok if the key is
** freed or if any object subsequently gets the same address.
**
** Not destroying dead keys helps to keep key hash slots stable. This avoids
** specialization back-off for HREFK when a value flips between nil and
** non-nil and the GC gets in the way. It also allows safely hoisting
** HREF/HREFK across GC steps. Dead keys are only removed if a table is
** resized (i.e. by NEWREF) and xREF must not be CSEd across a resize.
**
** The trade-off is that a write barrier for tables must take the key into
** account, too. Implicitly resurrecting the key by storing a non-nil value
** may invalidate the incremental GC invariant.
*/


/* -- Tags and values ----------------------------------------------------- */

/* Tagged value. */
typedef LJ_ALIGN(8) union TValue {
  /* Payload version. */
  struct {
    union {
      lua_Number n;      /* Numeric payload. */
      GCobj* gcr;        /* GC object payload. */
      void* lightud_ptr; /* Light userdata payload. */
    };
    /* Generic value tag for all values (including numbers).
    ** For numbers, canonical tag is used (unlike old implementation).
    */
    uint32_t value_tag;
    uint32_t padding; /* To be merged with value_tag. */
  };

  /* Layout version. */
  struct {
    union {
      uint64_t u64;
      struct {
        uint32_t lo;
        uint32_t hi;
      } u32;
    };
    uint64_t u64_hi;
  };

  /* Framelink version. */
  struct {
    GCobj* func;        /* Function for next frame (or dummy L). */
    union {
      int64_t ftsz;     /* Frame type and size of previous frame. */
      BCIns* pcr;       /* Bytecode RIP in case caller was a regular lua function. */
    } tp;
  } fr;
} TValue;

LJ_STATIC_ASSERT( (sizeof(TValue)%2)==0 );

#define LOG_SIZEOF_TVALUE 0x4
LJ_STATIC_ASSERT((1 << LOG_SIZEOF_TVALUE) == sizeof(TValue));

/* More external and GCobj tags for internal objects. */
#define LAST_TT         LUA_TTHREAD
#define LUA_TPROTO      (LAST_TT+1)
#define LUA_TCDATA      (LAST_TT+2)

/* Internal object tags.
**
** Internal tags overlap the MSW of a number object (must be a double).
** Interpreted as a double these are special NaNs. The FPU only generates
** one type of NaN (0xfff8_0000_0000_0000). So MSWs > 0xfff80000 are available
** for use as internal tags. Small negative numbers are used to shorten the
** encoding of type comparisons (reg/mem against sign-ext. 8 bit immediate).
**
**                  ---MSW---.---LSW---
** primitive types |  itype  |         |
** lightuserdata   |  itype  |  void * |  (32 bit platforms)
** lightuserdata   |ffff|    void *    |  (64 bit platforms, 47 bit pointers)
** GC objects      |  itype  |  GCRef  |
** number           -------double------
**
** ORDER LJ_T
** Primitive types nil/false/true must be first, lightuserdata next.
** GC objects are at the end, table/userdata must be lowest.
** Also check lj_ir.h for similar ordering constraints.
*/
#define LJ_TNIL                 (~0u)
#define LJ_TFALSE               (~1u)
#define LJ_TTRUE                (~2u)
#define LJ_TLIGHTUD             (~3u)
#define LJ_TSTR                 (~4u)
#define LJ_TUPVAL               (~5u)
#define LJ_TTHREAD              (~6u)
#define LJ_TPROTO               (~7u)
#define LJ_TFUNC                (~8u)
#define LJ_TTRACE               (~9u)
#define LJ_TCDATA               (~10u)
#define LJ_TTAB                 (~11u)
#define LJ_TUDATA               (~12u)
#define LJ_TNUMX                (~13u)

#define LJ_TISTRUECOND          LJ_TFALSE
#define LJ_TISPRI               LJ_TTRUE
#define LJ_TISGCV               (LJ_TSTR+1)
#define LJ_TISTABUD             LJ_TTAB

#define LJ_T__MIN               (LJ_TNUMX) /* Minimal valid tag value */

/* High part of control variable payload in ITERN loop */
#define LJ_ITERN_MARK           0xfffe7fff

/* -- String object ------------------------------------------------------- */

/* String object header. String payload follows. */
typedef struct GCstr {
  GCHeader;
  uint8_t reserved;     /* Used by lexer for fast lookup of reserved words. */
  uint8_t unused;
  uint32_t hash;       /* Hash of string. */
  size_t len;           /* Size of string. */
} GCstr;

#define strdata(s)      ((const char *)((s)+1))
#define strdatawr(s)    ((char *)((s)+1))
#define strVdata(o)     strdata(strV(o))

/* -- Userdata object ----------------------------------------------------- */

/* Userdata object. Payload follows. */
typedef struct GCudata {
  GCHeader;
  uint8_t udtype;       /* Userdata type. */
  uint8_t unused2;
  uint32_t len;         /* Size of payload. */
  GCtab *env;           /* Should be at same offset in GCfunc. */
  GCtab *metatable;     /* Must be at same offset in GCtab. */
} GCudata;

/* Userdata types. */
enum {
  UDTYPE_USERDATA,      /* Regular userdata. */
  UDTYPE_IO_FILE,       /* I/O library FILE. */
  UDTYPE_FFI_CLIB,      /* FFI C library namespace. */
  UDTYPE__MAX
};

#define uddata(u)       ((void *)((u)+1))

/* -- C data object ------------------------------------------------------- */

/* C data object. Payload follows. */
typedef struct GCcdata {
  GCHeader;
  uint16_t ctypeid;     /* C type ID. */
} GCcdata;

/* Prepended to variable-sized or realigned C data objects. */
typedef struct GCcdataVar {
  uint16_t offset;      /* Offset to allocated memory (relative to GCcdata). */
  uint16_t extra;       /* Extra space allocated (incl. GCcdata + GCcdatav). */
  size_t len;           /* Size of payload. */
} GCcdataVar;

#define cdataptr(cd)    ((void *)((cd)+1))
#define cdataisv(cd)    ((cd)->marked & LJ_GC_CDATA_VAR)
#define cdatav(cd)      ((GCcdataVar *)((char *)(cd) - sizeof(GCcdataVar)))
#define cdatavlen(cd)   lua_check(cdataisv(cd), cdatav(cd)->len)
#define sizecdatav(cd)  (cdatavlen(cd) + cdatav(cd)->extra)
#define memcdatav(cd)   ((void *)((char *)(cd) - cdatav(cd)->offset))

/* -- Prototype object ---------------------------------------------------- */

/* Memory layout of GCproto object (names are not syntactic):
** * proto_header: instance of struct GCproto
** * bytecode_ins: array of bytecode instructions
** * obj_const:    reversed array of prototype object constants
** * num_const:    array of prototype numeric constants
**                 (pointed at by proto_header.k)
** * upvalues:     array of prototype upvalues
**                 (pointed at by proto_header.uv)
**
** For more information on prototype layout see lj_parse.c
*/
typedef struct GCproto {
  GCHeader;
  uint8_t numparams;    /* Number of parameters. */
  uint8_t framesize;    /* Fixed frame size. */
  uint8_t sizeuv;       /* Number of upvalues. */
  uint8_t flags;        /* Miscellaneous flags (see below). */
  uint16_t trace;       /* Anchor for chain of root traces. */
  size_t sizebc;        /* Number of bytecode instructions. */
  void *k;              /* Split constant array (points to the middle). */
  GCobj* gclist;
  size_t sizekgc;       /* Number of collectable constants. */
  size_t sizekn;        /* Number of lua_Number constants. */
  size_t sizept;        /* Total size including colocated arrays. */
  GCstr *chunkname;     /* Name of the chunk this function was defined in. */
  BCLine firstline;     /* First line of the function definition. */
  BCLine numline;       /* Number of lines for the function definition. */
  void *lineinfo;       /* Compressed map from bytecode ins. to source line. */
  uint16_t *uv;         /* Upvalue list. local slot|0x8000 or parent uv idx. */
  uint8_t *uvinfo;      /* Upvalue names. */
  uint8_t *varinfo;     /* Names and compressed extents of local variables. */
#ifdef UJIT_PROFILER
  uint8_t profcount;
#endif // UJIT_PROFILER
} GCproto;

/* Flags for prototype. */
#define PROTO_CHILD             0x01    /* Has child prototypes. */
#define PROTO_VARARG            0x02    /* Vararg function. */
#define PROTO_FFI               0x04    /* Uses BC_KCDATA for FFI datatypes. */
#define PROTO_NOJIT             0x08    /* JIT disabled for this function. */
#define PROTO_ILOOP             0x10    /* Patched bytecode with ILOOP etc. */
/* Only used during parsing. */
#define PROTO_HAS_RETURN        0x20    /* Already emitted a return. */
#define PROTO_FIXUP_RETURN      0x40    /* Need to fixup emitted returns. */
/* Top bits used for counting created closures. */
#define PROTO_CLCOUNT           0x20    /* Base of saturating 3 bit counter. */
#define PROTO_CLC_BITS          3
#define PROTO_CLC_POLY          (3*PROTO_CLCOUNT)  /* Polymorphic threshold. */

#define PROTO_UV_LOCAL          0x8000  /* Upvalue for local slot. */
#define PROTO_UV_IMMUTABLE      0x4000  /* Immutable upvalue. */

#define proto_kgc(pt, idx) \
  lua_check((uintptr_t)(intptr_t)(idx) >= (uintptr_t)-(intptr_t)(pt)->sizekgc, \
            (((GCobj**)((pt)->k))[(idx)]) )
#define proto_knumtv(pt, idx) \
  lua_check((uintptr_t)(idx) < (pt)->sizekn, &((TValue*)((pt)->k))[(idx)])
#define proto_bc(pt)            ((BCIns *)((char *)(pt) + sizeof(GCproto)))
#define proto_bcpos(pt, pc)     ((BCPos)((pc) - proto_bc(pt)))
#define proto_uv(pt)            ((pt)->uv)

#define proto_chunkname(pt)     ((pt)->chunkname)
#define proto_chunknamestr(pt)  (strdata(proto_chunkname((pt))))
#define proto_lineinfo(pt)      ((pt)->lineinfo)
#define proto_uvinfo(pt)        ((pt)->uvinfo)
#define proto_varinfo(pt)       ((pt)->varinfo)

/* -- Upvalue object ------------------------------------------------------ */

typedef struct GCupval {
  GCHeader;
  uint8_t closed;       /* Set if closed (i.e. uv->v == &uv->u.value). */
  uint8_t immutable;    /* Immutable value. */
  uint32_t dhash;       /* Disambiguation hash: dh1 != dh2 => cannot alias. */
  union {
    TValue tv;          /* If closed: the value itself. */
    struct {            /* If open: double linked list, anchored at thread. */
      struct GCupval* prev;
      struct GCupval* next;
    };
  };
  TValue *v;            /* Points to stack slot (open) or above (closed). */
} GCupval;

#define uvprev(uv_)     ((uv_)->prev)
#define uvnext(uv_)     ((uv_)->next)
#define uvval(uv_)      ((uv_)->v)

/* -- Function object (closures) ------------------------------------------ */

/* Common header for functions. env should be at same offset in GCudata. */
#define GCfuncHeader \
  GCHeader; uint8_t ffid; uint8_t nupvalues; \
  GCtab *env; BCIns* pc; GCobj *gclist

typedef struct GCfuncC {
  GCfuncHeader;
  lua_CFunction f;      /* C function to be called. */
  TValue upvalue[1];    /* Array of upvalues (TValue). */
} GCfuncC;

typedef struct GCfuncL {
  GCfuncHeader;
  GCupval *uvptr[1];  /* Array of _pointers_ to upvalue objects. */
} GCfuncL;

typedef union GCfunc {
  GCfuncC c;
  GCfuncL l;
} GCfunc;

#define isluafunc(fn)   ((fn)->c.ffid == FF_LUA)
#define iscfunc(fn)     ((fn)->c.ffid == FF_C)
#define isffunc(fn)     ((fn)->c.ffid > FF_C)
#define funcproto(fn) \
  lua_check(isluafunc(fn), (GCproto *)(((char*)((fn)->l.pc))-sizeof(GCproto)))
/* -- Table object -------------------------------------------------------- */

/* Hash node. */
typedef struct Node {
  TValue val;           /* Value object. Must be first field. */
  TValue key;           /* Key object. */
  struct Node* next;    /* Hash chain. */
} Node;

LJ_STATIC_ASSERT(offsetof(Node, val) == 0);

struct GCtab {
  GCHeader;
  uint8_t  nomm;       /* Negative cache for fast metamethods. */
  int8_t   colo;       /* Array colocation. */
  uint32_t unused;     /* Unused 32-bit padding. */
  TValue   *array;     /* Array part. */
  GCtab    *metatable; /* Must be at same offset in GCudata. */
  GCobj    *gclist;
  Node     *node;      /* Hash part. */
  size_t   asize;      /* Size of array part (keys [0, asize-1]). */
  size_t   hmask;      /* Hash part mask (size of hash part - 1). */
  Node     *freetop;   /* Top of free elements. */
};

#define sizetabcolo(n)  ((n)*sizeof(TValue) + sizeof(GCtab))

/* Metamethods. ORDER MM */
#ifdef LJ_HASFFI
#define MMDEF_FFI(_) \
  _(new, 0)
#else
#define MMDEF_FFI(_)
#endif

#if LJ_52 || LJ_HASFFI
#define MMDEF_PAIRS(_) \
  _(pairs,  1) \
  _(ipairs, 1)
#else
#define MMDEF_PAIRS(_)
#define MM_pairs        255
#define MM_ipairs       255
#endif

#define MMDEF(_) \
  _(index,     2) \
  _(newindex,  3) \
  _(gc,        1) \
  _(mode,      0) /* mode is actually a string. */ \
  _(eq,        2) \
  /* According to Lua 5.1 documentation, __len accepts 1 argument. But due to
   * implementation details, in "real world" there are 2 arguments passed,
   * see http://lua-users.org/lists/lua-l/2010-01/msg00160.html */ \
  _(len,       2) \
  /* Only the above (fast) metamethods are negative cached (max. 8). */ \
  _(lt,        2) \
  _(le,        2) \
  _(concat,    2) \
  _(call,      1) /* 1 fixarg (callee) + varargs (optionally). */ \
  /* The following must be in ORDER ARITH. */ \
  _(add,       2) \
  _(sub,       2) \
  _(mul,       2) \
  _(div,       2) \
  _(mod,       2) \
  _(pow,       2) \
  /* Similar to __len: actual number of arguments is 2 due to implementation
   * details, see http://lua-users.org/lists/lua-l/2007-07/msg00587.html */ \
  _(unm,       2) \
  /* The following are used in the standard libraries. */ \
  _(metatable, 0) /* 0 since it's not invokable as a metamethod. */ \
  _(tostring,  1) \
  MMDEF_FFI(_)    \
  MMDEF_PAIRS(_)

enum MMS {
#define MMENUM(name, narg)    MM_##name,
MMDEF(MMENUM)
#undef MMENUM
  MM__MAX,
  MM____ = MM__MAX,
  MM_FAST = MM_len
};

/* GC root IDs. */
typedef enum {
  GCROOT_MMNAME,        /* Metamethod names. */
  GCROOT_MMNAME_LAST = GCROOT_MMNAME + MM__MAX-1,
  GCROOT_BASEMT,        /* Metatables for base types. */
  GCROOT_BASEMT_NUM = GCROOT_BASEMT + ~LJ_TNUMX,
  GCROOT_IO_INPUT,      /* Userdata for default I/O input file. */
  GCROOT_IO_OUTPUT,     /* Userdata for default I/O output file. */
  GCROOT_MAX
} GCRootID;

enum {
  GCSpause, GCSpropagate, GCSatomic, GCSsweepstring, GCSsweep, GCSfinalize, GCSlast
};

typedef struct GCState {
  size_t sealed;        /* Memory used by non-string sealed objects. */
  size_t threshold;     /* Memory threshold. */
  uint8_t currentwhite; /* Current white color. */
  uint8_t state;        /* GC state. */
  uint8_t nocdatafin;   /* No cdata finalizer called. */
  uint8_t unused2;
  size_t state_count[GCSlast]; /* Count of GC invocations with different states since previous call of luaE_metrics() */
  size_t sweepstr;      /* Sweep position in string table. */
  GCobj *root;          /* List of all collectable objects. */
  GCobj **sweep;        /* Sweep position in root list. */
  GCobj *gray;          /* List of gray objects. */
  GCobj *grayagain;     /* List of objects for atomic traversal. */
  GCobj *weak;          /* List of weak tables (to be cleared). */
  GCobj *mmudata;       /* List of userdata (to be finalized). */
  size_t stepmul;       /* Incremental GC step granularity. */
  size_t debt;          /* Debt (how much GC is behind schedule). */
  size_t estimate;      /* Estimate of memory actually in use. */
  size_t pause;         /* Pause between successive GC cycles. */

  size_t tabnum;        /* Number of tables in GC. */
  size_t udatanum;      /* Number of userdata objects in GC. */
} GCState;

typedef struct uj_strhash_t {
  GCobj **hash;  /* Hash table itself (array of hash chain anchors). */
  size_t  mask;  /* Hash table mask (size of hash table - 1). */
  size_t  count; /* Number of interned strings. */
} uj_strhash_t;

#define VM_SUFFIX_SIZE   7
#define VM_SUFFIX_LENGTH (VM_SUFFIX_SIZE - 1)

/* Buffer size should be enough to hold initial '.', terminating '\0'
** and at least one extra payload character.
*/
LJ_STATIC_ASSERT(VM_SUFFIX_SIZE >= 3);

struct uj_profile_topframe {
  uint8_t ffid;    /* FFUNC: fast function id. */
  union { /* PROFILER: Data describing top frame of the guest stack. */
    uint64_t raw;        /* Raw value for context save/restore. */
    TValue *interp_base; /* LFUNC: Base of the executed coroutine. */
    lua_CFunction cf;    /* CFUNC: Address of the C function. */
  } guesttop;
};

/* Dynamically resizeable buffer. */
struct sbuf {
  char *buf;    /* Pointer to the content. */
  size_t sz;    /* Content size. */
  size_t cap;   /* Buffer capacity. */
  lua_State *L; /* Pointer to lua_State that will allocate memory */
};

#if LJ_HASJIT
#define ARGBUF_MAX_SIZE 8

struct argbuf { /* auxiliary buffer for passing array of TValues around */
  size_t n; /* number of arguments */
  TValue *base; /* Pointer to an array of arguments */
};
#endif /* LJ_HASJIT */

/* Global state, shared by all threads of a Lua universe. */
typedef struct global_State {
  strhash_f hashf;
  uj_strhash_t strhash;        /* Main string hash table.   */
  uj_strhash_t strhash_sealed; /* Sealed string hash table. */
  /* Pointer to either strhash or strhash_sealed.
  ** Workaround for proper count decrease in strhash_sealed
  ** in lj_gc_freeall.
  */
  uj_strhash_t *strhash_sweep;
  size_t strhash_hit;  /* New string has been found in the storage */
  size_t strhash_miss; /* New string has been added to the storage */
  struct mem_manager mem; /* Memory allocator data. */
  GCState gc;           /* Garbage collector. */
  struct sbuf tmpbuf;   /* Temporary buffer for string concatenation. */
  Node nilnode;         /* Fallback 1-element hash part (nil key and value). */
  GCstr *strempty;      /* Pointer to an empty string, either to own or to the
                        ** one from DataState. */

  /* NB! Following two MUST be adjacent: */
  GCstr strempty_own;   /* Own instance of empty string. */
  uint8_t stremptyz;    /* Zero terminator of empty string. */

  GCobj *nullobj;       /* NULL GCobj pointer. */
  uint8_t hookmask;     /* Hook mask. */
  uint8_t dispatchmode; /* Dispatch mode. */
  lua_State *mainthref; /* Link to main thread. */
  TValue registrytv;    /* Anchor for registry. */
  TValue tmptv, tmptv2; /* Temporary TValues. */
  GCupval uvhead;       /* Head of double-linked list of all open upvalues. */
  int32_t hookcount;    /* Instruction hook countdown. */
  int32_t hookcstart;   /* Start count for instruction hook counter. */
  lua_Hook hookf;       /* Hook function. */
  lua_CFunction wrapf;  /* Wrapper for C function calls. */
  lua_CFunction panic;  /* Called as a last resort for errors. */
  volatile vmstate_t vmstate; /* VM state or current JIT code trace number. */
  BCIns bc_cfunc_int;   /* Bytecode for internal C function calls. */
  BCIns bc_cfunc_ext;   /* Bytecode for external C function calls. */
  lua_State *jit_L;     /* Current JIT code lua_State or NULL. */
  TValue *jit_base;     /* Current JIT code L->base. */
  lua_State *L_mem;     /* Currently allocating coroutine. */
  struct uj_profile_topframe top_frame; /* Holds info about currently executing function */
  CTState *ctype_state;              /* Pointer to C type state. */
  GCobj   *gcroot[GCROOT_MAX];       /* GC roots. */
  char     vmsuffix[VM_SUFFIX_SIZE]; /* VM-specific suffix for matching debug data. */
  lua_State *datastate; /* Pointer to the DataState or NULL. */
  GCtab* dataroot;      /* Pointer to the root of data (if DataState) or NULL. */

#if LJ_HASJIT
  struct argbuf *argbuf;
  struct argbuf argbuf_head;
  TValue argbuf_slots[ARGBUF_MAX_SIZE];
#endif /* LJ_HASJIT */

#ifdef UJIT_PROFILER
  uint8_t profcount;
#endif // UJIT_PROFILER
#ifdef UJIT_COVERAGE
  struct coverage *coverage;
#endif /* UJIT_COVERAGE */
#ifdef UJIT_IPROF_ENABLED
  GCstr *iprof_keys[IPROF_KEY_MAX];
#endif /* UJIT_IPROF_ENABLED */
  int enable_itern;     /* Enables ISNEXT/ITERN generation in frontend */
} global_State;

static LJ_AINLINE lua_State* gl_datastate(global_State *g) {
  return g->datastate;
}

static LJ_AINLINE uj_strhash_t* gl_strhash(global_State *g) {
  return &g->strhash;
}

static LJ_AINLINE uj_strhash_t* gl_strhash_sealed(global_State *g) {
  return &g->strhash_sealed;
}

/* Garbage collector states. Order matters. */
#define mainthread(g)   ((g)->mainthref)
#define niltv(L) \
  lua_check(tvisnil(&G(L)->nilnode.val), &G(L)->nilnode.val)
#define niltvg(g) \
  lua_check(tvisnil(&(g)->nilnode.val), &(g)->nilnode.val)

/* Hook management. Hook event masks are defined in lua.h. */
#define HOOK_EVENTMASK          0x0f
#define HOOK_ACTIVE             0x10
#define HOOK_ACTIVE_SHIFT       4
#define HOOK_GC                 0x40
#define hook_active(g)          ((g)->hookmask & HOOK_ACTIVE)
#define hook_enter(g)           ((g)->hookmask |= HOOK_ACTIVE)
#define hook_entergc(g)         ((g)->hookmask |= (HOOK_ACTIVE|HOOK_GC))
#define hook_leave(g)           ((g)->hookmask &= ~HOOK_ACTIVE)
#define hook_save(g)            ((g)->hookmask & ~HOOK_EVENTMASK)
#define hook_restore(g, h) \
  ((g)->hookmask = ((g)->hookmask & HOOK_EVENTMASK) | (h))

struct coro_timeout {
  uint64_t  usec;          /* Microseconds before LUAE_TIMEOUT (0 = no timeout). */
  uint64_t  expticks;      /* Timeout threshold as an absolute number of ticks. */
  size_t    nres;          /* Number of results returned by timeoutf. */
  lua_CFunction callback;  /* Function to execute on LUAE_TIMEOUT. */
};

/* Per-thread state object. */
struct lua_State {
  GCHeader;
  uint8_t dummy_ffid;      /* Fake FF_C for curr_funcisL() on dummy frames. */
  uint8_t status;          /* Thread status. */
  uint16_t events;         /* External events in the threads's context. */
  uint16_t unused1;
  uint32_t unused2;
  global_State *glref;     /* Link to global state. */
  GCobj        *gclist;    /* GC chain. */
  TValue       *base;      /* Base of currently executing function. */
  TValue       *top;       /* First free slot in the stack. */
  TValue       *maxstack;  /* Last free slot in the stack. */
  TValue       *stack;     /* Stack base. */
  GCobj        *openupval; /* List of open upvalues in the stack. */
  GCtab        *env;       /* Thread environment (table of globals). */
  void *cframe;            /* End of C stack frame chain. */
  size_t stacksize;        /* True stack size (incl. LJ_STACK_EXTRA). */
  struct coro_timeout timeout;
#ifdef UJIT_IPROF_ENABLED
  struct iprof *iprof;
#endif /* UJIT_IPROF_ENABLED */
};

#define G(L)                    ((L)->glref)
#define registry(L)             (&G(L)->registrytv)

static LJ_AINLINE struct mem_manager *MEM(const lua_State *L) {
  return &(L->glref->mem);
}

static LJ_AINLINE struct mem_manager *MEM_G(global_State *g) {
  return &(g->mem);
}

/* Macros to access the currently executing (Lua) function.
** NB! Despite its name, curr_func returns not exactly functional object,
** but "something located at (base - 1) on coroutine stack" which is
** almost always a functional object, but on rare occasions can be a lua_State.
** That's why no object type assertions here. See lj_frame.h for details.
*/
#define curr_func(L)            (&(((L)->base - 1)->fr.func)->fn)
#define curr_funcisL(L)         (isluafunc(curr_func(L)))
#define curr_proto(L)           (funcproto(curr_func(L)))
#define curr_topL(L)            ((L)->base + curr_proto(L)->framesize)
#define curr_top(L)             (curr_funcisL(L) ? curr_topL(L) : (L)->top)

/* -- GC object definition and conversions -------------------------------- */

/* GC header for generic access to common fields of GC objects. */
typedef struct GChead {
  GCHeader;
  uint8_t unused1;
  uint8_t unused2;
  uint32_t padding;
  GCtab *env;
  GCtab *metatable;
  GCobj *gclist;
} GChead;

/* The env field SHOULD be at the same offset for all GC objects. */
LJ_STATIC_ASSERT(offsetof(GChead, env) == offsetof(GCfuncL, env));
LJ_STATIC_ASSERT(offsetof(GChead, env) == offsetof(GCudata, env));

/* The metatable field MUST be at the same offset for all GC objects. */
LJ_STATIC_ASSERT(offsetof(GChead, metatable) == offsetof(GCtab, metatable));
LJ_STATIC_ASSERT(offsetof(GChead, metatable) == offsetof(GCudata, metatable));

/* The gclist field MUST be at the same offset for all GC objects. */
LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(lua_State, gclist));
LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCproto, gclist));
LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCfuncL, gclist));
LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCtab, gclist));

/* dummy_ffid and ffid MUST be at the same offset for curr_funcisL() */
LJ_STATIC_ASSERT(offsetof(lua_State, dummy_ffid) == offsetof(GCfuncL, ffid));
LJ_STATIC_ASSERT(offsetof(lua_State, dummy_ffid) == offsetof(GCfuncC, ffid));

union GCobj {
  GChead gch;
  GCstr str;
  GCupval uv;
  lua_State th;
  GCproto pt;
  GCfunc fn;
  GCcdata cd;
  GCtab tab;
  GCudata ud;
};

/* Returns a non-0 value if the object is sealed and 0 otherwise. */
static LJ_AINLINE int uj_obj_is_sealed(const GCobj *o)
{
  return o->gch.gct != ~LJ_TCDATA && (o->gch.marked & UJ_GCO_SEALED);
}

/* Returns a non-0 value if the object is immutable and 0 otherwise. */
static LJ_AINLINE int uj_obj_is_immutable(const GCobj *o)
{
  return (o->gch.marked & UJ_GCO_IMMUTABLE);
}

/* Macros to convert a GCobj pointer into a specific value. */
#define gco2str(o)      lua_check((o)->gch.gct == ~LJ_TSTR, &(o)->str)
#define gco2uv(o)       lua_check((o)->gch.gct == ~LJ_TUPVAL, &(o)->uv)
#define gco2th(o)       lua_check((o)->gch.gct == ~LJ_TTHREAD, &(o)->th)
#define gco2pt(o)       lua_check((o)->gch.gct == ~LJ_TPROTO, &(o)->pt)
#define gco2func(o)     lua_check((o)->gch.gct == ~LJ_TFUNC, &(o)->fn)
#define gco2cd(o)       lua_check((o)->gch.gct == ~LJ_TCDATA, &(o)->cd)
#define gco2tab(o)      lua_check((o)->gch.gct == ~LJ_TTAB, &(o)->tab)
#define gco2ud(o)       lua_check((o)->gch.gct == ~LJ_TUDATA, &(o)->ud)

/* Macro to convert any collectable object into a GCobj pointer. */
#define obj2gco(v)      ((GCobj *)(v))

/* -- TValue type checks -------------------------------------------------- */

#define gettag(o)       ((o)->value_tag)
#define tagisvalid(o)   (gettag(o) >= LJ_T__MIN)

#define tvisnil(o)      (gettag(o) == LJ_TNIL)
#define tvisfalse(o)    (gettag(o) == LJ_TFALSE)
#define tvistrue(o)     (gettag(o) == LJ_TTRUE)

#define tvisbool(o)     (tvisfalse(o) || tvistrue(o))
#define tvislibiofile(o) (tvisudata((o)) && udataV((o))->udtype == UDTYPE_IO_FILE)

#define tvislightud(o)  (gettag(o) == LJ_TLIGHTUD)
#define tvisstr(o)      (gettag(o) == LJ_TSTR)
#define tvisfunc(o)     (gettag(o) == LJ_TFUNC)
#define tvisthread(o)   (gettag(o) == LJ_TTHREAD)
#define tvisproto(o)    (gettag(o) == LJ_TPROTO)
#define tviscdata(o)    (gettag(o) == LJ_TCDATA)
#define tvistab(o)      (gettag(o) == LJ_TTAB)
#define tvisudata(o)    (gettag(o) == LJ_TUDATA)
#define tvisnum(o)      (gettag(o) == LJ_TNUMX)

#define tvistruecond(o) (gettag(o) < LJ_TISTRUECOND)
#define tvispri(o)      (gettag(o) >= LJ_TISPRI)
#define tvistabud(o)    (gettag(o) <= LJ_TISTABUD)  /* && !tvisnum() */
#define tvisgcv(o)      ((gettag(o) - LJ_TISGCV) > (LJ_TNUMX - LJ_TISGCV))

/* -- TValue getters ------------------------------------------------------ */

#ifndef NDEBUG
#include "lj_gc.h"
#endif /* !NDEBUG */

/* Macros to get tagged values. */
#define gcval(o)        ((o)->gcr)
#define boolV(o)        lua_check(tvisbool(o), (LJ_TFALSE - (o)->value_tag))
#define lightudV(o)     lua_check(tvislightud(o), (o)->lightud_ptr)
#define gcV(o)          lua_check(tvisgcv(o), gcval(o))
#define strV(o)         lua_check(tvisstr(o), &gcval(o)->str)
#define funcV(o)        lua_check(tvisfunc(o), &gcval(o)->fn)
#define threadV(o)      lua_check(tvisthread(o), &gcval(o)->th)
#define protoV(o)       lua_check(tvisproto(o), &gcval(o)->pt)
#define cdataV(o)       lua_check(tviscdata(o), &gcval(o)->cd)
#define tabV(o)         lua_check(tvistab(o), &gcval(o)->tab)
#define udataV(o)       lua_check(tvisudata(o), &gcval(o)->ud)
#define numV(o)         lua_check(tvisnum(o), (o)->n)
#define rawV(o)         ((o)->u64)

/* Special macros to test special forms of numbers. */
#define tvisint(o)      (tvisnum(o) && numV(o) == (lua_Number)lj_num2int(numV(o)))
#define tvisnan(o)      (numV(o) != numV(o))
#define tviszero(o)     ((rawV(o) << 1) == 0)
#define tvispzero(o)    (rawV(o) == 0)
#define tvismzero(o)    (rawV(o) == U64x(80000000,00000000))
#define tvispone(o)     (rawV(o) == U64x(3ff00000,00000000))
#define rawnumequal(o1, o2)     (rawV(o1) == rawV(o2))

/* -- TValue setters ------------------------------------------------------ */

#define settag(o, i)            ((o)->value_tag = (i))

static LJ_AINLINE void setnilV(TValue *o)
{
  settag(o, LJ_TNIL);
}

static LJ_AINLINE void setboolV(TValue *o, uint32_t x)
{
  settag(o, LJ_TFALSE - x);
}

static LJ_AINLINE void setlightudV(TValue *o, void *p)
{
  o->lightud_ptr = p;
  settag(o, LJ_TLIGHTUD);
}

/* Regarding 'NULL == L' condition. There are some places (for example, expr_kvalue)
** where we use TValues without particular L/G/GC context. In this case, L equals to
** NULL and the isdead check is not needed.
*/
#define tvchecklive(L, o) \
  UNUSED(L), lua_assert(!tvisgcv(o) || uj_obj_is_sealed(gcval(o)) || \
  (gcval(o)->gch.marked & LJ_GC_FIXED) || \
  ((~gettag(o) == gcval(o)->gch.gct) && (NULL == (L) || !isdead(G(L), gcval(o)))))

static LJ_AINLINE void setgcV(lua_State *L, TValue *o, GCobj *v, uint32_t itype) {
  o->gcr = v;
  settag(o, itype);
  tvchecklive(L, o);
}

static LJ_AINLINE void setstrV(lua_State *L, TValue *tv, GCstr *v) {
  setgcV(L, tv, obj2gco(v), LJ_TSTR);
}

static LJ_AINLINE void setprotoV(lua_State *L, TValue *tv, GCproto *v) {
  setgcV(L, tv, obj2gco(v), LJ_TPROTO);
}

static LJ_AINLINE void setfuncV(lua_State *L, TValue *tv, GCfunc *v) {
  setgcV(L, tv, obj2gco(v), LJ_TFUNC);
}

static LJ_AINLINE void setcdataV(lua_State *L, TValue *tv, GCcdata *v) {
  setgcV(L, tv, obj2gco(v), LJ_TCDATA);
}

static LJ_AINLINE void settabV(lua_State *L, TValue *tv, GCtab *v) {
  setgcV(L, tv, obj2gco(v), LJ_TTAB);
}

static LJ_AINLINE void setudataV(lua_State *L, TValue *tv, GCudata *v) {
  setgcV(L, tv, obj2gco(v), LJ_TUDATA);
}

static LJ_AINLINE void setthreadV(lua_State *L, TValue *tv, lua_State *v) {
  setgcV(L, tv, obj2gco(v), LJ_TTHREAD);
}

static LJ_AINLINE void setnumV(TValue *o, double x)
{
  settag(o, LJ_TNUMX); o->n = x;
}

static LJ_AINLINE void setrawV(TValue *o, uint64_t x)
{
  settag(o, LJ_TNUMX); o->u64 = x;
}

#define setnanV(o)  setrawV(o, LJ_NAN)
#define setpinfV(o) setrawV(o, LJ_PINFINITY)
#define setminfV(o) setrawV(o, LJ_MINFINITY)

static LJ_AINLINE void setintV(TValue *o, lua_Integer i)
{
  setnumV(o, (lua_Number)i);
}

#define setintptrV(o, i)        setintV((o), (i))

/* Copy tagged values. */
static LJ_AINLINE void copyTV(lua_State *L, TValue *o1, const TValue *o2)
{
  *o1 = *o2; tvchecklive(L, o1);
}

/* -- Miscellaneous object handling --------------------------------------- */

/* Names and maps for internal and external object tags. */
LJ_DATA const char *const uj_obj_typename[1+LUA_TCDATA+1];
LJ_DATA const char *const uj_obj_itypename[~LJ_TNUMX+1];

#define lj_typename(o)  (uj_obj_itypename[~gettag(o)])

/* Allocate size bytes in the context of L for a collectable object. */
void *uj_obj_new(lua_State *L, size_t size);

/* Compare two objects without calling metamethods. */
int uj_obj_equal(const TValue *o1, const TValue *o2);

struct deepcopy_ctx {
  GCtab *map; /* mapping from source table addresses to copied ones */
  GCtab *env; /* dummy env for copied Lua functions */
};

/*
 * Deep copy object from possibly another global state.
 * Works for limited types of objects, see implementation.
 */
GCobj *uj_obj_deepcopy(lua_State *L, GCobj *obj, struct deepcopy_ctx *ctx);

/* Clear marks after deep copy */
void uj_obj_clear_mark(lua_State *L, GCobj *o);

/*
 * A set of interfaces that generalizes the operation of applying some property
 * to an object.
 */

/* A function that flips (sets or clears) a mark on an object. */
typedef void (*gco_mark_flipper) (lua_State *L, GCobj *o);

/*
 * A function that traverses an object in accordance with some semantics
 * and sets the correct value of the mark with the given flipper.
 */
typedef void (*gco_traverser) (lua_State *L, GCobj *o,
                               gco_mark_flipper flipper);

static LJ_AINLINE int lj_obj_has_mark(const GCobj *o) {
  return (o->gch.marked & UJ_GCO_TMPMARK);
}

static LJ_AINLINE void lj_obj_set_mark(GCobj *o) {
  o->gch.marked |= UJ_GCO_TMPMARK;
}

static LJ_AINLINE void lj_obj_clear_mark(GCobj *o) {
  o->gch.marked &= (~UJ_GCO_TMPMARK);
}

/*
 * Protected marking of an object: Calls marker in protected mode, and if it
 * throws, calls rubber to restore the original state of the object.
 * Marker may throw. If marker never throws, rubber may be NULL. Otherwise
 * rubber must not be NULL and must not throw.
 */
void uj_obj_pmark(lua_State *L, GCobj *o,
                  gco_mark_flipper marker,
                  gco_mark_flipper rubber);

/*
 * Propagates a "set mark" semantics on the object:
 * Calls traverser to inspect the object and apply marker.
 */
void uj_obj_propagate_set(lua_State *L, GCobj *o,
                          gco_traverser traverser,
                          gco_mark_flipper marker);

/*
 * Propagates a "clear mark" semantics on the object:
 * Calls traverser to inspect the object and apply rubber.
 */
void uj_obj_propagate_clear(lua_State *L, GCobj *o,
                            gco_traverser traverser,
                            gco_mark_flipper rubber);

#endif
