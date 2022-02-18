/*
 * Target architecture selection.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_ARCH_H
#define _UJ_ARCH_H

/* Target OS. */

#ifdef UJ_TARGET_LINUX
#define UJ_OS_NAME "Linux"
#endif

#ifdef UJ_TARGET_MACOS
#define UJ_OS_NAME "OSX" /* Keep this (and not MACOS e.g.) for compatibility */
#endif

#ifndef UJ_OS_NAME
#error "Unable to detect target OS (expected to be set by the build system)"
#endif

/*
 * Set target architecture properties.
 * Should not be changed for uJIT.
 */
#define UJ_ARCH_NAME "x64"
#define UJ_TARGET_EHRETREG 0
#define UJ_TARGET_JUMPRANGE 31 /* +-2^31 = +-2GB */
#define UJ_TARGET_MASKSHIFT 1
#define UJ_TARGET_MASKROT 1
#define UJ_TARGET_UNALIGNED 1
#define UJ_PAGESIZE 4096

/*
 * Note: LJ_HASJIT, LJ_HASFFI and LJ_52 didn't get renamed to have UJ_ prefix
 * for easier backporting from LuaJIT
 */

/* Disable or enable the JIT compiler. */
#if defined(UJIT_DISABLE_JIT)
#define LJ_HASJIT 0
#else
#define LJ_HASJIT 1
#endif

/* Disable or enable the FFI extension. */
#if defined(UJIT_DISABLE_FFI)
#define LJ_HASFFI 0
#else
#define LJ_HASFFI 1
#endif

/*
 * Compatibility with Lua 5.1 vs. 5.2.
 * The following is unconditional regardless of LJ_52 (compared to LuaJIT):
 *   1. The following code triggers "ambiguous syntax" error:
 *          f()
 *          (g or h)()
 *   2. math.mod, math.fmod and string.gfind are exported
 *   3. os.execute, file.close and coroutine.running have Lua 5.1 behaviour
 */
#ifdef UJIT_ENABLE_LUA52COMPAT
#define LJ_52 1
#else
#define LJ_52 0
#endif

#endif /* !_UJ_ARCH_H */
