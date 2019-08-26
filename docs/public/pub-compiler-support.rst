.. _pub-compiler-support:

|PROJECT| Compiler Support
==========================

.. contents:: :local:

Introduction
------------

.. note::

     This page originates from the `LuaJIT 2.0 NYI <http://wiki.luajit.org/NYI>`_ reference.

All aspects of Lua are implemented in interpreter, but not all of them are implemented in JIT compiler. This page serves as a quick reference to identify whether certain things are compiled or not.

It's not a stated goal to compile everything, since the interpreter is fast enough for many tasks. And speed doesn't matter for anything that's done only a couple of times during the runtime of a program. E.g. it would be absolutely pointless to compile ``require()`` or ``module()`` (in fact, these should eventually be rewritten as plain Lua functions).

However, the number of JIT-compiled cases will grow over time, based on demand and user feedback.

The following tables provide an indication whether a feature is JIT-compiled or not:

     - **yes**: Always JIT-compiled.
     - **partial**: May be JIT-compiled, depending on the circumstances. Otherwise will fall back to the interpreter.
     - **no**: Not JIT-compiled (yet), will always fall back to the interpreter.
     - **never**: Ditto. Will not be JIT-compiled, even in future versions.

Bytecode
--------

Almost all bytecodes are compiled, except for these:

.. list-table::
     :header-rows: 1

     * - Bytecode
       - Compiled?
       - Remarks
     * - ``CALLT``
       - partial
       - Tailcall. Some tailcalls to frames lower than the starting frame of the trace are not compiled.
     * - ``CAT``
       - partial
       - Concatenation operator ``..``. Compiled with ``-Ojitcat``. ``__concat`` metamethod is not compiled.
     * - ``FNEW``
       - no
       - Create closure.
     * - ``FUNC*``
       - partial
       - Call built-in function. See below.
     * - ``ISNEXT``
       - **yes**
       - Check for ``next()`` loop optimization. Compiled with ``-Ojitpairs`` enabled (since 0.22).
     * - ``ITERN``
       - partial
       - Optimized call to ``next()`` in a loop. Compiled with ``-Ojitpairs`` enabled except for the last iteration (since 0.22).
     * - ``RET*``
       - partial
       - Return from function. Returns to C frames and some returns to frames lower than the starting frame of the trace are not compiled.
     * - ``TSETM``
       - no
       - Initialize table with multiple return values. Compiled in LuaJIT 2.1.
     * - ``UCLO``
       - no
       - Close upvalues.
     * - ``VARG``
       - partial
       - Vararg operator ``...``. Multi-result ``VARG`` is only compiled when used with ``select()``.

Notes:

     -  Table accesses to mixed dense/sparse tables are not compiled.
     -  Bytecode execution that would cause an error in the interpreter is never compiled.

Libraries
---------

The following tables list whether or not **calls** to the various built-in library functions will get compiled. This may depend on the arguments passed (esp. their types) and the exact circumstances of the call.

Base Library
^^^^^^^^^^^^

.. container:: table-wrap

     ============== ========= ==============================================================================================================
     Function       Compiled? Remarks
     ============== ========= ==============================================================================================================
     assert         **yes**
     collectgarbage no
     dofile         never
     error          never
     gcinfo         no
     getfenv        partial   Only ``getfenv(0)`` is compiled.
     getmetatable   **yes**
     ipairs         **yes**
     load           never
     loadfile       never
     loadstring     never
     module         never
     newproxy       never
     next           partial   Compiled with ``-Ojitpairs`` except for the case when the last element is passed as a second arg (since 0.22).
     pairs          **yes**   Compiled with ``-Ojitpairs`` (since 0.22).
     pcall          **yes**
     print          no
     rawequal       **yes**
     rawget         **yes**
     rawlen         **yes**
     rawset         **yes**
     require        never
     select         partial   Only compiled when the first argument is a constant.
     setfenv        no
     setmetatable   **yes**
     tonumber       partial   Won't compile for bases other than 10, other exceptions apply.
     tostring       partial   Only compiled for strings, numbers, booleans, nil, and values with a ``__tostring`` metamethod.
     type           **yes**
     unpack         no
     xpcall         **yes**
     ============== ========= ==============================================================================================================

String Library
^^^^^^^^^^^^^^

.. container:: table-wrap

     ============== ========= ================================================================================
     Function       Compiled? Remarks
     ============== ========= ================================================================================
     string.byte    **yes**
     string.char    no        Compiled in LuaJIT 2.1.
     string.dump    never
     string.find    partial   Compiled for plain string searches (no patterns) with ``-Ojitstr`` (since 0.20).
     string.format  partial   Compiled for non-%p and non-string arguments for %s.
     string.gmatch  no
     string.gsub    no
     string.len     **yes**
     string.lower   **yes**   Compiled with ``-Ojitstr`` (since 0.20).
     string.match   no
     string.rep     no        Compiled in LuaJIT 2.1.
     string.reverse no        Compiled in LuaJIT 2.1.
     string.sub     **yes**
     string.upper   **yes**   Compiled with ``-Ojitstr`` (since 0.20).
     ============== ========= ================================================================================

Table Library
^^^^^^^^^^^^^

.. container:: table-wrap

     ============== ========= ===============================================================
     Function       Compiled? Remarks
     ============== ========= ===============================================================
     table.concat   **yes**   Compiled with ``-Ojittabcat`` (since 0.20).
     table.foreach  never     Deprecated in Lua 5.1. Use pairs() instead.
     table.foreachi never     Deprecated in Lua 5.1. Use ipairs() instead, which is compiled.
     table.getn     **yes**
     table.insert   partial   Only when pushing.
     table.maxn     no
     table.pack     no
     table.remove   partial   Only when popping; compiled in LuaJIT 2.1.
     table.sort     no
     table.unpack   no
     ============== ========= ===============================================================

Math Library
^^^^^^^^^^^^

.. container:: table-wrap

     =============== ========= ==================
     Function        Compiled? Remarks
     =============== ========= ==================
     math.abs        **yes**
     math.acos       **yes**   Since 0.23.
     math.asin       **yes**   Since 0.23.
     math.atan       **yes**   Since 0.23.
     math.atan2      **yes**   Since 0.23.
     math.ceil       **yes**
     math.cos        **yes**
     math.cosh       **yes**   Since 0.23.
     math.deg        **yes**
     math.exp        **yes**
     math.floor      **yes**
     math.fmod       no
     math.frexp      no
     math.huge       **yes**
     math.ldexp      **yes**
     math.log        **yes**
     math.log10      **yes**
     math.max        **yes**
     math.min        **yes**
     math.mod        no        Same as math.fmod.
     math.modf       no
     math.pi         **yes**
     math.pow        **yes**
     math.rad        **yes**
     math.random     **yes**
     math.randomseed no
     math.sin        **yes**
     math.sinh       **yes**   Since 0.23.
     math.sqrt       **yes**
     math.tan        **yes**
     math.tanh       **yes**   Since 0.23.
     =============== ========= ==================

IO Library
^^^^^^^^^^

.. container:: table-wrap

     ========== ========= =======
     Function   Compiled? Remarks
     ========== ========= =======
     io.close   no
     io.flush   no
     io.input   no
     io.lines   no
     io.open    no
     io.output  no
     io.popen   no
     io.read    no
     io.tmpfile no
     io.type    no
     io.write   no
     ========== ========= =======

Bit Library
^^^^^^^^^^^

.. container:: table-wrap

     =========== ========= =======================
     Function    Compiled? Remarks
     =========== ========= =======================
     bit.arshift **yes**
     bit.band    **yes**
     bit.bnot    **yes**
     bit.bor     **yes**
     bit.bswap   **yes**
     bit.bxor    **yes**
     bit.lshift  **yes**
     bit.rol     **yes**
     bit.ror     **yes**
     bit.rshift  **yes**
     bit.tobit   **yes**
     bit.tohex   no        Compiled in LuaJIT 2.1.
     =========== ========= =======================

FFI Library
^^^^^^^^^^^


.. container:: table-wrap

     ============ ========= ===================================================================
     Function     Compiled? Remarks
     ============ ========= ===================================================================
     ffi.alignof  **yes**
     ffi.abi      **yes**   Since 0.22.
     ffi.cast     partial   Same restrictions as ffi.new (casting is a form of cdata creation).
     ffi.cdef     never
     ffi.copy     **yes**
     ffi.errno    partial   Not when setting a new value.
     ffi.fill     **yes**
     ffi.gc       partial   Not when clearing a finalizer. Compiled in LuaJIT 2.1.
     ffi.istype   **yes**
     ffi.load     never
     ffi.metatype never
     ffi.new      partial   2.0: Not for VLA/VLS, > 8 byte alignment or > 128 bytes.
     ffi.offsetof **yes**
     ffi.sizeof   partial   Not for VLA/VLS types (see below).
     ffi.string   **yes**
     ffi.typeof   partial   Only for cdata arguments. Never for cdecl strings.
     ============ ========= ===================================================================


Coroutine Library
^^^^^^^^^^^^^^^^^

No functions are compiled.

OS Library
^^^^^^^^^^^

No functions are compiled.

Package Library
^^^^^^^^^^^^^^^

No functions are compiled.

Debug Library
^^^^^^^^^^^^^

.. container:: table-wrap


     ================== ========= =======================
     Function           Compiled? Remarks
     ================== ========= =======================
     debug.getmetatable no        Compiled in LuaJIT 2.1.
     debug.\*           no/never  Unlikely to change.
     ================== ========= =======================

JIT Library
^^^^^^^^^^^

No functions are compiled.

|PRJ_INT| Library
^^^^^^^^^^^^^^^^^

.. container:: table-wrap

     ======================= ========= ==============================================================================
     Function                Compiled? Remarks
     ======================= ========= ==============================================================================
     ujit.coverage.pause     never
     ujit.coverage.start     never
     ujit.coverage.stop      never
     ujit.coverage.unpause   never
     ujit.debug.gettableinfo never
     ujit.dump.bc            never
     ujit.dump.bcins         never
     ujit.dump.mcode         never
     ujit.dump.stack         never
     ujit.dump.start         never
     ujit.dump.stop          never
     ujit.dump.trace         never
     ujit.getmetrics         no
     ujit.immutable          **yes**   Since 0.18.
     ujit.iprof.start        no
     ujit.iprof.stop         no
     ujit.math.isfinite      **yes**
     ujit.math.isinf         **yes**
     ujit.math.isnan         **yes**
     ujit.math.isninf        **yes**
     ujit.math.ispinf        **yes**
     ujit.math.nan           **yes**
     ujit.memprof.start      never
     ujit.memprof.stop       never
     ujit.profile.available  never
     ujit.profile.init       never
     ujit.profile.start      never
     ujit.profile.stop       never
     ujit.profile.terminate  never
     ujit.seal               no
     ujit.string.split       no
     ujit.string.trim        **yes**
     ujit.table.keys         **yes**   Since 0.20, via ``IR_CALLL``.
     ujit.table.rindex       partial   Since 0.23, compiles for tables without metatables (including nested lookups).
     ujit.table.shallowcopy  **yes**   Since 0.20, via ``IR_TDUP``.
     ujit.table.size         **yes**   Since 0.22, via ``IR_CALLL``.
     ujit.table.toset        **yes**   Since 0.20, via ``IR_CALLL``.
     ujit.table.values       **yes**   Since 0.20, via ``IR_CALLL``.
     ujit.usesfenv           no
     ======================= ========= ==============================================================================
