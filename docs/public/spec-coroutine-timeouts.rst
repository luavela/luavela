.. _spec-coroutine-timeouts:

|PROJECT| Coroutine Timeouts
============================

.. contents:: :local:

Introduction
------------

The Lua language provides user-space `coroutines <https://www.lua.org/pil/9.html>`__ as a mechanism for implementing multitasking. Lua coroutines are cooperative by design, which means that unless a coroutine willingly yields control to its resumer, it can run forever. However, sometimes there is a need to have coroutines that will run no longer than a given timespan (with the accuracy no better than provided by modern non-real-time OSes). This article discusses implementation details of this feature in |PROJECT| (aka "|PROJECT|-level coroutine timeouts").

Key Requirements
----------------

Here is a set of requirements formulated for the feature implementation within |PROJECT| 0.13 (not a fully formalized list though – some terms may be defined implicitly, wording may sometimes be inexact):

1. Expiration timeouts must be implemented on coroutine level (not on e.g. function level). I.e. the concept of "expiration timespan" is only applicable to entities that can be resumed (coroutines) and is not relevant to entities that can be called (functions).
2. Lua-level interface for the feature is not required. C-level interface is required.
3. In ideal world, a coroutine that executes beyond its expiration timespan, must yield control to its resumer and stay in the resumable state after that. However, this is too difficult to implement given Lua's embeddable nature. That said, following behaviour is enough:
4. **Execution interrupt on expiration.** A coroutine that executes beyond its expiration timespan (let's call it an "expired coroutine"), raises a dedicated run-time error (let's call it a "timeout exception").
5. **Timeout function execution.** Before the timeout exception is handled, there must be an option to call a ``lua_CFunction`` (let's call it a "timeout function", may be different for different coroutines) in the context of the expired coroutine. If the call happens (i.e. if some timeout function is set for the coroutine), there must be a guarantee that the stack of the expired coroutine is not modified between throwing the timeout exception and invocation of the timeout function. The timeout function may return any number of arguments, but this number must be fixed: ``LUA_MULTRET`` must not be returned.
6. **Error handling, clean-up and returning control.** The timeout exception must be caught and handled at the same stack level which resumed the expired coroutine. Both host and guest stacks must be unwound up to this level and all resources must be freed. Control must be transferred to the code immediately following the code that resumed the expired coroutine. After clean-up and stack unwinding are performed, top slots of the Lua stack must contain values returned by the timeout function.
7. Timeout checking mechanism must be implemented on the interpreter level, and as lightweight as possible: usage of system calls and library functions is highly discouraged.

Description of the Implementation
----------------------------------

First, following auxiliary subsystems are introduced:

-  Signal-based timers: a wrapper around POSIX timers which deliver timer events via a configurable signal number.
-  Timer interrupts: A signal-based timer with a payload (inside a signal handler) which counts ticks (i.e. increments some global user-space variable) with some interval (``TIMERINT_INTERVAL_USEC``). Think ``jiffies`` in the Linux kernel.

With this in place, some byte-codes are equipped with an extra semantics that checks if a currently running coroutine has expired. If it has, a timeout exception is thrown in the context of the running coroutine. Otherwise the semantics of the byte-code is executed normally. The byte-codes that check for expiration timeout (aka "timeout check points") are documented below. One can see that "|PROJECT| jiffies" are user-space with this implementation, so checking for a timeout does not involve leaving user-space or a call to some library function.

Finally, following interface-level changes are implemented (only most important changes are listed, see `API Reference <pub-api-reference>`_ for more details):

1. There is an extended C-level interface for specifying an expiration timeout for a coroutine. Just for the record, coroutine are supposed to be resumed normally via ``lua_resume``, coroutines with a timeout set cannot be resumed from Lua via ``coroutine.resume``.
2. There is an extended C-level interface for setting a timeout function for a coroutine.
3. If coroutine does not expire, ``lua_resume`` behaves according to the `Reference Manual <https://www.lua.org/manual/5.1/>`_.
4. Otherwise the behaviour is according to the feature requirements (see above): a timeout exception is thrown, a timeout function is called (it was set for the expired coroutine), stack unwinding and clean-up are performed. In this case, ``lua_resume`` returns an extended coroutine status ``LUAE_TIMEOUT``.
5. Coroutines that returned ``LUAE_TIMEOUT``, can no longer be resumed.

If you do not want to use this feature at all, please compile |PROJECT| with ``UJIT_ENABLE_CO_TIMEOUT`` set to ``OFF``.

Timeout Check Points
^^^^^^^^^^^^^^^^^^^^

    -  **Before 0.16:** ``BC_CALL*``: ``BC_CALL``, ``BC_CALLM``, ``BC_CALLT``, ``BC_CALLMT``
    -  **As of 0.16:** ``BC_*FUNC*``: ``BC_FUNCF``, ``BC_FUNCV``, ``BC_JFUNCF``, ``BC_IFUNCF``, ``BC_IFUNCV``, ``BC_FUNCC``, ``BC_FUNCCW``
    -  ``BC_RET*``: ``BC_RET``, ``BC_RET0``, ``BC_RET1``, ``BC_RETM``
    -  Byte codes for loops:

       -  ``BC_*LOOP``: ``BC_LOOP``, ``BC_ILOOP``
       -  ``BC_*FORI``: ``BC_JFORI``, ``BC_FORI``
       -  ``BC_*FORL``: ``BC_FORL``, ``BC_JFORL``, ``BC_IFORL``
       -  ``BC_*ITERL``: ``BC_ITERL``, ``BC_JITERL``, ``BC_IITERL``
       -  ``BC_*ITRNL``: ``BC_ITRNL``, ``BC_JITRNL``, ``BC_IITRNL``

    -  ``vm_*`` interfaces: ``vm_returnc``, ``vm_resume``
    -  ``coroutine.*`` library: ``coroutine.resume``, ``coroutine.yield``

Implementation Limitations and Caveats
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

    -  Timeouts are checked only for coroutine body functions (and their subsequent callees), which implies that:

       -  Timeouts are not checked while a timeout function is executed in the coroutine's context (the one you set with ``luaE_settimeoutf``). In other words, ``LUAE_TIMEOUT`` cannot be re-thrown once it has been thrown.
       -  Timeouts are not checked while an error function is executed in the coroutine's context (think ``xpcall``'s error functions).
       -  Timeouts are not checked while any hook is executed in the coroutine's context.
       -  As of |PROJECT|  0.16, timeouts are not checked while a ``__gc`` finalizer is executed in the coroutine's context.
       -  As of |PROJECT|  0.15.1 / 0.16.1, timeouts are not checked past C-call boundaries.

    -  As a special case and as of |PROJECT| 0.16, timeouts are not checked during normal coroutine execution if the JIT compiler is active.
    -  Currently timeouts are checked only at the VM level, which implies that:

       -  Timeouts are **not** checked while an arbitrary C code is executed by the VM. In particular, timeouts are not tracked in ``lua_yield``.
       -  Timeouts are **not** checked while a JIT-compiled code is executed.

    -  If some coroutine A, which is started with an expiration timeout, starts another coroutine B with an expiration timeout, each coroutine will use its own expiration timeout value. It means that wall-clock execution time for the coroutine A may significantly exceed its expiration timeout.
    -  Expiration timeout cannot be set while a coroutine executes hooks.
    -  Timeout function is currently called in unprotected mode, which allows to set no limits on the number of values returned from it. This complies with the initial requirements, but implies some limitations on the implementation of the timeout function: Be careful to use only protected calls from it to handle possible run-time errors correctly.

Usage Example
^^^^^^^^^^^^^

.. code-block:: c

    #include <stdio.h>
    #include <signal.h>
    #include <sys/time.h>

    #include "lua.h"
    #include "lauxlib.h"
    #include "lextlib.h"

    static const struct timeval timeout = {
        .tv_sec  = 0,
        .tv_usec = 200000 /* 0.2 sec */
    };

    int main(int argc, char **argv)
    {
        lua_State *L = lua_open();
        lua_State *L1;
        size_t i;

        luaL_openlibs(L);

        /* Initialize timer interrupts and load the chunk */
        if (luaE_intinit(SIGPROF) != LUAE_INT_SUCCESS)
            return 1;

        if (luaL_dofile(L, lua_chunk) != 0) /* path to some Lua chunk */
            return 2;

        L1 = lua_newthread(L);
        if (luaE_settimeout(L1, &timeout, 0) != 0)
            return 3;
     
        luaE_settimeoutf(L, timeout_callback); /* must be a lua_CFunction defined elsewhere */
     
        /*
         * Assume coroutine's body function accepts an integer
         * as its only arguments and is called coroutine_start
         */
        lua_getfield(L1, LUA_GLOBALSINDEX, "coroutine_start");
        lua_pushinteger(L1, (lua_Integer)10);

        if (lua_resume(L1, 1) == LUAE_TIMEOUT)
            fprintf(stderr, "TIMEOUT\n"); /* Return values of timeout_callback are on the stack */

        /* more action here */

        if (luaE_intterm() != LUAE_INT_SUCCESS)
            return 4;

        lua_close(L);
        return 0;
    }
