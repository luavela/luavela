/*
 * Configuration header.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef luaconf_h
#define luaconf_h

#include <limits.h>
#include <stddef.h>
#include <assert.h>

/*
 * Support for internal assertions.
 * Unlike other implementations (PUC-Rio Lua, LuaJIT),
 * we do *not* support flags LUA_USE_ASSERT or LUA_USE_APICHECK. Instead,
 * all macros are defined unconditionally, and actual toggling assertions
 * on/off should be controlled at the build system level.
 */

#define lua_assert(c)       assert(c)
#define luai_apicheck(L, o) { (void)(L); assert(o); }

/* Special file system characters. */
#define LUA_DIRSEP      "/"
#define LUA_PATHSEP     ";"
#define LUA_PATH_MARK   "?"
#define LUA_EXECDIR     "!"
#define LUA_IGMARK      "-"

/*
 * Default path for loading Lua and C modules with require().
 * The entire scheme, with minor modifications and extra comments,
 * is borrowed from LuaJIT.
 * All symbols prefixed with LUAIMPL_ are used to implement this header file
 * and should definitely *NOT* be used outside of it.
 */

#define LUAIMPL_LOCALROOT      LUA_DIRSEP "usr" LUA_DIRSEP "local"
#define LUAIMPL_LUADIR         LUA_DIRSEP "lua" LUA_DIRSEP "5.1"

/* Bits and pieces of paths and module masks: */
#define LUAIMPL_LIBDIR         LUA_DIRSEP "lib"   LUAIMPL_LUADIR
#define LUAIMPL_SHAREDIR       LUA_DIRSEP "share" LUAIMPL_LUADIR
#define LUAIMPL_EXTDIR         LUA_DIRSEP "share" LUA_DIRSEP "ujit"
#define LUAIMPL_MODULE         LUA_DIRSEP LUA_PATH_MARK ".lua"
#define LUAIMPL_MODULE_INIT    LUA_DIRSEP LUA_PATH_MARK LUA_DIRSEP "init.lua"
#define LUAIMPL_MODULE_SO      LUA_DIRSEP LUA_PATH_MARK ".so"
#define LUAIMPL_MODULE_LOADALL LUA_DIRSEP "loadall.so"

#ifdef LUA_ROOT

/*
 * Extra paths that can be appended to the lists of the default Lua and C paths
 * depending on the LUA_ROOT parameter. Furthermore, for multilib systems
 * the architecture-specific library path component can be tweaked with
 * LUA_MULTILIB.
 */

#ifndef LUA_MULTILIB
#define LUA_MULTILIB "lib"
#endif /* !LUA_MULTILIB */

#define LUAIMPL_ROOT LUA_ROOT

#define LUAIMPL_LPATH                                             \
	LUA_PATHSEP LUA_ROOT LUAIMPL_SHAREDIR LUAIMPL_MODULE      \
	LUA_PATHSEP LUA_ROOT LUAIMPL_SHAREDIR LUAIMPL_MODULE_INIT

#define LUAIMPL_CPATH LUA_PATHSEP LUA_ROOT \
	LUA_DIRSEP LUA_MULTILIB LUAIMPL_LUADIR LUAIMPL_MODULE_SO

#else  /* !LUA_ROOT */

#define LUAIMPL_ROOT LUAIMPL_LOCALROOT

#define LUAIMPL_LPATH
#define LUAIMPL_CPATH

#endif /* LUA_ROOT */

/* The order of the paths matches the one from LuaJIT. */

#define LUA_PATH_DEFAULT "." LUAIMPL_MODULE                                \
	/* Impl-specific Lua path: */                                      \
	LUA_PATHSEP LUAIMPL_ROOT LUAIMPL_EXTDIR LUAIMPL_MODULE             \
	/* Impl-independent Lua paths #1: */                               \
	LUA_PATHSEP LUAIMPL_LOCALROOT LUAIMPL_SHAREDIR LUAIMPL_MODULE      \
	LUA_PATHSEP LUAIMPL_LOCALROOT LUAIMPL_SHAREDIR LUAIMPL_MODULE_INIT \
	/* Impl-independent Lua paths #2, optional: */                     \
	LUAIMPL_LPATH

#define LUA_CPATH_DEFAULT "." LUAIMPL_MODULE_SO                            \
	/* Impl-independent C path #1: */                                  \
	LUA_PATHSEP LUAIMPL_LOCALROOT LUAIMPL_LIBDIR LUAIMPL_MODULE_SO     \
	/* Impl-independent C path #2, optional: */                        \
	LUAIMPL_CPATH                                                      \
	/* Impl-independent "the last resort" C path: */                   \
	LUA_PATHSEP LUAIMPL_LOCALROOT LUAIMPL_LIBDIR LUAIMPL_MODULE_LOADALL

/* Environment variable names for path overrides and initialization code. */
#define LUA_PATH        "LUA_PATH"
#define LUA_CPATH       "LUA_CPATH"
#define LUA_INIT        "LUA_INIT"

/* Quoting in error messages. */
#define LUA_QL(x)       "'" x "'"
#define LUA_QS          LUA_QL("%s")

/* Various tunables. */
#define LUAI_MAXCSTACK  8000    /* Max. # of stack slots for a C func (<10K). */
#define LUAI_GCPAUSE    200     /* Pause GC until memory is at 200%. */
#define LUAI_GCMUL      200     /* Run GC at 200% of allocation speed. */
#define LUA_MAXCAPTURES 32      /* Max. pattern captures. */

/* Compatibility with older library function names. */
#define LUA_COMPAT_MOD          /* OLD: math.mod, NEW: math.fmod */
#define LUA_COMPAT_GFIND        /* OLD: string.gfind, NEW: string.gmatch */

/*
 * Following LUA_COMPAT_* macros do not have any actual effect in this
 * implementation. However, they are listed for a closer match with luaconf.h
 * from the PUC-Rio Lua.
 */
#undef  LUA_COMPAT_GETN      /* Hardcoded: No Lua 5.0 behavior for table.getn */
#undef  LUA_COMPAT_LOADLIB   /* Hardcoded: No _G.loadlib */
#define LUA_COMPAT_VARARG    /* Hardcoded: CLI creates the `arg` table */
#define LUA_COMPAT_LSTR    1 /* Hardcoded: No nesting of [[...]] */
#define LUA_COMPAT_OPENLIB   /* Hardcoded: luaL_openlib is here */

/* Configuration for the frontend (the uJIT executable). */
#define LUA_PROGNAME    "ujit"  /* Fallback frontend name. */
#define LUA_PROMPT      "> "    /* Interactive prompt. */
#define LUA_PROMPT2     ">> "   /* Continuation prompt. */
#define LUA_MAXINPUT    512     /* Max. input line length. */

/* Note: changing the following defines breaks the Lua 5.1 ABI. */
#define LUA_INTEGER     ptrdiff_t
#define LUA_IDSIZE      256      /* Size of lua_Debug.short_src. */

/*
 * Size of lauxlib and io.* on-stack buffers. Weird workaround to avoid using
 * unreasonable amounts of stack space, but still retain ABI compatibility.
 * Blame Lua for depending on BUFSIZ in the ABI, blame **** for wrecking it.
 */
#define LUAL_BUFFERSIZE (BUFSIZ > 16384 ? 8192 : BUFSIZ)

/*
 * The following defines are here only for compatibility with luaconf.h
 * from the standard Lua distribution. They must not be changed for uJIT.
 */
#define LUA_NUMBER_DOUBLE
#define LUA_NUMBER              double
#define LUAI_UACNUMBER          double
#define LUA_NUMBER_SCAN         "%lf"
#define LUA_NUMBER_FMT          "%.14g"
#define lua_number2str(s, n)    sprintf((s), LUA_NUMBER_FMT, (n))
#define LUAI_MAXNUMBER2STR      32
#define LUA_INTFRMLEN           "l"
#define LUA_INTFRM_T            long

/* Linkage of public API functions. */
#define LUA_API         extern __attribute__((visibility("default")))
#define LUALIB_API      LUA_API
#define LUAEXT_API      LUA_API

#endif /* !luaconf_h */
