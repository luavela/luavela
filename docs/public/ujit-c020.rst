.. _ujit-c020:

|PROJECT| 0.20 C API Reference
==============================

.. contents:: :local:

Header Files
------------

``<ujit/lextlib.h>``
^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    #include <ujit/lextlib.h>

Main header file for |PROJECT|'s extended C API. Please include it to access all functions, data structures and constants documented below.

Functions
----------

``luaE_coveragestart``
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_coveragestart(lua_State *L, const char *filename, const char **excludes, size_t num);

Starts platform-level coverage counting for the state ``L`` and dumps output into ``filename``. Regexps for excluding files from coverage can be passed with ``excludes``, ``num`` corresponds to the number of passed regexps. Returns ``LUA_COV_SUCCESS`` on success and ``LUA_COV_ERROR`` in case of any error.

``luaE_coveragestart_cb``
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_coveragestart_cb(lua_State *L, lua_Coveragewriter cb, void *context, const char **excludes, size_t num);

Same as ``luaE_coveragestart``, but outputs through provided ``lua_Coveragewriter`` callback. 

``luaE_coveragestop``
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_coveragestop(lua_State *L)

Stops platform-level coverage counting for the state ``L``. Returns ``LUA_COV_SUCCESS`` on success and ``LUA_COV_ERROR`` in case of any error.

``luaE_dumpbc``
^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_dumpbc(lua_State *L, int idx, FILE *out);

Dumps the byte code of the functional object located at ``idx`` to ``out``. If ``idx`` is not a functional object, does nothing.

``luaE_dumpstart``
^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_dumpstart(const lua_State *L, FILE *out);

Starts dumping JIT compiler's progress to ``out``. Returns 0 if dumping was started successfully, and a non-0 value otherwise.

``luaE_dumpstop``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_dumpstop(const lua_State *L);

Stops dumping JIT compiler's progress. Returns 0 if dumping was started successfully, and a non-0 value otherwise.

``luaE_getdataroot``
^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_getdataroot(lua_State *L);

For the regular state ``L``, pushes data state's data root on top of ``L``'s stack. See also ``luaE_setdataroot``.

``luaE_immutable``
^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_immutable(lua_State *L, int idx);

Makes an object at ``idx`` immutable. See :ref:`here <spec-sealing>` for details.

``luaE_intinit``
^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_intinit(int signo);

Global initialization of timer interrupts. Signal with the number ``signo`` will be used to deliver interrupts to the process with some pre-defined interval. Returns ``LUA_INT_SUCCESS`` on success, ``LUA_INT_ERR`` otherwise (e.g. initialization is already performed). This function must be called prior to usage of any facilities provided by the API for coroutine timeouts.

``luaE_intresolvable``
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_intresolvable(const struct timeval *timeout);

Returns a non-0 value if a ``timeout`` value has resolution greater than or equal to the one provided by the timer interrupts. Otherwise returns 0.


``luaE_intterm``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_intterm(void);

Global termination of timer interrupts. Termination is performed only if the timer interrupts were initialized. Returns ``LUA_INT_SUCCESS`` on success, ``LUA_INT_ERR`` otherwise. Facilities provided by the API for coroutine timeouts must not be used after calling this function.

``luaE_iterate``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    uint64_t luaE_iterate(lua_State *L, int idx, uint64_t iter_state);

Pushes on stack the next key-value pair from the table stored at ``idx`` and returns a new value of the internal iterator state for subsequent calls. If the entire table is traversed, does not touch the stack and returns ``LUA_ITER_END``. For the first invocation, ``iter_state`` must be set to ``LUA_ITER_BEGIN``. Please note that the calling code must not use ``iter_state`` as well as the return value for anything but passing it back to this function.

Usage example:

.. code-block:: c

    uint64_t iter = LUA_ITER_BEGIN;
    while ((iter = luaE_iterate(L, index, iter)) != LUA_ITER_END) {
        /* Key is located at index -2 (2nd top-most element on the stack) */
        /* Value is located at index -1 (the top-most element on the stack) */
        lua_pop(L, 2); /* remove key-value pair from the stack before the next iteration */
    } 

``luaE_metrics``
^^^^^^^^^^^^^^^^

.. code-block:: c

    struct lua_Metrics luaE_metrics (lua_State *L);

Returns a structure containing numerous runtime metrics of the state. Please find the definition of ``struct lua_Metrics`` in the Types section.

``luaE_newdataslave``
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    lua_State *luaE_newdataslave(lua_State *datastate);

Creates a new Lua state which uses ``datastate`` for accessing the global data feed.

``luaE_profavailable``
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_profavailable(void);

Returns ``LUA_PROFILE_SUCCESS`` if profiling is available and ``LUA_PROFILE_ERR`` otherwise.

``luaE_profinit``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_profinit(void);

Global profiler initialization. Returns ``LUA_PROFILE_SUCCESS`` on success, ``LUA_PROFILE_ERR`` otherwise (e.g. initialization is already performed). This function must be called prior to usage of any other facilities provided by the profiler (except ``luaE_profavailable``).

``luaE_profterm``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_profterm(void);

Global profiler termination. Termination is performed only if the profiler was initialized and is in a non-running state at the time of the call.  Returns ``LUA_PROFILE_SUCCESS`` on success, ``LUA_PROFILE_ERR`` otherwise. No other facilities provided by the profiler must be used after calling this function (except ``luaE_profavailable`` and ``luaE_profinit``).

``luaE_seal``
^^^^^^^^^^^^^

.. code-block:: c

    void luaE_seal(lua_State *L, int index);

Recursively seals a value at the given acceptable index. The value must be a table, string, function or function prototype. For the function, its prototype is also sealed. For the table, all keys, values and array slots are also sealed. Attempt to seal a function with upvalues results in an error.

``luaE_setdataroot``
^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_setdataroot(lua_State *L, int idx);

For the data state ``L``, sets the table at ``idx`` as its data root. See also ``luaE_getdataroot``.

``luaE_settimeout``
^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_settimeout(lua_State *L, const struct timeval *timeout, int restart);

Sets a ``timeout`` for the coroutine ``L``. If the ``restart`` flag is set to a non-zero value, the new ``timeout`` value is applied immediately. Returns ``LUA_TIMEOUT_SUCCESS`` on success, and one of ``LUA_TIMEOUT_ERR*`` status codes otherwise (see below). If coroutine terminates because of timeout, lua_resume returns LUA_TIMEOUT status. Such coroutines cannot be resumed.

``luaE_settimeoutf``
^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    lua_CFunction luaE_settimeoutf(lua_State *L, lua_CFunction timeoutf);

Sets a new function to be called in case of coroutine timeout and returns the old one. If a coroutine terminates because of timeout, the timeout function ``timeoutf`` is called in the context of the coroutine before its stack is unwound. Currently a call to ``timeoutf`` is not protected. ``timeoutf`` can return any fixed number of arguments (i.e. ``LUA_MULTRET`` cannot be returned).

``luaE_shallowcopytable``
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_shallowcopytable(lua_State *L, int idx);

Creates a shallow copy of a table at ``idx`` and pushes it on stack. Metatable of the table is not copied. Throws a runtime error in case an element at ``idx`` is not a table.

``luaE_tablekeys``
^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_tablekeys(lua_State *L, int idx);

Creates a new table from table at ``idx`` with source table keys as values and pushes it on stack. Metatable of the table is not copied. Throws a runtime error in case an element at ``idx`` is not a table. Implementation detail (not guaranteed in future versions): Created table is a sequence.

``luaE_tabletoset``
^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_tabletoset(lua_State *L, int idx);

Creates a new table from table at ``idx`` with source table values as keys and values set to ``true`` and pushes it on stack. Metatable of the table is not copied. Throws a runtime error in case an element at ``idx`` is not a table.

``luaE_tablevalues``
^^^^^^^^^^^^^^^^^^^^
     
.. code-block:: c

    void luaE_tablevalues(lua_State *L, int idx);

Creates a new table from table at ``idx`` with source table values as values and pushes it on stack. Metatable of the table is not copied. Throws a runtime error in case an element at ``idx`` is not a table. Implementation detail (not guaranteed in future versions): Created table is a sequence.

``luaE_totalmem``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    size_t luaE_totalmem(void);

Returns a total number of bytes requested by |PROJECT|'s allocator from operating system.

``luaE_usesfenv``
^^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaE_usesfenv(lua_State *L, int idx);

Checks if a function at ``idx`` uses its environment. Following logic applies:

    - For regular Lua functions, returns a non-zero value if the function meets at least one of following conditions (and 0 otherwise):

      - It references at least one global variable.
      - It references at least one upvalue.

    - For built-in functions, always returns 0.
    - For registered C functions, always returns a non-zero value.

``luaE_verstring``
^^^^^^^^^^^^^^^^^^

.. code-block:: c

    const char *luaE_verstring(void);

Returns a string describing current |PROJECT| version.

``luaopen_bit``
^^^^^^^^^^^^^^^

.. code-block:: c

    int luaopen_bit(lua_State *L);

Opens the ``bit`` library, an extension to the Lua standard libraries. This function is called by ``luaL_openlibs`` as well, so no need to call it explicitly if you use ``luaL_openlibs``.

``luaopen_ffi``
^^^^^^^^^^^^^^^

.. code-block:: c

    int luaopen_ffi(lua_State *L);

Opens the ``ffi`` library, an extension to the Lua standard libraries. This function is called by ``luaL_openlibs`` as well, so no need to call it explicitly if you use ``luaL_openlibs``.

``luaopen_jit``
^^^^^^^^^^^^^^^

.. code-block:: c

    int luaopen_jit(lua_State *L);

Opens the ``jit`` library, an extension to the Lua standard libraries. This function is called by ``luaL_openlibs`` as well, so no need to call it explicitly if you use ``luaL_openlibs``.

``luaopen_ujit``
^^^^^^^^^^^^^^^^

.. code-block:: c

    int luaopen_ujit(lua_State *L);

Opens the ``ujit`` library, an extension to the Lua standard libraries. This function is called by ``luaL_openlibs`` as well, so no need to call it explicitly if you use ``luaL_openlibs``.

Types
------

``lua_Coveragewriter``
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   typedef void (*lua_Coveragewriter) (void *context, const char *lineinfo, size_t size);

Callback for streaming line information in platform-level coverage counting. Should accept three arguments: pointer to callback-specific context, ``const char`` pointer to coverage ``lineinfo`` message and size of the message.

``lua_Metrics``
^^^^^^^^^^^^^^^

.. code-block:: c

    struct _Metrics {
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

Various runtime metrics.

Extended Thread Statuses
------------------------

``LUA_TIMEOUT``
^^^^^^^^^^^^^^^

Returned by ``lua_resume`` in case of coroutine timeout.

Constants
----------

``LUA_BITLIBNAME``
^^^^^^^^^^^^^^^^^^

Name of the ``bit`` library: ``"bit"``.

``LUA_COV_ERROR``
^^^^^^^^^^^^^^^^^

Generic error code for platform-level coverage counting.

``LUA_COV_SUCCESS``
^^^^^^^^^^^^^^^^^^^

Generic success code for platform-level coverage counting.

``LUA_FFILIBNAME``
^^^^^^^^^^^^^^^^^^

Name of the ``ffi`` library: ``"ffi"``.

``LUA_INT_ERR``
^^^^^^^^^^^^^^^

Generic error code for timer interrupts.

``LUA_INT_SUCCESS``
^^^^^^^^^^^^^^^^^^^

Generic success code for timer interrupts.

``LUA_ITER_BEGIN``
^^^^^^^^^^^^^^^^^^

Initial iterator state for ``luaE_iterate``, should be passed on the first call to the function.

``LUA_ITER_END``
^^^^^^^^^^^^^^^^

Final iterator state for ``luaE_iterate``, returned when table traversal is finished.

``LUA_JITLIBNAME``
^^^^^^^^^^^^^^^^^^

Name of the ``jit`` library: ``"ujit"``.

``LUA_PROFILE_ERR``
^^^^^^^^^^^^^^^^^^^

Generic error code for profiler's interfaces.

``LUA_PROFILE_ERRIO``
^^^^^^^^^^^^^^^^^^^^^

I/O error occurred during profiling.

``LUA_PROFILE_ERRMEM``
^^^^^^^^^^^^^^^^^^^^^^

Memory error occurred during profiling.

``LUA_PROFILE_SUCCESS``
^^^^^^^^^^^^^^^^^^^^^^^

Generic success code for profiler's interfaces.

``LUA_TIMEOUT_ERRABORT``
^^^^^^^^^^^^^^^^^^^^^^^^

Coroutine is in a non-runnable state at the time of the call to ``luaE_settimeout``. For example, you try to set a timeout for a coroutine which was resumed and threw error during execution.

``LUA_TIMEOUT_ERRBADTIME``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Malformed ``const struct timeval *timeout`` was passed to ``luaE_settimeout``. Currently following values are considered malformed:

   - ``NULL``
   - non-``NULL`` pointer pointing to a struct with at least one member with a negative value

``LUA_TIMEOUT_ERRHOOK``
^^^^^^^^^^^^^^^^^^^^^^^

Coroutine is inside a Lua hook callback at the time of the call to ``luaE_settimeout``.

``LUA_TIMEOUT_ERRMAIN``
^^^^^^^^^^^^^^^^^^^^^^^

Attempt to set a timeout for the main coroutine of the Lua VM.

``LUA_TIMEOUT_ERRNOTICKS``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Timer interrupts were not initialized prior to call to ``luaE_settimeout``. See also ``luaE_intinit``.

``LUA_TIMEOUT_ERRRUNNING``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Attempt to set a timeout for a coroutine which is in a running state and already has a timeout, which is prohibited by default. To allow this behavior, set the ``restart`` flag of ``luaE_settimeout`` to a non-0 value.

``LUA_TIMEOUT_SUCCESS``
^^^^^^^^^^^^^^^^^^^^^^^

Generic success code for timeout-related interfaces.

``LUA_UJITLIBAME``
^^^^^^^^^^^^^^^^^^

Name of the ``ujit`` library: ``"ujit"``.
