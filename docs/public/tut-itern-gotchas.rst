.. _tut-itern-gotchas:

Tutorial: ``next`` and ``ITERN`` Gotchas
========================================

.. contents:: :local:

Introduction
------------

This article gathers bits and pieces around the mega-task of compiling generic for loops.

Semantics of the Generic for Loop
---------------------------------

According to the `Language Reference Manual <https://www.lua.org/manual/5.1/>`_, a *generic for* loop

.. code-block:: lua

    for var_1, ..., var_n in explist do
        block
    end

is equivalent to:

.. code-block:: lua

    do
        local f, s, var = explist
        while true do
            local var_1, ..., var_n = f(s, var)
            var = var_1
            if var == nil then
                break
            end
            block
        end
    end

Translating to Bytecode: General Case
--------------------------------------

In most general case, generic ``for`` loops are translated into:

.. code::

    ...         ; Fetch f, s and var somehow (e.g. make a CALL to pairs)
    JMP =>iter  ; Enter the loop: jump to the first call to the iterator
    body:
    ; ...
    ; block
    ; ...
    iter:
    HOTCNT         ; Bump counter for the hot code
    ITERC          ; Call the iterator via regular VM calling convention (i.e. unstash f, s, var and CALL)
    ITERL =>body   ; Inspect iterator's return values:
                ; If the first variable is nil, fall through (= exit the loop).
                ; Otherwise update var and execute loop body once.

Please note:

    -  ``ITERC`` is recorded, like any other ``CALL``-like
       instruction (in fact, ``ITERC`` belongs to the ``CALL``
       family of byte codes).
    -  ``ITERL`` is recorded, it is one of the entry points to
       the compiled code. Technically, we enter the trace in
       ``JLOOP``, and ``JITERL`` is a thin wrapper around it.

Translating to Bytecode: Specialized Case
-----------------------------------------

Whenever built-in ``next`` is used as an iterator (directly or via ``pairs``), generic ``for`` loops are translated into:

.. code::

    ...           ; Fetch f, s and var somehow (e.g. make a CALL to pairs)
    ISNEXT =>iter ; Check that the actual iterator is built-in next:
               ; If it is not true, *dynamically* change ITERN to ITERC
               ; falling back to the general case.
               ; JMP =>iter.
    body:
    ; ...
    ; block
    ; ...
    iter:
    HOTCNT         ; Bump counter for the hot code
    ITERN          ; 1) Advance to the next element in s
                ; 2) If there are no more elements, JMP =>end_of_loop
                ; 3) Otherwise update var and JMP =>body
    ITERL =>body   ; Not called
    end_of_loop:

Please note:

    - Emission of ``JMP``/``ITERC`` vs. ``ISNEXT``/``ITERN`` is done by the front end based on some heuristics (see ``lj_parse.c``).
    - ``ITERN`` is de facto an alternative VM-side implementation of the built-in ``next`` function.
    - ``ISNEXT`` is not recorded.
    - ``ITERN`` is not recorded.
    - Additionally, ``tab_next`` in ``lj_tab.c`` implements approximately the same semantics as ``ITERN``, which are used e.g. in ``luaE_iterate``.

``next`` as a Built-in Function
--------------------------------

Apart from the ``ITERN`` byte-code, the platform obviously provides the ``next`` function itself:

    - ``next`` is a fast function implemented in assembly (look for something like ``.ffunc_1 next`` in VM code).
    - ``next`` is not recorded.
    - ``next`` uses (slow) ``lj_tab_next`` to iterate over the table. (**TODO:** Can we switch it to the faster ``lj_tab_iterate``, by the way?)

``ISNEXT`` and Magic Constant ``0xfffe7fff``
--------------------------------------------

When ``ISNEXT`` ensures that it is ok to iterate with ``ITERN``, it prepares the stack slot for the control variable in a special manner (**TODO:** Can we dump it accordingly?):

.. code::

    MSB                              LSB
        64 bits      32 bits  32 bits
    +----------------+--------+--------+
    |        LJ_TNUMX|fffe7fff| payload|
    +----------------+--------+--------+

Now, when ``ITERN`` traverses the table, only 32 ``payload`` bits are de facto used to store either an ``index to the array part`` or ``asize + index to the hash part``.

There is, however, a very rare use case for the magic constant ``0xfffe7fff``: whenever ``ITERN`` is despecialized to ``ITERC`` on the fly, it is used as a signal to convert the control variable to the ``lj_tab_next``-compatible form. To understand this, please play with following code:

.. code-block:: lua

    local t = { foo = 9, bar = 10, 4, 5, 6 }
    local r = {}

    local function dummy() end

    local function f(next)
        for k, v in next, t, nil do
            r[#r + 1] = k
            if v == 5 then
                f(dummy)
            end
        end
    end

    f(next)
    assert(#r == 5)

More Context
------------

    -  An attempt to record ``next`` (the built-in) for LuaJIT: https://blog.cloudflare.com/luajit-hacking-getting-next-out-of-the-nyi-list/.
