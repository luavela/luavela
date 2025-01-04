/*
 * IR CALL* instruction definitions.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_IRCALL_H
#define _LJ_IRCALL_H

#include "lj_obj.h"
#include "lj_tab.h"
#include "jit/lj_ir.h"
#include "jit/lj_jit.h"
#include "lj_vm.h"
#include "uj_obj_immutable.h"
#include "uj_sbuf.h"
#if LJ_HASFFI
#include "ffi/lj_crecord.h"
#endif

#ifndef _BUILDVM_H
/* Do not include math header when building buildvm
** to avoid unnecessary dependencies.
*/
#include "utils/uj_math.h"
#endif

/* C call info for CALL* instructions. */
typedef struct CCallInfo {
  ASMFunction func;             /* Function pointer. */
  uint32_t flags;               /* Number of arguments and flags. */
} CCallInfo;

#define CCI_NARGS(ci)           ((ci)->flags & 0xff)    /* Extract # of args. */
#define CCI_NARGS_MAX           32                      /* Max. # of args. */

#define CCI_OTSHIFT             16
#define CCI_OPTYPE(ci)          ((ci)->flags >> CCI_OTSHIFT)  /* Get op/type. */
#define CCI_OPSHIFT             24
#define CCI_OP(ci)              ((ci)->flags >> CCI_OPSHIFT)  /* Get op. */

#define CCI_CALL_N              (IR_CALLN << CCI_OPSHIFT)
#define CCI_CALL_L              (IR_CALLL << CCI_OPSHIFT)
#define CCI_CALL_S              (IR_CALLS << CCI_OPSHIFT)

/* C call info flags. */
#define CCI_L                   0x0100  /* Implicit L arg. */
#define CCI_CASTU64             0x0200  /* Cast u64 result to number. */
#define CCI_NOFPRCLOBBER        0x0400  /* Does not clobber any FPRs. */
#define CCI_VARARG              0x0800  /* Vararg function. */
#define CCI_IMMUTABLE           0x1000  /* Makes argument immutable.
                                         * Should be also CALLS */
#define CCI_ALLOC               0x2000  /* Function allocates. */


/* Helpers for conditional function definitions. */
#define IRCALLCOND_ANY(x)               x

#define IRCALLCOND_FPMATH(x)           NULL

#if LJ_HASFFI
#define IRCALLCOND_FFI(x)               x
#else
#define IRCALLCOND_FFI(x)               NULL
#endif

#define ARG1_FP         1
#define ARG2_64         2

/* Function definitions for CALL* instructions. */
#define IRCALLDEF(_) \
  _(ANY,        uj_str_cmp,             2,         N, INT, CCI_NOFPRCLOBBER) \
  _(ANY,        uj_cstr_find,           4,         N, PTR, 0) \
  _(ANY,        uj_str_has_pattern_specials, 1,    N, INT, 0) \
  _(ANY,        uj_str_lower,           2,         N, STR, CCI_L|CCI_ALLOC) \
  _(ANY,        uj_str_upper,           2,         N, STR, CCI_L|CCI_ALLOC) \
  _(ANY,        uj_str_new,             3,         S, STR, CCI_L) \
  _(ANY,        uj_str_tonum,           2,         N, INT, 0) \
  _(ANY,        uj_str_fromint,         2,         N, STR, CCI_L) \
  _(ANY,        uj_str_fromnumber,      2,         N, STR, CCI_L) \
  _(ANY,        uj_str_trim,            2,         N, STR, CCI_L|CCI_ALLOC) \
  _(ANY,        lj_tab_new_jit,         2,         S, TAB, CCI_L) \
  _(ANY,        lj_tab_dup,             2,         S, TAB, CCI_L) \
  _(ANY,        lj_tab_newkey,          3,         S, P32, CCI_L) \
  _(ANY,        lj_tab_len,             1,         L, INT, 0) \
  _(ANY,        lj_tab_size,            1,         L, INT, CCI_NOFPRCLOBBER) \
  _(ANY,        lj_tab_iterate_jit,     2,         L, U32, CCI_NOFPRCLOBBER) \
  _(ANY,        lj_tab_concat,          6,         L, STR, CCI_L|CCI_ALLOC) \
  _(ANY,        lj_tab_nexta,           2,         L, INT, CCI_NOFPRCLOBBER) \
  _(ANY,        lj_tab_nexth,           3,         L, PTR, CCI_L|CCI_NOFPRCLOBBER) \
  _(ANY,        lj_tab_rawrindex_jit,   2,         L, PTR, CCI_L|CCI_NOFPRCLOBBER) \
  _(ANY,        lj_gc_step_jit,         2,         S, NIL, CCI_L) \
  _(ANY,        lj_gc_barrieruv,        2,         S, NIL, 0) \
  _(ANY,        uj_obj_new,             2,         S, P32, CCI_L) \
  _(ANY,        lj_math_random_step,    1,         S, NUM, CCI_CASTU64) \
  _(ANY,        uj_math_modi,           2,         N, INT, 0) \
  _(ANY,        sinh,                   1,         N, NUM, 0) \
  _(ANY,        cosh,                   1,         N, NUM, 0) \
  _(ANY,        tanh,                   1,         N, NUM, 0) \
  _(ANY,        uj_str_frombuf,         2,         L, STR, CCI_L|CCI_ALLOC) \
  _(ANY,        uj_sbuf_push_str,       2,         L, PTR, 0) \
  _(ANY,        uj_sbuf_push_char,      2,         L, PTR, 0) \
  _(ANY,        uj_sbuf_push_numint,    2,         L, PTR, 0) \
  _(ANY,        uj_sbuf_push_number,    2,         L, PTR, 0) \
  _(ANY,        uj_sbuf_push_int,       2,         L, PTR, 0) \
  _(ANY,        uj_sbuf_push_block,     3,         L, PTR, 0) \
  _(ANY,        uj_obj_immutable,       2,         S, NIL, CCI_L|CCI_IMMUTABLE|CCI_NOFPRCLOBBER) \
  _(ANY,        lj_tab_keys,            2,         L, TAB, CCI_L|CCI_ALLOC) \
  _(ANY,        lj_tab_values,          2,         L, TAB, CCI_L|CCI_ALLOC) \
  _(ANY,        lj_tab_toset,           2,         L, TAB, CCI_L|CCI_ALLOC) \
  _(FFI,        lj_carith_divi64,       ARG2_64,   N, I64, CCI_NOFPRCLOBBER) \
  _(FFI,        lj_carith_divu64,       ARG2_64,   N, U64, CCI_NOFPRCLOBBER) \
  _(FFI,        lj_carith_modi64,       ARG2_64,   N, I64, CCI_NOFPRCLOBBER) \
  _(FFI,        lj_carith_modu64,       ARG2_64,   N, U64, CCI_NOFPRCLOBBER) \
  _(FFI,        lj_carith_powi64,       ARG2_64,   N, I64, CCI_NOFPRCLOBBER) \
  _(FFI,        lj_carith_powu64,       ARG2_64,   N, U64, CCI_NOFPRCLOBBER) \
  _(FFI,        lj_cdata_setfin,        2,         N, P32, CCI_L) \
  _(FFI,        strlen,                 1,         L, INTP, 0) \
  _(FFI,        memcpy,                 3,         S, PTR, 0) \
  _(FFI,        memset,                 3,         S, PTR, 0) \
  _(FFI,        lj_crecord_errno,       0,         S, INT, CCI_NOFPRCLOBBER)
  \
  /* End of list. */

typedef enum {
#define IRCALLENUM(cond, name, nargs, kind, type, flags)        IRCALL_##name,
IRCALLDEF(IRCALLENUM)
#undef IRCALLENUM
  IRCALL__MAX
} IRCallID;

/*
 * NB! Using a macro preserves the convenient look of the interface and works
 * around undefined behaviour issue with the arg of enum type preceding
 * the variadic arguments.
 */
TRef lj_ir_call_(jit_State *J, int id, ...);
#define lj_ir_call(J, /* IRCallID */ id, ...) \
  lj_ir_call_((J), (int)(id), __VA_ARGS__)

LJ_DATA const CCallInfo lj_ir_callinfo[IRCALL__MAX+1];

#endif
