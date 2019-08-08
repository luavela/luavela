.. _spec-interruptible-coroutines:

Interruptible Coroutines
========================

.. contents:: :local:

.. note::

    This document is under active development. It might and probably will change substantially in the future.

Interface
---------

.. code-block:: c

   LUAEXT_API int luaE_setinterrupt(lua_State *L, const struct timeval *timeout);

Returns ``LUAE_TIMEOUT_SUCCESS`` on success and fails in following cases:

    - ``timeout`` is ``NULL`` or contains negative values;
    - ``timeout`` is not resolvable (i.e. it is less than the resolution of the platform-wide timer); The only exception is the effective value of ``0``.
    - Interrupts are not initialized with ``luaE_intinit``;
    - The interface is called from a hook;
    - The status of ``L`` is **not** one of: 0, ``LUA_YIELD``
    - ``L`` is the "main" state of the virtual machine (e.g.  the one created by ``lua_open()``).

In case of failures, utilizes exactly the same error codes as ``luaE_settimeout``.

Once the timeout is set, the coroutine must be resumed with ``lua_resume``. When it is interrupted, ``lua_resume`` will return ``LUA_INTERRUPT`` status. An interrupted coroutine can be resumed over and over until it returns any value other than ``LUA_INTERRUPT``.

Notes and Caveats
-----------------

- While interrupted, the coroutine's stack can be traversed;
- Do not try to ``lua_call`` anything in the context of the interrupted coroutine;
- Although ``lua_resume`` returns ``LUA_INTERRUPT`` in case of interrupts, ``lua_status`` still returns ``LUA_YIELD``;
- ``luaE_setinterrupt`` can be invoked from inside the running coroutine which will either prolong or shorten its quantum "on the fly";
- ``luaE_setinterrupt`` can be invoked on an already interrupted coroutine, which will affect the coroutine once it is resumed;
- ``luaE_setinterrupt`` and ``luaE_settimeout`` are independent, i.e. you can start a coroutine with a quantum of 10 ms with ``luaE_setinterrupt`` **and** make it expire after say 80ms with ``luaE_setttimeout`` (80ms here is a wall-clock time before expiration – this time "ticks" while your coroutine is interrupted);
- To disable interrupts, pass a pointer to ``struct timeval no_interrupts = {0};``;

Implementation Notes
--------------------

For this feature, |PROJECT| reuses the mechanism of "timeout checkpoints", which already used for ``luaE_settimeout``. This means that if either of following is true, a coroutine will not be interrupted (= interrupt checks are just disabled):

- Error function (the second argument of ``xpcall``) is running.
- Timeout function (see ``luaE_settimeoutf``) is running.
- A hook is being executed.
- A userdata finalizer (``__gc`` metamethod) is being executed.
- A Lua callback is being executed from a C/C++ API (think "no checks past a C-call boundary").
- JIT compiler is active.
