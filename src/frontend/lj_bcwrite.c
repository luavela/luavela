/*
 * Bytecode writer.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "uj_sbuf.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#endif /* LJ_HASFFI */
#if LJ_HASJIT
#include "uj_dispatch.h"
#include "jit/lj_jit.h"
#endif /* LJ_HASJIT */
#include "lj_bcdump.h"
#include "lj_vm.h"
#include "utils/leb128.h"

/* Context for bytecode writer. */
typedef struct BCWriteCtx {
  struct sbuf sb;               /* Output buffer. */
  lua_State *L;                 /* Lua state. */
  GCproto *pt;                  /* Root prototype. */
  lua_Writer wfunc;             /* Writer callback. */
  void *wdata;                  /* Writer callback data. */
  int strip;                    /* Strip debug info. */
  int status;                   /* Status from writer callback. */
} BCWriteCtx;

/* -- Bytecode writer ----------------------------------------------------- */

/* Write a single constant key/value of a template table. */
static void bcwrite_ktabk(BCWriteCtx *ctx, const TValue *o)
{
  struct sbuf *sb = &ctx->sb;
  if (tvisstr(o)) {
    const GCstr *str = strV(o);
    uj_sbuf_push_uleb128(sb, BCDUMP_KTAB_STR + str->len);
    uj_sbuf_push_str(sb, str);
  } else if (tvisnum(o)) {
    uj_sbuf_push_char(sb, BCDUMP_KTAB_NUM);
    uj_sbuf_push_uleb128(sb, rawV(o));
  } else {
    lua_assert(tvispri(o));
    uj_sbuf_push_char(sb, BCDUMP_KTAB_NIL+~gettag(o));
  }
}

/* Write a template table. */
static void bcwrite_ktab(BCWriteCtx *ctx, const GCtab *t)
{
  struct sbuf *sb = &ctx->sb;
  size_t narray = 0, nhash = 0;
  if (t->asize > 0) {  /* Determine max. length of array part. */
    ptrdiff_t i;
    TValue *array = t->array;
    for (i = (ptrdiff_t)t->asize-1; i >= 0; i--)
      if (!tvisnil(&array[i]))
        break;
    narray = (size_t)(i+1);
  }
  if (t->hmask > 0) {  /* Count number of used hash slots. */
    size_t i, hmask = t->hmask;
    Node *node = t->node;
    for (i = 0; i <= hmask; i++)
      nhash += !tvisnil(&node[i].val);
  }
  /* Write number of array slots and hash slots. */
  uj_sbuf_push_uleb128(sb, narray);
  uj_sbuf_push_uleb128(sb, nhash);
  if (narray) {  /* Write array entries (may contain nil). */
    size_t i;
    TValue *o = t->array;
    for (i = 0; i < narray; i++, o++)
      bcwrite_ktabk(ctx, o);
  }
  if (nhash) {  /* Write hash entries. */
    size_t i = nhash;
    Node *node = t->node + t->hmask;
    for (;; node--)
      if (!tvisnil(&node->val)) {
        bcwrite_ktabk(ctx, &node->key);
        bcwrite_ktabk(ctx, &node->val);
        if (--i == 0) break;
      }
  }
}

/* Write GC constants of a prototype. */
static void bcwrite_kgc(BCWriteCtx *ctx, GCproto *pt)
{
  struct sbuf *sb = &ctx->sb;
  size_t i, sizekgc = pt->sizekgc;
  GCobj **kr = (GCobj**)pt->k - (ptrdiff_t)sizekgc;
  for (i = 0; i < sizekgc; i++, kr++) {
    GCobj *o = *kr;
    size_t tp;
    /* Determine constant type and needed size. */
    if (o->gch.gct == ~LJ_TSTR) {
      tp = BCDUMP_KGC_STR + gco2str(o)->len;
    } else if (o->gch.gct == ~LJ_TPROTO) {
      lua_assert((pt->flags & PROTO_CHILD));
      tp = BCDUMP_KGC_CHILD;
#if LJ_HASFFI
    } else if (o->gch.gct == ~LJ_TCDATA) {
      CTypeID id = gco2cd(o)->ctypeid;
      if (id == CTID_INT64) {
        tp = BCDUMP_KGC_I64;
      } else if (id == CTID_UINT64) {
        tp = BCDUMP_KGC_U64;
      } else {
        lua_assert(id == CTID_COMPLEX_DOUBLE);
        tp = BCDUMP_KGC_COMPLEX;
      }
#endif /* LJ_HASFFI */
    } else {
      lua_assert(o->gch.gct == ~LJ_TTAB);
      tp = BCDUMP_KGC_TAB;
    }
    /* Write constant type. */
    uj_sbuf_push_uleb128(sb, tp);
    /* Write constant data (if any). */
    if (tp >= BCDUMP_KGC_STR) {
      uj_sbuf_push_str(sb, gco2str(o));
    } else if (tp == BCDUMP_KGC_TAB) {
      bcwrite_ktab(ctx, gco2tab(o));
#if LJ_HASFFI
    } else if (tp != BCDUMP_KGC_CHILD) {
      const TValue *p = (TValue *)cdataptr(gco2cd(o));
      uj_sbuf_push_uleb128(sb, rawV(&p[0]));
      if (tp == BCDUMP_KGC_COMPLEX) {
        uj_sbuf_push_uleb128(sb, rawV(&p[1]));
      }
#endif /* LJ_HASFFI */
    }
  }
}

/* Write number constants of a prototype. */
static void bcwrite_knum(BCWriteCtx *ctx, GCproto *pt)
{
  struct sbuf *sb = &ctx->sb;
  size_t i, sizekn = pt->sizekn;
  const TValue *o = (TValue*)pt->k;
  for (i = 0; i < sizekn; i++, o++) {
      uj_sbuf_push_uleb128(sb, rawV(o));
  }
}

/* Zeroing counter and flag fields for HOTCNT in dump */
static void bcwrite_zero_hotcnt(BCIns *bc, size_t nbc)
{
  BCIns *end = bc + nbc;

  while (bc != end) {
    if (bc_op(*bc) == BC_HOTCNT)
      *(BCIns *)bc = (BCIns)BC_HOTCNT;
    bc++;
  }
}

/* Write bytecode instructions. */
static void bcwrite_bytecode(BCWriteCtx *ctx, GCproto *pt)
{
  struct sbuf *sb = &ctx->sb;
  lua_State *L = ctx->L;
  size_t nbc = pt->sizebc-1;  /* Omit the [JI]FUNC* header. */
  size_t bc_pos = uj_sbuf_size(sb);
#if LJ_HASJIT
  size_t n = uj_sbuf_size(sb);
#endif /* LJ_HASJIT */

  uj_sbuf_push_block(sb, proto_bc(pt) + 1, nbc * sizeof(BCIns));
  bcwrite_zero_hotcnt((BCIns *)uj_sbuf_at(sb, bc_pos), nbc);
#if LJ_HASJIT
  uint8_t *p = (uint8_t *)uj_sbuf_at(sb, n);

  /* Unpatch modified bytecode containing ILOOP/JLOOP etc. */
  if ((pt->flags & PROTO_ILOOP) || pt->trace) {
    jit_State *J = L2J(L);
    size_t i;
    for (i = 0; i < nbc; i++, p += sizeof(BCIns)) {
      BCOp op = (BCOp)p[0];
      if (bc_isiloop(op) || op == BC_JFORI) {
        p[0] = (uint8_t)(op-BC_IFORL+BC_FORL);
      } else if (bc_isjloop(op)) {
        BCReg rd = p[2] + (p[3] << 8);
        BCIns ins = traceref(J, rd)->startins;
        p[0] = (uint8_t)(op-BC_JFORL+BC_FORL);
        p[2] = bc_c(ins);
        p[3] = bc_b(ins);
      }
    }
  }
#else /* !LJ_HASJIT */
  UNUSED(L);
#endif /* LJ_HASJIT */
}

/* Write prototype. */
static void bcwrite_proto(BCWriteCtx *ctx, GCproto *pt)
{
  struct sbuf *sb = &ctx->sb;
  lua_State *L = ctx->L;
  size_t sizedbg = 0;
  char proto_length[LEB128_U64_MAXSIZE];

  /* Recursively write children of prototype. */
  if ((pt->flags & PROTO_CHILD)) {
    ptrdiff_t i, n = pt->sizekgc;
    GCobj **kr = (GCobj**)pt->k - 1;
    for (i = 0; i < n; i++, kr--) {
      GCobj *o = *kr;
      if (o->gch.gct == ~LJ_TPROTO)
        bcwrite_proto(ctx, gco2pt(o));
    }
  }

  /* Start writing the prototype info to a buffer. */
  uj_sbuf_reset(sb);

  /* Write prototype header. */
  uj_sbuf_push_char(sb, (pt->flags & (PROTO_CHILD|PROTO_VARARG|PROTO_FFI)));
  uj_sbuf_push_char(sb, pt->numparams);
  uj_sbuf_push_char(sb, pt->framesize);
  uj_sbuf_push_char(sb, pt->sizeuv);
  uj_sbuf_push_uleb128(sb, pt->sizekgc);
  uj_sbuf_push_uleb128(sb, pt->sizekn);
  uj_sbuf_push_uleb128(sb, pt->sizebc-1);
  if (!ctx->strip) {
    if (proto_lineinfo(pt))
      sizedbg = pt->sizept - (size_t)((char *)proto_lineinfo(pt) - (char *)pt);
    uj_sbuf_push_uleb128(sb, sizedbg);
    if (sizedbg) {
      uj_sbuf_push_uleb128(sb, pt->firstline);
      uj_sbuf_push_uleb128(sb, pt->numline);
    }
  }

  /* Write bytecode instructions and upvalue refs. */
  bcwrite_bytecode(ctx, pt);
  uj_sbuf_push_block(sb, proto_uv(pt), pt->sizeuv * 2);

  /* Write constants. */
  bcwrite_kgc(ctx, pt);
  bcwrite_knum(ctx, pt);

  /* Write debug info, if not stripped. */
  if (sizedbg) {
    uj_sbuf_push_block(sb, proto_lineinfo(pt), sizedbg);
  }

  /* Pass serialized prototype length to writer function. */
  if (ctx->status == 0) {
    size_t proto_length_size = write_uleb128((uint8_t *)proto_length, uj_sbuf_size(sb));
    ctx->status = ctx->wfunc(L, proto_length, proto_length_size, ctx->wdata);
  }

  /* Pass seralized prototype itself to writer function. */
  if (ctx->status == 0) {
    ctx->status = ctx->wfunc(L, uj_sbuf_front(sb), uj_sbuf_size(sb), ctx->wdata);
  }
}

/* Write header of bytecode dump. */
static void bcwrite_header(BCWriteCtx *ctx)
{
  struct sbuf *sb = &ctx->sb;
  lua_State *L = ctx->L;
  GCstr *chunkname = proto_chunkname(ctx->pt);
  uj_sbuf_reset(sb);
  uj_sbuf_push_char(sb, BCDUMP_HEAD1);
  uj_sbuf_push_char(sb, BCDUMP_HEAD2);
  uj_sbuf_push_char(sb, BCDUMP_HEAD3);
  uj_sbuf_push_char(sb, BCDUMP_VERSION);
  uj_sbuf_push_char(sb, (ctx->strip ? BCDUMP_F_STRIP : 0) +
             ((ctx->pt->flags & PROTO_FFI) ? BCDUMP_F_FFI : 0));
  if (!ctx->strip) {
    uj_sbuf_push_uleb128(sb, chunkname->len);
    uj_sbuf_push_str(sb, chunkname);
  }
  ctx->status = ctx->wfunc(L, uj_sbuf_front(sb), uj_sbuf_size(sb), ctx->wdata);
}

/* Write footer of bytecode dump. */
static void bcwrite_footer(BCWriteCtx *ctx)
{
  if (ctx->status == 0) {
    uint8_t zero = 0;
    ctx->status = ctx->wfunc(ctx->L, &zero, 1, ctx->wdata);
  }
}

/* Protected callback for bytecode writer. */
static TValue *cpwriter(lua_State *L, lua_CFunction dummy, void *ud)
{
  BCWriteCtx *ctx = (BCWriteCtx *)ud;

  UNUSED(L);
  UNUSED(dummy);
  uj_sbuf_reserve(&ctx->sb, 1024);  /* Avoids resize for most prototypes. */
  bcwrite_header(ctx);
  bcwrite_proto(ctx, ctx->pt);
  bcwrite_footer(ctx);
  return NULL;
}

/* Write bytecode for a prototype. */
int lj_bcwrite(lua_State *L, GCproto *pt, lua_Writer writer, void *data,
               int strip)
{
  BCWriteCtx ctx;
  int status;
  ctx.L = L;
  ctx.pt = pt;
  ctx.wfunc = writer;
  ctx.wdata = data;
  ctx.strip = strip;
  ctx.status = 0;
  uj_sbuf_init(L, &ctx.sb);
  status = lj_vm_cpcall(L, NULL, &ctx, cpwriter);
  if (status == 0) status = ctx.status;
  uj_sbuf_free(ctx.L, &ctx.sb);
  return status;
}

