.. _pub-running-ujit:

Running |PROJECT|
=================

.. note::

   The internal name of |PROJECT| in IPONWEB is |PRJ_INT|. These two along with respective CLI names may be used interchangeably in the documentation for historical reasons.

.. note::

   This manual is effective as of |PROJECT| 0.7 release. Please beware: Although it originates from `LuaJIT's CLI guide <http://luajit.org/running.html>`__, current set of CLI options supported by |PROJECT| differs significantly. For older |PROJECT| releases, please use the aforementioned LuaJIT's guide.

.. note::

   Prior to |PROJECT| 0.15 the name of the executable was ``luajit``.

General CLI Options
-------------------

|PROJECT| has only a single stand-alone executable, called |CLI_BIN|. It can be used to run simple Lua statements or whole Lua applications from the command line. It has an interactive mode, too.

The |CLI_BIN| stand-alone executable is just a slightly modified version of the regular ``lua`` stand-alone executable. It supports the same basic options, too. ``ujit -h`` prints a short list of the available options. Please have a look at the `Lua 5.1 Reference Manual <https://www.lua.org/manual/5.1/>`_ for details. The sections below cover additional options provided by |PROJECT| .

|PROJECT| Interpreter Options
-----------------------------

-b (dump bytecode)
^^^^^^^^^^^^^^^^^^

.. parsed-literal::

   $ |CLI_BIN| -b output ...

Dumps bytecode of the chunk saving it to the ``output`` file in human readable form. ``output`` parameter is required and cannot be omitted. To dump to ``stdout``, set ``output`` to ``-`` (dash character).

Output file is overwritten on each run.

Typical usage examples:

.. parsed-literal::

   $ |CLI_BIN| -b test.out test.lua                  # List bytecode to test.out
   $ |CLI_BIN| -b test.out -e "print('hello world')" # List cmdline script's bytecode to test.out
   $ |CLI_BIN| -b- test.lua                          # List bytecode to stdout
   $ |CLI_BIN| -b- -e "print('hello world')"         # List cmdline script's bytecode to stdout

-B (dump bytecode with source)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. parsed-literal::

   $ |CLI_BIN| -B output script_file.lua

Similar to ``-b``, but also prints a source code which corresponds to each bytecode (similar to 'disassembly /s' in gdb). If command line chunk is given instead of a script file, prints a warning and dumps bytecode as ``-b``.

|PROJECT|  General Compiler Options
-----------------------------------

|PROJECT| provides a number of options for manipulating JIT compiler's behaviour. General syntax is following:

.. parsed-literal::

   $ |CLI_BIN| -j cmd[=arg[,arg...]] ...

This option performs a |PROJECT| control command or activates one of the loadable extension modules. The command is first looked up in the ``jit.*`` library. If no matching function is found, a module named ``jit.<cmd>`` is loaded and the ``start()`` function of the module is called with the specified arguments (if any). The space between ``-j`` and ``cmd`` is optional.

Control Commands
^^^^^^^^^^^^^^^^

.. parsed-literal::

   $ |CLI_BIN| -jcmd

Following commands are supported:

.. container:: table-wrap

   ========= ======================================================
   Command   Description
   ========= ======================================================
   ``on``    Turns the JIT compiler on (default).
   ``off``   Turns the JIT compiler off (only use the interpreter).
   ``flush`` Flushes the whole cache of compiled code.
   ========= ======================================================

Dumping Compiler Progress
^^^^^^^^^^^^^^^^^^^^^^^^^

.. parsed-literal::

   $ |CLI_BIN| -p output ...

Executes a chunk and dumps compiler's progress as the execution moves on. For each trace, following information is dumped:

   -  Bytecode corresponding to the recorded trace.
   -  IR of the trace, along with emitted snapshots and information about allocated host registers (if available, i.e. if trace recording was not aborted).
   -  Machine code of the trace (if available, i.e. if trace recording was not aborted).
   -  If trace recoding was aborted, specifies the abort reason.
   -  If the trace exited during script execution, exit state is dumped.

``output`` parameter is required and cannot be omitted. To dump to ``stdout``, set ``output`` to ``-`` (dash character). Output file is overwritten on each run.

|PROJECT| Compiler Optimization Options
---------------------------------------

.. parsed-literal::

   $ |CLI_BIN| -O[level] ...
   $ |CLI_BIN| -O[+]flag -O-flag ...
   $ |CLI_BIN| -Oparam=value ...

This options allows fine-tuned control of the optimizations used by the JIT compiler. This is mainly intended for debugging |PROJECT| itself. Please note that the JIT compiler is extremely fast (we are talking about the microsecond to millisecond range). Disabling optimizations doesn't have any visible impact on its overhead, but usually generates code that runs slower.

The first form sets an optimization level — this enables a specific mix of optimization flags. ``-O0`` turns off all optimizations and higher numbers enable more optimizations. Omitting the level (i.e. just ``-O``) sets the default optimization level, which is ``-O3`` in the current version. Optimizations specific to`` -O4`` are either experimental or known to work improperly.

From within Lua code optimization level can be set by the same integer values passed as a first argument to ``jit.opt.start(<optimization_level>, ...).``

The second form adds or removes individual optimization flags. The third form sets a parameter for the VM or the JIT compiler to a specific value.

You can either use this option multiple times (like ``-Ocse -O-dce -Ohotloop=10``) or separate several settings with a comma (like ``-O+cse,-dce,hotloop=10``). The settings are applied from left to right and later settings override earlier ones. You can freely mix the three forms, but note that setting an optimization level overrides all earlier flags.

To set individual option flags from within Lua code, define each one as a separate argument to
``jii.opt.start`` e.g.``jit.opt.start("-sink", "+loop").`` 

Here are the available flags and at what optimization levels they are enabled: 

.. container:: table-wrap

   ============= ====== ====== ===== ===== ===== =======================================================
    Flag         -01    -02    -03   -04   WIP   Description
   ============= ====== ====== ===== ===== ===== =======================================================
   ``fold``       ✅     ✅     ✅    ✅         Constant Folding, Simplifications and Reassociation
   ``cse``        ✅     ✅     ✅    ✅         Common-Subexpression Elimination
   ``dce``        ✅     ✅     ✅    ✅         Dead-Code Elimination
   ``narrow``            ✅     ✅    ✅         Narrowing of numbers to integers
   ``loop``              ✅     ✅    ✅         Loop Optimizations (code hoisting)
   ``fwd``                      ✅    ✅         Load Forwarding (L2L) and Store Forwarding (S2L)
   ``dse``                      ✅    ✅         Dead-Store Elimination
   ``abc``                      ✅    ✅         Array Bounds Check Elimination
   ``sink``                     ✅    ✅         `Allocation Sinking Optimization <http://wiki.luajit.org/Allocation-Sinking-Optimization>`__
   ``fuse``                                 ❗   Fusion of operands into instructions. This optimization is currently a no-op in |PROJECT| at the moment.
   ``nohrefk``                         ✅        Disables emission of the ``HREFK`` IR instruction. Available since |PROJECT| 0.10.
   ``noretl``                          ✅        Disables recording of returns to lower Lua frames. Available since |PROJECT| 0.10.
   ``jitcat``                          ✅        Enables compilation of concatenation. Available since |PROJECT| 0.11.
   ``jittabcat``                       ✅        Enables compilation of table.concat. Available since |PROJECT| 0.20.
   ``jitstr``                          ✅        Enables compilation of string.find, string.lower, string.upper. Available since |PROJECT| 0.20.
   ``movtv``                                ❗   Optimizes copying data between tables. Available since |PROJECT| 0.23.
   ``movtvpri``                             ❗   Same as ``movtv``, but for recording-time ``nil``, ``false`` and ``true`` values. Available since |PROJECT| 0.24.
   ``jitpairs``                             ❗   Enables compilation of 'pairs' and 'next'. Available since |PROJECT| 0.22, but is known to produce incorrect results sometimes. Work on fix in progress.
   ============= ====== ====== ===== ===== ===== =======================================================

Notes:

   -  ``-O3`` is the default set of optimizations provided by LuaJIT
   -  ``-O4`` is ``-O3`` plus the set of optimizations specific to |PROJECT|

Here are the parameters and their default values:

.. container:: table-wrap

   ============== ======= ===============================================================================================
   Parameter      Default Description
   ============== ======= ===============================================================================================
   ``maxtrace``   1000    Maximum number of traces in the cache
   ``maxrecord``  4000    Maximum number of recorded IR instructions
   ``maxirconst`` 500     Maximum number of IR constants of a trace
   ``maxside``    100     Maximum number of side traces of a root trace
   ``maxsnap``    500     Maximum number of snapshots for a trace
   ``hotloop``    56      Number of iterations to detect a hot loop or hot call
   ``hotexit``    10      Number of taken exits to start a side trace
   ``tryside``    4       Number of attempts to compile a side trace
   ``instunroll`` 4       Maximum unroll factor for instable loops
   ``loopunroll`` 15      Maximum unroll factor for loop ops in side traces
   ``callunroll`` 3       Maximum unroll factor for pseudo-recursive calls
   ``recunroll``  2       Minimum unroll factor for true recursion
   ``sizemcode``  64      Size of each machine code area in KBytes (In LuaJIT default value is 32. This might be important for comparing JIT performance)
   ``maxmcode``   8192    Maximum total size of all machine code areas in KBytes (In LuaJIT default value is 512. This might be important for comparing JIT performance)
   ============== ======= ===============================================================================================

.. warning::

   Unlike LuaJIT, |PROJECT| does *not* support ``-Onodce`` syntax for optimization flags, use ``-O-dce`` for switching certain optimizations off.

Extended Configuration Options
------------------------------

.. note::

   This section applies to |PROJECT| 0.21 and above.

|PROJECT| supports extended configuration options in the form of

.. parsed-literal::

   $ |CLI_BIN| -X opt1=value1 -X opt2=value2

When CLI is invoked, these options are read first and are used for creating an according virtual machine instance.

Supported options:

.. container:: table-wrap

   ========= =============================================================== ======================= ====================
   Option    Description                                                     Supported Values        Availability
   ========= =============================================================== ======================= ====================
   ``hashf`` Hashing function used for interning strings across the platform -  ``murmur`` (default) Since |PROJECT| 0.21
                                                                             -  ``city``
   ``itern`` Enables ITERN optimization in frontend                          -  ``on`` (default)     Since |PROJECT| 0.22
                                                                             -  ``off``
   ========= =============================================================== ======================= ====================
