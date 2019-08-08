.. _ujit-024:

|PROJECT| 0.24 Lua API Reference (devel)
========================================

.. contents:: :local:

Functions
---------

``ujit.getmetrics``
^^^^^^^^^^^^^^^^^^^

.. code-block:: lua

    local metrics = ujit.getmetrics()

Returns a table with the current values of |PROJECT|-specific metrics. The table has following keys:

   ==================== ================================================================================================
   Key                  Description
   ==================== ================================================================================================
   strnum               Current number of ``string`` objects.
   tabnum               Current number of ``table`` objects.
   udatanum             Current number of ``userdata`` objects.
   gc_total             Current number of bytes used by non-sealed objects and all strings (both sealed and non-sealed).
   gc_sealed            Current number of sealed objects excluding strings.
   gc_freed             Number of freed bytes since the last retrieval of metrics.
   gc_allocated         Number of allocated bytes since the last retrieval of metrics.
   gc_steps_pause       Number of GC's ``pause`` phases since the last retrieval of metrics.
   gc_steps_propagate   Number of GC's ``propagate`` phases since the last retrieval of metrics.
   gc_steps_atomic      Number of GC's ``atomic`` phases since the last retrieval of metrics.
   gc_steps_sweepstring Number of GC's ``sweepstring`` phases since the last retrieval of metrics.
   gc_steps_sweep       Number of GC's ``sweep`` phases since the last retrieval of metrics.
   gc_steps_finalize    Number of GC's ``finalize`` phases since the last retrieval of metrics.
   jit_snap_restore     Number of snapshot restorations since the last retrieval of metrics.
   strhash_hit          Number of hits to the internal string storage since the last retrieval of metrics.
   strhash_miss         Number of misses to the internal string storage since the last retrieval of metrics.
   ==================== ================================================================================================

``ujit.immutable``
^^^^^^^^^^^^^^^^^^

.. code-block:: lua

   local value = ujit.immutable(value)

Makes an object immutable and returns a reference to it for convenience. See :ref:`here <sealing-public>` for more details.

``ujit.seal``
^^^^^^^^^^^^^

.. code-block:: lua

   ujit.seal(obj)

Recursively seals ``obj``. Throws a run-time error if sealing could not be finalized. In case of any errors, the state of ``obj`` is guaranteed to be the same as it was prior to the call to this interface. See :ref:`here <sealing-public>` for more details.

``ujit.usesfenv``
^^^^^^^^^^^^^^^^^

.. code-block:: lua

   local uses_fenv = ujit.usesfenv(func)

Checks if a function ``func`` uses its environment. Following logic applies:

- For regular Lua functions, returns ``true`` if the function meets at least one of following conditions (and ``false`` otherwise):

    - It references at least one global variable.
    - It references at least one upvalue.

- For built-in functions, always returns ``false``.
- For registered C functions, always returns ``true``.

Modules
-------

ujit.coverage
^^^^^^^^^^^^^

``start``
"""""""""

.. code-block:: lua

   local started = ujit.coverage.start(filename[, excludes])

Starts platform-level coverage counting and streams output to ``filename``. ``excludes`` array with regexps can be optionally passed to exclude filenames from coverage output. Returns ``true`` on success and ``false`` on any error.

``stop``
""""""""

.. code-block:: lua

   ujit.coverage.stop()

Stops platform-level coverage counting. Does nothing if coverage was not enabled. Does not have a return value.

``pause``
"""""""""

.. code-block:: lua

    ujit.coverage.pause()

Pauses streaming of coverage counting output into file. Does nothing if coverage was not enabled or was already paused. Does not have a return value.

``unpause``
"""""""""""

.. code-block:: lua

   ujit.coverage.unpauses()

Unpauses streaming of coverage counting output info file. Does nothing if coverage was not enabled or was already unpaused. Does not have a return value.

ujit.debug
^^^^^^^^^^

``gettableinfo``
""""""""""""""""

.. code-block:: lua

   local info = ujit.debug.gettableinfo(table)

Returns a table ``info`` containing internal characteristics of table. ``info`` provides following fields:

    ========= ===============================================================================
    Field     Description
    ========= ===============================================================================
    acapacity Capacity of the array part of table
    asize     Number of elements stored in the array part of table
    hcapacity Capacity of the hash part of table
    hmaxchain Length of the longest chain in the hash part of table
    hgchains  Number of chains in the hash part of table (1-element chains are included, too)
    hsize     Number of elements stored in the hash part of table
    ========= ===============================================================================

ujit.dump
^^^^^^^^^

``bc``
"""""""

.. code-block:: lua

   ujit.dump.bc(io_object, func)

Dumps bytecode of the function ``func`` to ``io_object``. Throws an error if ``io_object`` is not of appropriate type or if ``func`` is not a function. Does not have a return value.

``bcins``
"""""""""

.. code-block:: lua

   local dumped = ujit.dump.bcins(io_object, func, pc[, nest_level])

Dumps ``pc``-th bytecode of the function ``func`` to ``io_object``. ``pc`` is 0-based. If ``nest_level`` is specified, prepends the output with corresponding indentation. Throws an error if ``io_object`` is not of appropriate type or if ``func`` is not a function. Returns ``true`` if data was dumped, and ``false`` otherwise.

``mcode``
"""""""""

.. code-block:: lua

   ujit.dump.mcode(io_object, trace_no)

Dumps machine code for the trace ``trace_no`` to ``io_object``. Throws an error if ``io_object`` is not of appropriate type. Does not have a return value.

``stack``
"""""""""

.. code-block:: lua

   ujit.dump.stack(io_object)

Dumps the Lua stack of currently executed coroutine to ``io_object``. If any error occurs, dumps nothing. Never throws a run-time error.

``start``
""""""""""

.. code-block:: lua

   local started, fname_real = ujit.dump.start([fname_stub])

Starts dumping the progress of the JIT compiler to ``fname_stub`` suffixed with some random extension. ``started`` is set to ``true`` if dumping was started, and ``false`` otherwise. The resulting dump file name is returned to ``fname_real`` if dumping was actually started. If ``fname_stub`` is omitted or passed as ``"-"``, dumping is started to standard output, and ``fname_real`` is set to ``"-"``, too.

``stop``
""""""""

.. code-block:: lua

   local stopped = ujit.dump.stop()

Stops dumping the progress of the JIT compiler. Returns true if stop was successful, and false otherwise.

``trace``
"""""""""

.. code-block:: lua

   ujit.dump.trace(io_object, trace_no)

Dumps IR for the trace ``trace_no`` to ``io_object``. Throws an error if ``io_object`` is not of appropriate type. Does not have a return value.

ujit.math
^^^^^^^^^

All functions in this module treat arguments as standard Lua's ``math`` functions:

    - All extra arguments are ignored.
    - For the first argument, all non-number values except
       coercible strings throw.
    - Coercible strings are coerced to numbers and
       corresponding conversion results are processed regularly.
       Strings "nan", "infinity", "inf", "+inf", "-inf" (all
       case-insensitive) are **coercible**.
    - Non-coercible strings throw.

``isfinite``
""""""""""""

.. code-block:: lua

   local isfinite_x = ujit.math.isfinite(42) -- true
   local isfinite_y = ujit.math.isfinite(math.huge) -- false

Returns ``false``  if number is NaN, negative or positive infinity. Returns ``true``  otherwise.

``isinf``
"""""""""

.. code-block:: lua

   local isinf_x = ujit.math.isinf(42) -- false
   local isinf_y = ujit.math.isinf(math.huge) -- true
   local isinf _z= ujit.math.isinf(-math.huge) -- true

Returns ``true``  if number is positive or negative infinity and ``false``  otherwise.

``isnan``
"""""""""

.. code-block:: lua

   local isnan_x = ujit.math.isnan(42) -- false
   local isnan_y = ujit.math.isnan(math.huge) -- false
   local isnan_z = ujit.math.isnan(ujit.math.nan) -- true
   local isnan_z2 = ujit.math.isnan(0 / 0) -- true

Returns ``true``  is number is NaN and ``false``  otherwise.

``isninf``
""""""""""

.. code-block:: lua

   local isninf_x = ujit.math.isninf(42) -- false
   local isninf_y = ujit.math.isninf(math.huge) -- false
   local isninf_z = ujit.math.isninf(-math.huge) -- true

Returns ``true``  if number is negative infinity and ``false`` otherwise.

``ispinf``
""""""""""

.. code-block:: lua

   local ispinf_x = ujit.math.ispinf(42) -- false
   local ispinf_y = ujit.math.ispinf(math.huge) -- true
   local ispinf_z = ujit.math.ispinf(-math.huge) -- false

Returns ``true`` if number is negative infinity and ``false`` otherwise.

``nan``
"""""""

.. code-block:: lua

   assert(ujit.math.isnan(ujit.math.nan))
   assert(ujit.math.nan ~= ujit.math.nan) -- NaN is not equal to itself or any other number

A constant for representing IEEE 754 NaN.

ujit.memprof
^^^^^^^^^^^^

``start``
""""""""""

.. code-block:: lua

   local started, fname_real = ujit.memprof.start(interval, fname_stub)

Starts memory profiling for ``interval`` seconds. If ``interval`` is 0, profiling runs until ``ujit.memprof.stop`` is called. Data is being streamed to ``fname_stub`` suffixed with some random extension. ``started`` is set to ``true`` if profiling was started, and ``false`` otherwise. Upon successful start, the resulting full profile file name is returned in ``fname_real``.

``stop``
""""""""

.. code-block:: lua

   local stopped = ujit.memprof.stop()

Stops memory profiling started by ``ujit.memprof.start``. Returns ``true`` on success and ``false`` otherwise.

ujit.profile
^^^^^^^^^^^^^

``available``
"""""""""""""

.. code-block:: lua

   local available = ujit.profile.available()

Returns true if |PROJECT|-level profiler is available, and false otherwise.

``init``
""""""""

.. code-block:: lua

   local initialized = ujit.profile.init()

Returns ``true`` if |PROJECT|-level profiler was successfully initialized, and ``false`` otherwise. Profiler cannot be used prior to initialization.

``start``
"""""""""

.. code-block:: lua

   local started, fname_real = ujit.profile.start(interval, mode[, fname_stub])

Starts profiling in mode with sampling interval (expressed in microseconds). Depending on the mode, may stream profile data to fname_stub suffixed with some random extension. started is set to true if profiling was started, and false otherwise. The resulting full profile file name is returned in fname_real if applicable (see below). Supported values for mode are:


   =============== =============================================================================================================================================
   Value           Description
   =============== =============================================================================================================================================
   ``"default"``   Collects only lightweight in-memory per-VM state profile. ``fname_stub`` is ignored, ``fname_real`` is always set to ``nil``.
   ``"leaf"``      Collects leaf profile. ``fname_stub`` must be specified. If profiling was started, the profile will be streamed to ``fname_real``.
   ``"callgraph"`` Collects full call-graph profile. ``fname_stub`` must be specified. If profiling was started, the profile will be streamed to ``fname_real``.
   =============== =============================================================================================================================================

``stop``
""""""""

.. code-block:: lua

   local counters[, err_reason] = ujit.profile.stop()

On success, stops profiling and returns a table with in-memory VM counters. On failure, returns ``nil`` as the first argument and an error reason string as the second argument.

``terminate``
"""""""""""""

.. code-block:: lua

   local terminated = ujit.profile.terminate()

Returns ``true`` if |PROJECT|-level profiler was successfully terminated, and ``false`` otherwise. Profiler cannot be used after termination.

ujit.string
^^^^^^^^^^^

``trim``
""""""""

.. code-block:: lua

    local s = ujit.string.trim("  \t\n  hello   \r\n") -- "hello"

Removes whitespace from both ends of the string.

``split``
"""""""""

.. code-block:: lua

    local t = {}
    for token in ujit.string.split("a,b,c", ",") do
        table.insert(t)
    end
    -- t == { "a", "b", "c" }

Returns an iterator that can be used in ``for`` loops. The separator should be non-empty plain text string (characters like ``\n`` and ``\0`` are supported for separators). Treats consecutive separators as if they have empty token between them, for example:

.. code-block:: lua

    local t = {}
    for token in ujit.string.split("a,,c", ",") do
        table.insert(t, token)
    end
    -- t == { "a", "", "c" }

Separators at the beginning/end of the string are treated as if there was an empty token in the beginning/end of the string, for example:

.. code-block:: lua

    local t = {}
    for token in ujit.string.split(",a,,c,", ",") do
        table.insert(t, token)
    end
    -- t == { "", "a", "", "c", "" }

ujit.table
^^^^^^^^^^^

``keys``
""""""""

.. code-block:: lua

    local new_table = ujit.table.keys(table)

Returns a new table with source ``table`` keys as values. Metatable of the table is not copied. Throws a runtime error in case the argument is not a table. Implementation detail (not guaranteed in future versions): Returned table is a sequence.

``rindex``
""""""""""

.. code-block:: lua

    local table = {x = {y = {z = "foo"}}}
    local value1 = ujit.table.rindex(table, "x", "y", "z") -- foo
    local value2 = ujit.table.rindex(table, "x", "A", "z") -- nil

Indexes ``table`` "recursively". If the look up fails at some point, returns ``nil`` without raising an error. Respects metamethods.

``shallowcopy``
"""""""""""""""

.. code-block:: lua

   local new_table = ujit.table.shallowcopy(table)

Returns a shallow copy of ``table``. Metatable of the table is not copied. Throws a runtime error in case the argument is not a table.

``size``
"""""""""

.. code-block:: lua

   local table = {1, 2, nil, 3, key = "value" }
   print(ujit.table.size(t)) -- 4

Returns number of non-nil elements in a table (both array and hash part).

``toset``
""""""""""

.. code-block:: lua

   local new_table = ujit.table.toset(table)

Returns a new table with source ``table`` values as keys and values set to ``true``. Metatable of the table is not copied. Throws a runtime error in case the argument is not a table. 

``values``
""""""""""

.. code-block:: lua

   local new_table = ujit.table.values(table)

Returns a new table with source ``table`` values as values. Metatable of the table is not copied. Throws a runtime error in case the argument is not a table.

Implementation detail (not guaranteed in future versions): Returned table is a sequence.
