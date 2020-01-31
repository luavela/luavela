/*
 * Target architecture selection.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _UJ_ARCH_H
#define _UJ_ARCH_H

/* Target OS. Values match LuaJIT's definitions. */
#define UJIT_OS_LINUX 2
#define UJIT_OS_OSX 3

#ifndef UJIT_OS
#error "UJIT_OS is expected to be set by the build system"
#endif /* !UJIT_OS */

#if UJIT_OS == UJIT_OS_LINUX
#define UJ_OS_NAME "Linux"
#elif UJIT_OS == UJIT_OS_OSX
#define UJ_OS_NAME "OSX"
#endif

#define UJ_TARGET_LINUX (UJIT_OS == UJIT_OS_LINUX)
#define UJ_TARGET_OSX (UJIT_OS == UJIT_OS_OSX)

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

#ifndef UJ_PAGESIZE
#define UJ_PAGESIZE 4096
#endif

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

#endif
