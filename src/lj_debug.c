/*
 * Debugging and introspection.
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lj_obj.h"
#include "lj_debug.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "uj_meta.h"
#include "uj_proto.h"
#include "uj_state.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_vm.h"
#if LJ_HASJIT
#include "jit/lj_jit.h"
#endif
#include "uj_cframe.h"

/* -- Frames -------------------------------------------------------------- */

/* Get frame corresponding to a level. */
const TValue *lj_debug_frame(lua_State *L, int level, int *size)
{
  const TValue *frame, *nextframe, *bot = L->stack;
  /* Traverse frames backwards. */
  for (nextframe = frame = L->base-1; frame > bot; ) {
    if (frame_isdummy(L, frame))
      level++;  /* Skip dummy frames. See uj_meta_call(). */
    if (level-- == 0) {
      *size = (int)(nextframe - frame);
      return frame;  /* Level found. */
    }
    nextframe = frame;
    if (frame_islua(frame)) {
      frame = frame_prevl(frame);
    } else {
      if (frame_isvarg(frame))
        level++;  /* Skip vararg pseudo-frame. */
      frame = frame_prevd(frame);
    }
  }
  *size = level;
  return NULL;  /* Level not found. */
}

/* Return bytecode position for function/frame or NO_BCPOS. */
BCPos lj_debug_framepc(lua_State *L, GCfunc *fn, const TValue *nextframe)
{
  const BCIns *ins;
  GCproto *pt;
  BCPos pos;
  if (!isluafunc(fn)) {  /* Cannot derive a PC for non-Lua functions. */
    return NO_BCPOS;
  } else if (nextframe == NULL) {  /* Lua function on top. */
    void *cf = uj_cframe_raw(L->cframe);
    if (cf == NULL || (char *)uj_cframe_pc(cf) == (char *)uj_cframe_L(cf))
      return NO_BCPOS;
    ins = uj_cframe_pc(cf);  /* Only happens during error/hook handling. */
  } else {
    if (frame_islua(nextframe)) {
      ins = frame_pc(nextframe);
    } else if (frame_iscont(nextframe)) {
      ins = frame_contpc(nextframe);
    } else {
      /* Lua function below errfunc/gc/hook: find cframe to get the PC. */
      void *cf = uj_cframe_raw(L->cframe);
      TValue *f = L->base-1;
       for (;;) {
        if (cf == NULL) { return NO_BCPOS; }
        while (uj_cframe_nres(cf) < 0) {
          if (f >= uj_state_stack_restore(L, -uj_cframe_nres(cf))) { break; }
          cf = uj_cframe_raw(uj_cframe_prev(cf));
          if (cf == NULL) { return NO_BCPOS; }
        }
        if (f < nextframe) { break; }
        if (frame_islua(f)) {
          f = frame_prevl(f);
        } else {
          if (frame_isc(f) || (LJ_HASFFI && frame_iscont(f) &&
                              (f-1)->u32.lo == LJ_CONT_FFI_CALLBACK)) {
            cf = uj_cframe_raw(uj_cframe_prev(cf));
          }
          f = frame_prevd(f);
        }
      }
      ins = uj_cframe_pc(cf);
    }
  }
  pt = funcproto(fn);
  pos = proto_bcpos(pt, ins) - 1;
#if LJ_HASJIT
  if (pos > pt->sizebc) {  /* Undo the effects of lj_trace_exit for JLOOP. */
    GCtrace *T = (GCtrace *)((char *)(ins-1) - offsetof(GCtrace, startins));
    lua_assert(bc_isret(bc_op(ins[-1])));
    pos = proto_bcpos(pt, T->startpc);
  }
#endif
  return pos;
}

/* -- Line numbers -------------------------------------------------------- */

/* Get line number for function/frame. */
static BCLine debug_frameline(lua_State *L, GCfunc *fn, const TValue *nextframe)
{
  BCPos pc = lj_debug_framepc(L, fn, nextframe);
  if (pc != NO_BCPOS) {
    GCproto *pt = funcproto(fn);
    lua_assert(pc <= pt->sizebc);
    return uj_proto_line(pt, pc);
  }
  return -1;
}

BCLine lj_debug_frameline(lua_State *L, GCfunc *fn, const TValue *nextframe)
{
  return debug_frameline(L, fn, nextframe);
}

/* -- Variable names ------------------------------------------------------ */

/*
 * Returns 1 if `pc1` is executed conditionally relative to `pc2`,
 * 0 otherwise
 */
static int debug_conditional(const GCproto *pt, const BCIns *pc1,
                             const BCIns *pc2)
{
  const BCIns *ins;

  for (ins = proto_bc(pt); ins < pc1; ++ins) {
    if (bcmode_d(bc_op(*ins)) == BCMjump) {
      const BCIns *target = bc_target(ins);
      if (target <= pc2 && target > pc1)
        return 1;
    }
  }
  return 0;
}

/*
 * Finds instruction that defines `slot`,
 * returns NULL in case it cannot say for sure
 */
static const BCIns *debug_findslotdef(const GCproto *pt, const BCIns *pc,
                                      BCReg slot)
{
  const BCIns *res = pc;

  while (--res > proto_bc(pt)) {
    BCIns ins = *res;
    BCOp op = bc_op(ins);
    BCReg ra = bc_a(ins);
    if (bcmode_a(op) == BCMbase) {
      if (slot >= ra && (op != BC_KNIL || slot <= bc_d(ins)))
        return NULL;
    } else if (bcmode_a(op) == BCMdst && ra == slot) {
      return debug_conditional(pt, res, pc) ? NULL : res;
    }
  }
  return NULL;
}

/* Deduce name of an object from slot number and PC. */
const char *lj_debug_slotname(GCproto *pt, const BCIns *ip, BCReg slot,
                              const char **name)
{
  const char *lname;
  const BCIns *def;
  BCIns ins;
  BCReg ra;

  lname = uj_proto_varname(pt, proto_bcpos(pt, ip), slot);
  if (lname != NULL) { *name = lname; return "local"; }
  def = debug_findslotdef(pt, ip, slot);
  if (!def)
    return NULL;

  ins = *def;
  ra = bc_a(ins);
  switch (bc_op(ins)) {
  case BC_MOV:
    if (ra == slot) {
      slot = bc_d(ins);
      return lj_debug_slotname(pt, ip, slot, name);
    }
  case BC_GGET:
    *name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_d(ins))));
    return "global";
  case BC_TGETS:
    *name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_c(ins))));
    if (def > proto_bc(pt)) {
      BCIns insp = def[-1];
      if (bc_op(insp) == BC_MOV && bc_a(insp) == ra+1 &&
          bc_d(insp) == bc_b(ins))
        return "method";
    }
    return "field";
  case BC_UGET:
    *name = uj_proto_uvname(pt, bc_d(ins));
    return "upvalue";
  default:
    return NULL;
  }
}

/* Deduce function name from caller of a frame. */
const char *lj_debug_funcname(lua_State *L, TValue *frame, const char **name) {
  TValue *pframe;
  GCfunc *fn;
  BCPos pc;
  if (frame <= L->stack) {
    return NULL;
  }
  if (frame_isvarg(frame)) {
    frame = frame_prevd(frame);
  }
  pframe = frame_prev(frame);
  if (frame_isdummy(L, pframe)) {
    return NULL;
  }
  fn = frame_func(pframe);
  pc = lj_debug_framepc(L, fn, frame);
  if (pc != NO_BCPOS) {
    GCproto *pt = funcproto(fn);
    const BCIns *ip = &proto_bc(pt)[lua_check(pc < pt->sizebc, pc)];
    enum MMS mm = bcmode_mm(bc_op(*ip));
    if (mm == MM_call) {
      BCReg slot = bc_a(*ip);
      if (bc_op(*ip) == BC_ITERC) slot -= 3;
      return lj_debug_slotname(pt, ip, slot, name);
    } else if (mm != MM__MAX) {
      *name = strdata(uj_meta_name(G(L), mm));
      return "metamethod";
    }
  }
  return NULL;
}

/* -- Source code locations ----------------------------------------------- */

/* Add current location of a frame to error message. */
void lj_debug_addloc(lua_State *L, const char *msg, const TValue *frame, const TValue *nextframe) {
  if (frame && !frame_isdummy(L, frame)) {
    GCfunc *fn = frame_func(frame);
    if (isluafunc(fn)) {
      BCLine line = debug_frameline(L, fn, nextframe);
      if (line >= 0) {
        char buf[LUA_IDSIZE];
        uj_proto_namencpy(buf, funcproto(fn), sizeof(buf));
        uj_str_pushf(L, "%s:%d: %s", buf, line, msg);
        return;
      }
    }
  }
  uj_str_pushf(L, "%s", msg);
}

/* -- Public debug API ---------------------------------------------------- */

int lj_debug_getinfo(lua_State *L, const char *what, lj_Debug *ar, int ext)
{
  int opt_f = 0, opt_L = 0;
  TValue *frame = NULL;
  TValue *nextframe = NULL;
  GCfunc *fn;
  if (*what == '>') {
    TValue *func = L->top - 1;
    api_check(L, tvisfunc(func));
    fn = funcV(func);
    L->top--;
    what++;
  } else {
    uint32_t offset = (uint32_t)ar->i_ci & 0xffff;
    uint32_t size = (uint32_t)ar->i_ci >> 16;
    lua_assert(offset != 0);
    frame = L->stack + offset;
    if (size) nextframe = frame + size;
    lua_assert(frame <= L->maxstack &&
               (!nextframe || nextframe <= L->maxstack));
    fn = frame_func(frame);
    lua_assert(fn->c.gct == ~LJ_TFUNC);
  }
  for (; *what; what++) {
    if (*what == 'S') {
      if (isluafunc(fn)) {
        GCproto *pt = funcproto(fn);
        BCLine firstline = pt->firstline;
        ar->source = proto_chunknamestr(pt);
        uj_proto_namencpy(ar->short_src, pt, sizeof(ar->short_src));
        ar->linedefined = (int)firstline;
        ar->lastlinedefined = (int)(firstline + pt->numline);
        ar->what = (firstline || !pt->numline) ? "Lua" : "main";
      } else {
        ar->source = "=[C]";
        ar->short_src[0] = '[';
        ar->short_src[1] = 'C';
        ar->short_src[2] = ']';
        ar->short_src[3] = '\0';
        ar->linedefined = -1;
        ar->lastlinedefined = -1;
        ar->what = "C";
      }
    } else if (*what == 'l') {
      ar->currentline = frame ? debug_frameline(L, fn, nextframe) : -1;
    } else if (*what == 'u') {
      ar->nups = fn->c.nupvalues;
      if (ext) {
        if (isluafunc(fn)) {
          GCproto *pt = funcproto(fn);
          ar->nparams = pt->numparams;
          ar->isvararg = !!(pt->flags & PROTO_VARARG);
        } else {
          ar->nparams = 0;
          ar->isvararg = 1;
        }
      }
    } else if (*what == 'n') {
      ar->namewhat = frame ? lj_debug_funcname(L, frame, &ar->name) : NULL;
      if (ar->namewhat == NULL) {
        ar->namewhat = "";
        ar->name = NULL;
      }
    } else if (*what == 'f') {
      opt_f = 1;
    } else if (*what == 'L') {
      opt_L = 1;
    } else {
      return 0;  /* Bad option. */
    }
  }
  if (opt_f) {
    setfuncV(L, L->top, fn);
    uj_state_stack_incr_top(L);
  }
  if (opt_L) {
    if (isluafunc(fn)) {
      GCtab *t = lj_tab_new(L, 0, 0);
      GCproto *pt = funcproto(fn);
      const void *lineinfo = proto_lineinfo(pt);
      if (lineinfo) {
        BCLine first = pt->firstline;
        int sz = pt->numline < 256 ? 1 : pt->numline < 65536 ? 2 : 4;
        size_t i, szl = pt->sizebc-1;
        for (i = 0; i < szl; i++) {
          BCLine line = first +
            (sz == 1 ? (BCLine)((const uint8_t *)lineinfo)[i] :
             sz == 2 ? (BCLine)((const uint16_t *)lineinfo)[i] :
             (BCLine)((const uint32_t *)lineinfo)[i]);
          if (line != first)
            setboolV(lj_tab_setint(L, t, line), 1);
        }
      }
      settabV(L, L->top, t);
    } else {
      setnilV(L->top);
    }
    uj_state_stack_incr_top(L);
  }
  return 1;  /* Ok. */
}

