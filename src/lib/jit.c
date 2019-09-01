/*
 * JIT library.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "lua.h"
#include "lauxlib.h"
#include "lextlib.h"

#include "uj_arch.h"
#include "lj_obj.h"
#include "uj_err.h"
#include "uj_dispatch.h"
#include "uj_str.h"
#include "utils/cpuinfo.h"
#include "uj_lib.h"

#include "luajit.h"

/* -- jit.* functions ----------------------------------------------------- */

#define LJLIB_MODULE_jit

static int setjitmode(lua_State *L, int mode)
{
  int idx = 0;
  const int nargs = lua_gettop(L);
  const TValue *arg1 = L->base;
  const TValue *arg2 = L->base + 1;

  if (nargs == 0 || tvisnil(arg1)) { /* jit.on/off/flush([nil]) */
    mode |= LUAJIT_MODE_ENGINE;
  } else {
    /* jit.on/off/flush(func|proto, nil|true|false) */
    if (tvisfunc(arg1) || tvisproto(arg1)) {
      idx = 1;
    } else if (!tvistrue(arg1)) { /* jit.on/off/flush(true, nil|true|false) */
      uj_err_argt(L, 1, LUA_TFUNCTION);
    }
    if (nargs >= 2 && tvisbool(arg2)) {
      mode |= boolV(arg2) ? LUAJIT_MODE_ALLFUNC : LUAJIT_MODE_ALLSUBFUNC;
    } else {
      mode |= LUAJIT_MODE_FUNC;
    }
  }

  if (luaJIT_setmode(L, idx, mode) != 1) {
    if ((mode & LUAJIT_MODE_MASK) == LUAJIT_MODE_ENGINE) {
      uj_err_caller(L, UJ_ERR_NOJIT);
    }
    uj_err_argt(L, 1, LUA_TFUNCTION);
  }

  return 0;
}

LJLIB_CF(jit_on)
{
#if UJIT_IPROF_ENABLED
  /* This check is necessary until trace profiling is not introduced */
  if (L->iprof)
    uj_err_caller(L, UJ_ERR_IPROF_ENABLED_JIT);
#endif /* UJIT_IPROF_ENABLED */
  return setjitmode(L, LUAJIT_MODE_ON);
}

LJLIB_CF(jit_off)
{
  return setjitmode(L, LUAJIT_MODE_OFF);
}

LJLIB_CF(jit_flush)
{
#if LJ_HASJIT
  if (L->base < L->top && tvisnum(L->base)) {
    int traceno = uj_lib_checkint(L, 1);
    luaJIT_setmode(L, traceno, LUAJIT_MODE_FLUSH|LUAJIT_MODE_TRACE);
    return 0;
  }
#endif /* LJ_HASJIT */
  return setjitmode(L, LUAJIT_MODE_FLUSH);
}

#if LJ_HASJIT
/* Push a string for every flag bit that is set. */
static void flagbits_to_strings(lua_State *L, uint32_t flags, uint32_t base,
                                const char *str)
{
  for (; *str; base <<= 1, str += 1+*str)
    if (flags & base)
      setstrV(L, L->top++, uj_str_new(L, str+1, *(uint8_t *)str));
}
#endif

LJLIB_CF(jit_status)
{
#if LJ_HASJIT
  jit_State *J = L2J(L);
  L->top = L->base;
  setboolV(L->top++, (J->flags & JIT_F_ON) ? 1 : 0);
  flagbits_to_strings(L, J->flags, JIT_F_CPU_FIRST, JIT_F_CPUSTRING);
  flagbits_to_strings(L, J->flags, JIT_F_OPT_FIRST, JIT_F_OPTSTRING);
  return (int)(L->top - L->base);
#else /* !LJ_HASJIT */
  setboolV(L->top++, 0);
  return 1;
#endif /* LJ_HASJIT */
}

LJLIB_PUSH(top-5) LJLIB_SET(os)
LJLIB_PUSH(top-4) LJLIB_SET(arch)
LJLIB_PUSH(top-3) LJLIB_SET(version_num)
LJLIB_PUSH(top-2) LJLIB_SET(version)

#include "lj_libdef.h"

/* -- jit.opt module ------------------------------------------------------ */

#if LJ_HASJIT

#define LJLIB_MODULE_jit_opt

/* Parse optimization level. */
static int jitopt_level(jit_State *J, const char *str)
{
  if (str[0] >= '0' && str[0] <= '9' && str[1] == '\0') {
    uint32_t flags;
    if (str[0] == '0') flags = JIT_F_OPT_0;
    else if (str[0] == '1') flags = JIT_F_OPT_1;
    else if (str[0] == '2') flags = JIT_F_OPT_2;
    else if (str[0] == '4') flags = JIT_F_OPT_4;
    else flags = JIT_F_OPT_3;
    J->flags = (J->flags & ~JIT_F_OPT_MASK) | flags;
    return 1;  /* Ok. */
  }
  return 0;  /* No match. */
}

/* Parse optimization flag. */
static int jitopt_flag(jit_State *J, const char *str) {
  const char *lst = JIT_F_OPTSTRING;
  uint32_t opt;
  int set = 1;
  if (str[0] == '+') {
    str++;
  } else if (str[0] == '-') {
    str++;
    set = 0;
  }
  for (opt = JIT_F_OPT_FIRST; ; opt <<= 1) {
    size_t len = *(const uint8_t *)lst;
    if (len == 0) {
      break;
    }
    if (strncmp(str, lst + 1, len) == 0 && str[len] == '\0') {
      if (set) {
        J->flags |= opt;
      } else {
        J->flags &= ~opt;
      }
      return 1;  /* Ok. */
    }
    lst += 1 + len;
  }
  return 0;  /* No match. */
}

/* Parse optimization parameter. */
static int jitopt_param(jit_State *J, const char *str)
{
  const char *lst = JIT_P_STRING;
  int i;
  for (i = 0; i < JIT_P__MAX; i++) {
    size_t len = *(const uint8_t *)lst;
    lua_assert(len != 0);
    if (strncmp(str, lst+1, len) == 0 && str[len] == '=') {
      int32_t n = 0;
      const char *p = &str[len+1];
      while (*p >= '0' && *p <= '9')
        n = n*10 + (*p++ - '0');
      if (*p) return 0;  /* Malformed number. */
      J->param[i] = n;
      if (i == JIT_P_hotloop)
        uj_dispatch_init_hotcount(J2G(J));
      return 1;  /* Ok. */
    }
    lst += 1+len;
  }
  return 0;  /* No match. */
}

/* jit.opt.start(flags...) */
LJLIB_CF(jit_opt_start)
{
  jit_State *J = L2J(L);
  int nargs = (int)(L->top - L->base);
  if (nargs == 0) {
    J->flags = (J->flags & ~JIT_F_OPT_MASK) | JIT_F_OPT_DEFAULT;
  } else {
    int i;
    for (i = 1; i <= nargs; i++) {
      const char *str = strdata(uj_lib_checkstr(L, i));
      if (!jitopt_level(J, str) &&
          !jitopt_flag(J, str) &&
          !jitopt_param(J, str))
        uj_err_callerv(L, UJ_ERR_JITOPT, str);
    }
  }
  return 0;
}

#include "lj_libdef.h"

#endif /* LJ_HASJIT */

/* -- JIT compiler initialization ----------------------------------------- */

#if LJ_HASJIT
/* Default values for JIT parameters. */
static const int32_t jit_param_default[JIT_P__MAX+1] = {
#define JIT_PARAMINIT(len, name, value) (value),
JIT_PARAMDEF(JIT_PARAMINIT)
#undef JIT_PARAMINIT
  0
};
#endif /* LJ_HASJIT */

#if LJ_HASJIT
/* Arch-dependent CPU detection. */
static uint32_t jit_cpudetect(void)
{
  uint32_t flags = 0;

  if (cpuinfo_has_sse2()) {
    flags |= JIT_F_SSE2;
  }

  if (cpuinfo_has_sse3()) {
    flags |= JIT_F_SSE3;
  }

  if (cpuinfo_has_sse4_1()) {
    flags |= JIT_F_SSE4_1;
  }

  if (cpuinfo_has_cmov()) {
    flags |= JIT_F_CMOV;
  }

  return flags;
}
#endif /* LJ_HASJIT */

/* Initialize JIT compiler. */
static void jit_init(lua_State *L)
{
#if LJ_HASJIT
  uint32_t flags = jit_cpudetect();
  jit_State *J = L2J(L);
  J->flags = flags | JIT_F_ON | JIT_F_OPT_DEFAULT;
  memcpy(J->param, jit_param_default, sizeof(J->param));
  uj_dispatch_update(G(L));
#else /* !LJ_HASJIT */
  UNUSED(L);
#endif /* LJ_HASJIT */
}

LUALIB_API int luaopen_jit(lua_State *L)
{
  lua_pushliteral(L, UJ_OS_NAME);
  lua_pushliteral(L, UJ_ARCH_NAME);
  lua_pushinteger(L, LUAJIT_VERSION_NUM);
  lua_pushliteral(L, LUAJIT_VERSION);
  LJ_LIB_REG(L, LUAE_JITLIBNAME, jit);
#if LJ_HASJIT
  LJ_LIB_REG(L, "jit.opt", jit_opt);
#endif /* LJ_HASJIT */
  L->top -= 2;
  jit_init(L);
  return 1;
}

