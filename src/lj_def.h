/*
 * uJIT common internal definitions.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#ifndef _LJ_DEF_H
#define _LJ_DEF_H

#include "luaconf.h"

#include <stdint.h>
#include <inttypes.h>

/* Needed everywhere. */
#include <string.h>
#include <stdlib.h>

/* Various VM limits. */
#define LJ_MAX_MEM      0x7fffff00      /* Max. total memory allocation. */
#define LJ_MAX_ALLOC    LJ_MAX_MEM      /* Max. individual allocation length. */
#define LJ_MAX_STR      LJ_MAX_MEM      /* Max. string length. */
#define LJ_MAX_SBUF     LJ_MAX_MEM      /* Max. string buffer length. */
#define LJ_MAX_UDATA    LJ_MAX_MEM      /* Max. userdata length. */

#define LJ_MAX_STRTAB   (1<<26)         /* Max. string table size. */
#define LJ_MAX_HBITS    26              /* Max. hash bits. */
#define LJ_MAX_ABITS    28              /* Max. bits of array key. */
#define LJ_MAX_ASIZE    ((1<<(LJ_MAX_ABITS-1))+1)  /* Max. array part size. */
#define LJ_MAX_COLOSIZE 16              /* Max. elems for colocated array. */

#define LJ_MAX_LINE     LJ_MAX_MEM      /* Max. source code line number. */
#define LJ_MAX_XLEVEL   200             /* Max. syntactic nesting level. */
#define LJ_MAX_BCINS    (1<<26)         /* Max. # of bytecode instructions. */
#define LJ_MAX_SLOTS    125             /* Max. # of slots in a Lua func. */
#define LJ_MAX_LOCVAR   120             /* Max. # of local variables. */
#define LJ_MAX_UPVAL    60              /* Max. # of upvalues. */

#define LJ_MAX_IDXCHAIN 100             /* __index/__newindex chain limit. */
#define LJ_STACK_EXTRA  5               /* Extra stack space (metamethods). */

#define LJ_NUM_CBPAGE   1               /* Number of FFI callback pages. */

/* Minimum table/buffer sizes. */
#define LJ_MIN_GLOBAL   6               /* Min. global table size (hbits). */
#define LJ_MIN_REGISTRY 2               /* Min. registry size (hbits). */
#define LJ_MIN_STRTAB   256             /* Min. string table size (pow2). */
#define LJ_MIN_SBUF     32              /* Min. string buffer length. */
#define LJ_MIN_VECSZ    8               /* Min. size for growable vectors. */
#define LJ_MIN_IRSZ     32              /* Min. size for growable IR. */
#define LJ_MIN_K64SZ    16              /* Min. size for chained K64Array. */

/* JIT compiler limits. */
#define LJ_MAX_JSLOTS   250             /* Max. # of stack slots for a trace. */
#define LJ_MAX_PHI      64              /* Max. # of PHIs for a loop. */
#define LJ_MAX_EXITSTUBGR       16      /* Max. # of exit stub groups. */

/* Various macros. */
#define UNUSED(x)       ((void)(x))     /* to avoid warnings */
#define UNUSED_FUNC     __attribute__((unused))

#define U64x(hi, lo)    (((uint64_t)0x##hi << 32) + (uint64_t)0x##lo)
#define i32ptr(p)       ((int32_t)(intptr_t)(void *)(p))
#define u32ptr(p)       ((uint32_t)(intptr_t)(void *)(p))

#define checki8(x)      ((x) == (int32_t)(int8_t)(x))
#define checku8(x)      ((x) == (int32_t)(uint8_t)(x))
#define checki16(x)     ((x) == (int32_t)(int16_t)(x))
#define checku16(x)     ((x) == (int32_t)(uint16_t)(x))
#define checki32(x)     ((x) == (int32_t)(x))
#define checku32(x)     ((x) == (uint32_t)(x))
#define checkptr32(x)   ((uintptr_t)(x) == (uint32_t)(uintptr_t)(x))

/* Every half-decent C compiler transforms this into a rotate instruction. */
#define lj_rol(x, n)    (((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))
#define lj_ror(x, n)    (((x)<<(-(int)(n)&(8*sizeof(x)-1))) | ((x)>>(n)))

#define LJ_NORET        __attribute__((noreturn))
#define LJ_ALIGN(n)     __attribute__((aligned(n)))
#define LJ_AINLINE      inline __attribute__((always_inline))
#define LJ_NOINLINE     __attribute__((noinline))

#define LJ_LIKELY(x)    __builtin_expect(!!(x), 1)
#define LJ_UNLIKELY(x)  __builtin_expect(!!(x), 0)

#define lj_ctz(x)       ((uint32_t)__builtin_ctz(x))
#define lj_bsr(x)       ((uint32_t)(__builtin_clz(x)^31))
#define lj_ffs(x)       ((uint32_t)__builtin_ffs(x))


static LJ_AINLINE uint32_t lj_pow2(uint32_t x)
{
  return 1u << x;
}

/* Attributes for interfaces that throw run-time errors */
#define UJ_ERRRUN __attribute__((noreturn, noinline))

static LJ_AINLINE uint32_t lj_bswap(uint32_t x) {
  return (uint32_t)__builtin_bswap32((int32_t)x);
}

static LJ_AINLINE uint64_t lj_bswap64(uint64_t x) {
  return (uint64_t)__builtin_bswap64((int64_t)x);
}

typedef union __attribute__((packed)) Unaligned16 {
  uint16_t u;
  uint8_t b[2];
} Unaligned16;

typedef union __attribute__((packed)) Unaligned32 {
  uint32_t u;
  uint8_t b[4];
} Unaligned32;

/* Unaligned load of uint16_t. */
static LJ_AINLINE uint16_t lj_getu16(const void *p) {
  return ((const Unaligned16 *)p)->u;
}

/* Unaligned load of uint32_t. */
static LJ_AINLINE uint32_t lj_getu32(const void *p) {
  return ((const Unaligned32 *)p)->u;
}

/*
 * Attributes for internal interface declarations.
 * According to the GCC manual, "extern declarations are not affected by
 * -fvisibility", <...> so it is more effective to use
 * "__attribute ((visibility))" <..> to tell the compiler which "extern"
 * declarations should be treated as hidden.
 * Thus let's explicitly specify visibility of symbols belonging to the
 * internal API:
 */
#define LJ_NOAPI    extern __attribute__((visibility("hidden")))
#define LJ_ASMENTRY LJ_NOAPI /* For arbitrary pointers inside the VM code. */
#define LJ_DATA     LJ_NOAPI
#define LJ_DATADEF  /* Simply to preserve the "declare ~ define" dichotomy. */

#define lua_check(c, e) (lua_assert(c), (e))
#define api_check       luai_apicheck

/* Static assertions. */
#define LJ_ASSERT_NAME2(name, line)     name ## line
#define LJ_ASSERT_NAME(line)            LJ_ASSERT_NAME2(lj_assert_, line)
#ifdef __COUNTER__
#define LJ_STATIC_ASSERT(cond) \
  extern void LJ_ASSERT_NAME(__COUNTER__)(int STATIC_ASSERTION_FAILED[(cond)?1:-1])
#else
#define LJ_STATIC_ASSERT(cond) \
  extern void LJ_ASSERT_NAME(__LINE__)(int STATIC_ASSERTION_FAILED[(cond)?1:-1])
#endif

/* Macros to enable / disable pedantic warnings */
#define UJ_PEDANTIC_OFF \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#define UJ_PEDANTIC_ON \
    _Pragma("GCC diagnostic pop")

/* Macros to enable/disable -Wstringop-truncation */
#if __GNUC__ >= 8
#define UJ_STRINGOP_TRUNCATION_WARN_OFF \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wstringop-truncation\"")
#define UJ_STRINGOP_TRUNCATION_WARN_ON \
    _Pragma("GCC diagnostic pop")
#else /* !(__GNUC__ >= 8)  */
#define UJ_STRINGOP_TRUNCATION_WARN_OFF /* stub */
#define UJ_STRINGOP_TRUNCATION_WARN_ON /* stub */
#endif /* __GNUC__ >= 8 */

#endif
