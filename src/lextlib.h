/*
 * Extended Lua API provided by uJIT.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _LEXTLIB_H
#define _LEXTLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "lua.h"

enum luae_HashF {
	LUAE_HASHF_DEFAULT = 0,
	LUAE_HASHF_MURMUR = LUAE_HASHF_DEFAULT,
	LUAE_HASHF_CITY
};

struct luae_Options {
	lua_State       *datastate; /* If not NULL, hashftype is ignored. */
	lua_Alloc        allocf;
	void            *allocud;
	enum luae_HashF  hashftype;
	int              disableitern;
};

/* Extended thread statuses; the 5th bit must be set to 1. */

#define LUAE_TIMEOUT 16 /* Returned by lua_resume in case of timeout */

/* API for obtaining various metrics from the platform. */

struct luae_Metrics {
	size_t strnum;
	size_t tabnum;
	size_t strhash_hit;
	size_t strhash_miss;
	size_t udatanum;
	size_t gc_total;
	size_t gc_sealed;
	size_t gc_freed;
	size_t gc_allocated;
	size_t gc_steps_pause;
	size_t gc_steps_propagate;
	size_t gc_steps_atomic;
	size_t gc_steps_sweepstring;
	size_t gc_steps_sweep;
	size_t gc_steps_finalize;
	size_t jit_snap_restore;
	size_t jit_mcode_size;
	unsigned int jit_trace_num;
};

/* Returns a string describing current uJIT version. */
LUAEXT_API const char *luaE_verstring(void);

LUAEXT_API struct luae_Metrics luaE_metrics(lua_State *L);
LUAEXT_API size_t             luaE_totalmem(void);

/* API for sealing and managing data slaves and data roots: */

/* NB! luaE_newdataslave is deprecated, use luaE_createstate instead. */
LUAEXT_API lua_State   *luaE_newdataslave(lua_State *datastate);
LUAEXT_API void         luaE_setdataroot(lua_State *L, int idx);
LUAEXT_API void         luaE_getdataroot(lua_State *L);
LUAEXT_API void         luaE_seal(lua_State *L, int idx);

LUAEXT_API lua_State   *luaE_createstate(const struct luae_Options *opt);

/* Makes the value stored at index idx immutable in-place. */
LUAEXT_API void luaE_immutable(lua_State *L, int idx);

/*
 * Checks if a function at idx uses its environment. Following logic applies:
 *  * For Lua functions, returns true if the function meets
 *    at least one of following conditions:
 *    * It references at least one global variable
 *    * It references at least one upvalue
 *    If neither condition is met, returns false.
 *  * For built-in functions, always returns false.
 *  * For registered C functions, always returns true.
 */
LUAEXT_API int luaE_usesfenv(lua_State *L, int idx);

/* Public extended API for iterating over tables. */
#define LUAE_ITER_BEGIN ((uint64_t)0)
#define LUAE_ITER_END   ((uint64_t)0)

/*
 * Pushes on stack the next key-value pair from the table stored at idx and
 * returns a new value of the internal iterator state for subsequent calls.
 * If the entire table is traversed, does not touch the stack and returns
 * LUAE_ITER_END. For the first invocation, iter_state must be set to
 * LUAE_ITER_BEGIN. NB! Calling code must *not* use iter_state and the return
 * value for anything but passing it to this interface.
 */
LUAEXT_API uint64_t luaE_iterate(lua_State *L, int idx, uint64_t iter_state);

/* Profiler public API. */
#define LUAE_PROFILE_SUCCESS 0
#define LUAE_PROFILE_ERR     1
#define LUAE_PROFILE_ERRMEM  2
#define LUAE_PROFILE_ERRIO   3

LUAEXT_API int luaE_profavailable(void);
LUAEXT_API int luaE_profinit(void);
LUAEXT_API int luaE_profterm(void);

/* Dumper public API. */

LUAEXT_API int  luaE_dumpstart(const lua_State *L, FILE *out);
LUAEXT_API int  luaE_dumpstop(const lua_State *L);
LUAEXT_API void luaE_dumpbc(lua_State *L, int idx, FILE *out);
LUAEXT_API void luaE_dumpbcsource(lua_State *L, int idx, FILE *out,
				  int hl_bc_pos);

/* Table public API. */

/*
 * Creates a shallow copy of a table at idx and pushes it on stack.
 */
LUAEXT_API void luaE_shallowcopytable(lua_State *L, int idx);

/*
 * Two functions below create a table with strictly ordered integer keys
 * with no holes i.e k_(i+1) = k_i + 1.
 * Values are supplied from array and hash parts of the source table.
 *
 * NB: metatable of the source table is not copied
 * NB: order of elements is not defined (see Lua Reference Manual).
 *   (which is true for integer keys as well as integer keys can reside
 *    both in array and hash parts).
 */
/*
 * Creates a sequence with source table keys as values and pushes it
 * on stack.
 */
LUAEXT_API void luaE_tablekeys(lua_State *L, int idx);

/*
 * Creates a sequence with source table values as values and pushes it
 * on stack.
 */
LUAEXT_API void luaE_tablevalues(lua_State *L, int idx);

/*
 * Creates a table with keys equal to values of the array part to the first nil
 * of source table, values equal to 'true' and pushes it on stack.
 */
LUAEXT_API void luaE_tabletoset(lua_State *L, int idx);

/*
 * Creates a deep copy of table at `idx` in `from` state and pushes it on the
 * top of a stack of `to` state.
 * Table may contain only non-gcv, strings, tables or Lua functions without
 * upvalues and accesses to globals.
 */
LUAEXT_API void luaE_deepcopytable(lua_State *to, lua_State *from, int idx);

/* Coverage public API. */

#define LUAE_COV_SUCCESS 0
#define LUAE_COV_ERROR   1

typedef void (*lua_Coveragewriter) (void *context, const char *lineinfo,
				    size_t size);

LUAEXT_API int luaE_coveragestart(lua_State *L, const char *filename,
				  const char **excludes, size_t num);
LUAEXT_API int luaE_coveragestart_cb(lua_State *L, lua_Coveragewriter cb,
				     void *context, const char **excludes,
				     size_t num);
LUAEXT_API int luaE_coveragestop(lua_State *L);

/* Public API for platform-level timer interrupts. */

#define LUAE_INT_SUCCESS 0
#define LUAE_INT_ERR     1

/*
 * Global initialisation of timer interrupts. Singal with the number signo will
 * be used to deliver interrupts to the process. This function must be called
 * prior to usage of any facilities provided by the API for coroutine timeouts.
 * Returns LUAE_INT_SUCCESS on success, LUAE_INT_ERR otherwise.
 */
LUAEXT_API int luaE_intinit(int signo);

/*
 * Global termination of timer interrupts. Termination is performed only if the
 * timer interrupts were initialised.Facilities provided by the API for
 * coroutine timeouts must not be used after calling this function.
 * Returns LUAE_INT_SUCCESS on success, LUAE_INT_ERR otherwise.
 */
LUAEXT_API int luaE_intterm(void);

/*
 * Returns a non-0 value if a time value has resolution greater than or equal
 * to the one provided by the timer interrupts. Otherwise returns 0.
 */
LUAEXT_API int luaE_intresolvable(const struct timeval *timeout);

/* Public API for coroutine timeouts. */

#define LUAE_TIMEOUT_SUCCESS    0
#define LUAE_TIMEOUT_ERRBADTIME 1 /* Malformed timeout value. */
#define LUAE_TIMEOUT_ERRNOTICKS 2 /* Timer interrupts are switched off. */
#define LUAE_TIMEOUT_ERRHOOK    3 /* Inside a hook. */
#define LUAE_TIMEOUT_ERRABORT   4 /* Coroutine in a non-runnable state. */
#define LUAE_TIMEOUT_ERRMAIN    5 /* Main coroutine of the VM. */
#define LUAE_TIMEOUT_ERRRUNNING 6 /* Already running, no restart requested. */

/*
 * Sets a timeout for the coroutine L. If the restart flag is set to a non-zero
 * value, the new timeout value is applied immediately. On success, returns
 * LUAE_TIMEOUT_SUCCESS. Otherwise returns one of LUAE_TIMEOUT_ERR* status codes.
 * If the coroutine terminates because of timeout, lua_resume returns
 * LUAE_TIMEOUT status. Such coroutines cannot be resumed.
 */
LUAEXT_API int luaE_settimeout(lua_State *L,
			       const struct timeval *timeout, int restart);

/*
 * Sets a new function to be called in case of coroutine timeout and returns the
 * old one. If a coroutine terminates because of timeout, the timeout function
 * timeoutf is called in the context of the coroutine before its stack is
 * unwound. Currently a call to timeoutf is not protected.
 */
LUAEXT_API lua_CFunction luaE_settimeoutf(lua_State *L, lua_CFunction timeoutf);

/*
 * Calls function openf with string modname as an argument and sets the call
 * result in package.loaded[modname], as if that function has been called
 * through require. Leaves a copy of that result on the stack. This function
 * implements a subset of luaL_requiref available since Lua 5.2. To be
 * deprecated once uJIT becomes fully 5.2-compatible.
 */
LUAEXT_API void luaE_requiref(lua_State *L,
			      const char *modname, lua_CFunction openf);

/*
 * Extensions to the standard Lua libraries:
 */

#define LUAE_BITLIBNAME  "bit"
LUALIB_API int luaopen_bit(lua_State *L);

#define LUAE_JITLIBNAME  "jit"
LUALIB_API int luaopen_jit(lua_State *L);

#define LUAE_FFILIBNAME  "ffi"
LUALIB_API int luaopen_ffi(lua_State *L);

#define LUAE_UJITLIBNAME "ujit"
LUALIB_API int luaopen_ujit(lua_State *L);

/* Backwards compatibility */

#define luaext_HashF luae_HashF
#define LUAEXT_HASHF_DEFAULT LUAE_HASHF_DEFAULT
#define LUAEXT_HASHF_MURMUR LUAE_HASHF_MURMUR
#define LUAEXT_HASHF_CITY LUAE_HASHF_CITY

#define luaext_Options luae_Options
#define LUA_TIMEOUT LUAE_TIMEOUT
#define lua_Metrics luae_Metrics

#define LUA_ITER_BEGIN LUAE_ITER_BEGIN
#define LUA_ITER_END LUAE_ITER_END

#define LUA_PROFILE_SUCCESS LUAE_PROFILE_SUCCESS
#define LUA_PROFILE_ERR     LUAE_PROFILE_ERR
#define LUA_PROFILE_ERRMEM  LUAE_PROFILE_ERRMEM
#define LUA_PROFILE_ERRIO   LUAE_PROFILE_ERRIO

#define LUA_COV_SUCCESS LUAE_COV_SUCCESS
#define LUA_COV_ERROR   LUAE_COV_ERROR

#define LUA_INT_SUCCESS LUAE_INT_SUCCESS
#define LUA_INT_ERR     LUAE_INT_ERR

#define LUA_TIMEOUT_SUCCESS    LUAE_TIMEOUT_SUCCESS
#define LUA_TIMEOUT_ERRBADTIME LUAE_TIMEOUT_ERRBADTIME
#define LUA_TIMEOUT_ERRNOTICKS LUAE_TIMEOUT_ERRNOTICKS
#define LUA_TIMEOUT_ERRHOOK    LUAE_TIMEOUT_ERRHOOK
#define LUA_TIMEOUT_ERRABORT   LUAE_TIMEOUT_ERRABORT
#define LUA_TIMEOUT_ERRMAIN    LUAE_TIMEOUT_ERRMAIN
#define LUA_TIMEOUT_ERRRUNNING LUAE_TIMEOUT_ERRRUNNING

#define LUA_BITLIBNAME  LUAE_BITLIBNAME
#define LUA_JITLIBNAME  LUAE_JITLIBNAME
#define LUA_FFILIBNAME  LUAE_FFILIBNAME
#define LUA_UJITLIBNAME LUAE_UJITLIBNAME

#endif /* !_LEXTLIB_H */
