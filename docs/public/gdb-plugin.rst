gdb Plugin for Debugging
========================

.. _gdb-plugin:

|PROJECT| provides gdb plugin for better visualization of |PROJECT|'s data structures to simplify debugging.

Installation and usage
-----------------------

Just run the following script in gdb:

.. code::

        (gdb) source /path/to/ujit/gdb/ujit-gdb.py

Note that this should be done on each run of gdb. If you don't want to do this every time, add the line above to ``~/.gdbinit``.

Available commands
------------------

.. note::

    Information about command usage can be obtained in gdb by calling ``help func`` i.e. ``help uj-stack``.

uj-stack[-vm] *L*
^^^^^^^^^^^^^^^^^^

Dumps Lua stack of the given coroutine ``L``.

.. code::

        (gdb) uj-stack 0x7ffff7fd8378
        [WARNING] Bad VM state: INTERP, must be CFUNC or LFUNC
        0x7ffff7fe8e70:0x7ffff7fe8e30 [    ] 5 slots: Red zone
        0x7ffff7fe8e20                [   M]
        0x7ffff7fe8e10:0x7ffff7fe8b80 [    ] 42 slots: Free stack slots
        0x7ffff7fe8b70                [  T ]
        0x7ffff7fe8b60                [    ] VALUE: fast function 19
        0x7ffff7fe8b50                [ B  ] VALUE: nil
        0x7ffff7fe8b40                [    ] FRAME: [L] delta=5, fast function 2
        0x7ffff7fe8b30                [    ] VALUE: string: 0x7ffff7fda590 "Hello, here is my string"
        0x7ffff7fe8b20                [    ] VALUE: table 0x7ffff7feb8e8
        0x7ffff7fe8b10                [    ] VALUE: thread 0x7ffff7fd9cf8
        0x7ffff7fe8b00                [    ] VALUE: table 0x7ffff7fd9ca8
        0x7ffff7fe8af0                [    ] FRAME: [L] delta=3, Lua function 0x7ffff7fe58f0, upvalues 1, 0x7ffff7fda4d8 "@/home/ibondarev/calls.lua":1
        0x7ffff7fe8ae0                [    ] VALUE: number: 141

Output is similar to ``uj_dump_stack`` function from |PROJECT|.

If execution is stopped inside the VM (e.g. break point was set on lj_BC_CALL), call ``uj-stack-vm`` instead (since v0.21).

uj-bc (since v0.22)
^^^^^^^^^^^^^^^^^^^

Dumps byte codes and corresponding source code of a currently executing frame. Highlights the byte code which is about to execute with "->".

Looks up ``L`` in current frame and uses it as a ``lua_State*`` if stopped in non-VM frame. If there's no ``L`` in current frame, specify it explicitly as in uj-stack command.

.. code::

            Breakpoint 1, 0x000055555563f996 in lj_BC_CALL ()
            (gdb) uj-bc
            <...>
            -- BYTECODE -- /home/jsmith/script.lua:0-15
            9    end
                0001    FNEW     0   0      ; /home/jsmith/script.lua:1
            11    local str = string.reverse("hello")
                0002    GGET     1   1      ; "string"
                0003    TGETS    1   1   2  ; "reverse"
                0004    KSTR     2   3      ; "hello"
            -> 0005    CALL     1   2   2
            12    local x = ujit.table.size({ 1, 2, nil, 3})
                0006    GGET     2   4      ; "ujit"
                0007    TGETS    2   2   5  ; "table"
                0008    TGETS    2   2   6  ; "size"
                0009    TDUP     3   7
                0010    CALL     2   2   2
            13    print(test(x))
            <...>

Note that in contrast to ``uj-stack``, there's no separate command if you want to dump byte codes when execution stopped inside the VM.

uj-tv *addr*
^^^^^^^^^^^^

.. code::

        (gdb) uj-tv L->base - 2
        string: 0x7ffff7fda590 "Hello, here is my string"

uj-tab *addr*
^^^^^^^^^^^^^

Dumps the contents of the ``GCtab`` at **addr**.

.. code::

            (gdb) uj-tab 0x7ffff7fd9ca8
            Array part:
            [0] nil
            [1] number: 11
            [2] string: 0x7ffff7fda590 "Hello, here is my string"
            [3] false
            [4] true
            [5] Lua function 0x7ffff7fec458, upvalues 0, 0x7ffff7fda4d8 "@/home/johnsmith/calls.lua":17
            [6] userdata 0x7ffff7fdf6f0
            [7] thread 0x7ffff7fd9cf8
            [8] table 0x7ffff7fec400
            [9] true
            [10] nil
            [11] cdata 0x7ffff7fe7bc8
            [12] nil
            [13] nil
            [14] nil
            [15] nil
            [16] nil
            Hash part:
            { false } => { Lua function 0x7ffff7fec648, upvalues 0, 0x7ffff7fda4d8 "@/home/jsmith/calls.lua":28 } next = 0x0
            { table 0x7ffff7fec5f8 } => { number: 11 } next = 0x7ffff7fecac8
            { nil } => { nil } next = 0x0
            { nil } => { nil } next = 0x0
            { nil } => { nil } next = 0x0
            { nil } => { nil } next = 0x0
            { Lua function 0x7ffff7fec6a0, upvalues 0, 0x7ffff7fda4d8 "@/home/jsmith/calls.lua":25 } => { string: 0x7ffff7fe5818 "str" } next = 0x0
            { string: 0x7ffff7fe5d40 "3" } => { false } next = 0x0
            { nil } => { nil } next = 0x0
            { nil } => { nil } next = 0x0
            { string: 0x7ffff7fe6458 "key1" } => { table 0x7ffff7fec6d0 } next = 0x0
            { thread 0x7ffff7fd9cf8 } => { true } next = 0x0
            { string: 0x7ffff7fe6480 "9" } => { true } next = 0x0
            { true } => { userdata 0x7ffff7fdf6f0 } next = 0x7ffff7feca50
            { userdata 0x7ffff7fdf6f0 } => { thread 0x7ffff7fd9cf8 } next = 0x7ffff7fecaa0
            { string: 0x7ffff7fe64a8 "11" } => { number: 15 } next = 0x0

``{ nil } => { nil } next = 0x0`` lines indicate unused slots in a hash part of a table and should  be ignored.

uj-str *addr*
^^^^^^^^^^^^^

.. code::

            (gdb) uj-str ((TValue *)0x7ffff7fe8b30)->gcr
            string: 0x7ffff7fda590 "Hello, here is my string"

uj-global-state *L*
^^^^^^^^^^^^^^^^^^^

Shows current VM state.

.. code::

            (gdb) uj-global-state L
            VM state: INTERP
            GC state: PAUSE
