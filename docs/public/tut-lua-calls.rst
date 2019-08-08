.. _tut-lua-calls:

Tutorial: Lua-Lua Calls in |PROJECT| Interpreter
================================================

.. contents:: :local:

Introduction
------------

This HOWTO discusses calling Lua functions from Lua code (Lua-Lua calls) and the way they are implemented in the |PROJECT| interpreter. Special attention is paid to implementation of tail calls.

Common Stack Layout and General CALL Semantics
----------------------------------------------

Consider a simple Lua chunk where some Lua function is called from the Lua code:

.. code-block:: lua

      function foo(bar)
            print('bar=' .. tostring(bar))
      end
       
      foo(42)

Corresponding byte code for the chunk is as follows:

.. code::

      $ ujit -b- ./lua-lua-call.lua
      -- BYTECODE -- lua-lua-call.lua:1-3
      0001    GGET     1   0      ; "print"
      0002    KSTR     2   1      ; "bar="
      0003    GGET     3   2      ; "tostring"
      0004    MOV      4   0
      0005    CALL     3   2   2
      0006    CAT      2   2   3
      0007    CALL     1   1   2
      0008    RET0     0   1

      -- BYTECODE -- lua-lua-call.lua:0-7
      0001    FNEW     0   0      ; lua-lua-call.lua:1
      0002    GSET     0   1      ; "foo"
      0003    GGET     0   1      ; "foo"
      0004    KSHORT   1  42
      0005    CALL     0   1   2
      0006    RET0     0   1

In the dump above, one can see a byte code ``CALL`` which obviously performs a call from one function to another. Let's look closer what actually happens when semantics of this byte code is executed. First, here is the expected layout of Lua stack (``RA`` and ``RC`` are operands of the ``CALL`` byte code):

.. code::

      BASE - 1  |  tp  | func | <-- caller's TValue (notation follows declarations in lj_obj.h)
      BASE      |   TValue    | <-- caller's base
      ...       |   TValue    | {
      ...       |   TValue    | {   other slots allocated during caller's execution
      ...       |   TValue    | {
      BASE + RA |   TValue    | <-- callee (LFUNC or an object with __call metamethod), RA holds its stack offset relative to caller's BASE
      ...       |   TValue    | {
      ...       |   TValue    | {   callee's args (RC - 1 slots)
      ...       |   TValue    | {
      TOP       |\\\\\\\\\\\\\| <-- first free slot on the stack

Before we go forward, note the stack slot stored at ``[BASE - 1]``. Its ``func`` is actually a caller which is currently being executed and its ``tp`` holds program counter (``PC``) of the caller's caller. This is |PROJECT|'s standard mechanism of linking stack frames together. Now what happens during ``CALL`` execution:

1. If ``[BASE + RA]`` is not a function object, its ``__call`` metamethod is resolved. If this operation fails, an error is thrown. Otherwise the interpreter proceeds.
2. Caller's ``PC``  is stored to the ``tp`` part of the ``[BASE + RA]`` slot.
3. ``BASE`` is set to ``[BASE + RA + 1]`` effectively pointing to the first argument of the callee.
4. First byte code of the callee gets decoded and dispatched, and actual execution of the callee begins.

In other words, the stack looks like:

.. code::

      OLD_BASE - 1  +-->|  tp' | func'| <-- caller's TValue (tp is a caller caller's PC; used as a frame link)
      OLD_BASE      |   |   TValue    | <-- caller's base
      ...           |   |   TValue    | {
      ...           |   |   TValue    | {   other slots allocated during caller's execution
      ...           |   |   TValue    | {
      BASE - 1      +---|  tp  | func | <-- callee's TValue (tp is a caller's PC; used as a frame link)
      BASE              |   TValue    | <-- callee's base (points to the first argument)
      ...               |   TValue    | {   callee's arguments
      ...               |   TValue    | {   (from 2nd to last)
      TOP               |\\\\\\\\\\\\\| <-- first free slot on the stack

``OLD_BASE`` is not actually stored anywhere and is shown on this figure only for convenience. When the interpreter needs to set ``BASE`` back to ``OLD_BASE`` (e.g. during return from the callee), caller's ``PC`` is used to retrieve the ``CALL`` byte code, read its ``RA`` and set ``BASE`` to the correct value.

``CALL`` is not the only only byte code for calling functions. The others are:

1. ``CALLT``: For tail-calling functions
2. ``CALLM``: For calling functions with variable number of arguments
3. ``CALLMT``: For tail-calling functions with variable number of arguments

All ``CALL*`` byte codes expect the same stack layout as described above, but differ in semantics. In the next section, we'll discuss implementation of tail calls.

Tail calls
----------

Chunk below illustrates the concept of a tail call:

.. code-block:: lua

      function bar(x)
            return x + 40
      end
      function foo(x)
            return bar(x) -- tail call to bar, no foo's code to execute after bar's execution
      end
      print(foo(2))

This chunk results in following byte code:

.. code::

      $ ujit -b- tailcall.lua
      -- BYTECODE -- tailcall.lua:1-3
      0001 KSHORT 1 40
      0002 ADD 1 0 1
      0003 RET1 1 2
      -- BYTECODE -- tailcall.lua:4-6
      0001 GGET 1 0 ; "bar"
      0002 MOV 2 0
      0003 CALLT 1 2
      -- BYTECODE -- tailcall.lua:0-9
      0001 FNEW 0 0 ; tailcall.lua:1
      0002 GSET 0 1 ; "bar"
      0003 FNEW 0 2 ; tailcall.lua:4
      0004 GSET 0 3 ; "foo"
      0005 GGET 0 4 ; "print"
      0006 GGET 1 3 ; "foo"
      0007 KSHORT 2 2
      0008 CALL 1 0 2
      0009 CALLM 0 1 0
      0010 RET0 0 1

As one can see, |PROJECT| emits a special opcode ``CALLT`` to handle a tail call. Its semantics is as follows (initial stack layout is the same as in the section above):

1. ``If [BASE + RA]`` is not a function object, its ``__call`` metamethod is resolved. If this operation fails, an error is thrown. Otherwise the interpreter proceeds.
2. Caller's ``func`` at ``[BASE - 1]`` is overwritten with callee's ``func``. Please note that original caller's ``tp`` is preserved.
3. All callee's arguments are moved from from ``[BASE + RA + 1]`` to ``[BASE]``.
4. First byte code of the callee gets decoded and dispatched, and actual execution of the callee begins.

In other words, original caller's stack gets destroyed during steps 2 and 3. The interpreter does not care about it as the nature of the call (tail call) guarantees that we will never return to the caller, so no need to maintain its stack frame. Besides, the described approach allows us to save some CPU cycles: After tail call finishes, we will use the original caller's ``tp`` to return to the parent (=caller's caller) frame right away instead of performing an extra stack hop in case of preserving caller's stack frame.

The figure below demonstrates the stack layout after ``CALLT`` is executed:

.. code::

            BASE - 1  |  tp  | func | <-- callee's func + original tp (caller caller's PC; used as a frame link)
            BASE      |   TValue    | <-- callee's base (points to the first argument), written over caller's frame
            ...       |   TValue    | {   callee's arguments from 2nd to last
            ...       |   TValue    | {   (written over caller's frame)
            TOP       |\\\\\\\\\\\\\| <-- first free slot on the stack

In the next section we will discuss incompatibility between |PROJECT| and the stock PUC-Rio Lua interpreter caused by this implementation.

Tail Call Incompatibility: A Case Study
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Consider a chunk **tailcall-incompat.lua**.

.. code::

            local function foo(env)
                  print(bar() == baz())
                  print(bar() == _G)
                  print(baz() == _G)
            end

            setfenv(foo, {
                  bar   = function() local env = getfenv(1); return env end,
                  baz   = function() return getfenv(1) end, -- NB! getfenv(1) is tail-called
                  print = print,
                  _G    = _G,
            })

            foo()

In this chunk, we set a custom environment for the ``foo`` function. Both ``bar`` and ``baz`` return their environments, with the latter doing it via a tail call.

Let's run the chunk with the PUC-Rio interpreter:

.. code::

      $ lua5.1 ./tailcall-incompat.lua
      true
      true
      true

Repeat the same with |PROJECT|:

.. code::

      $ ujit ./tailcall-incompat.lua
      false
      true
      false

Please note following:

1. According to the documentation, ``gentfenv(1)`` returns environment of the function that called ``getfenv``.
2. In Lua 5.1 environments are not inherited across function calls, the "default" environment is stored in a special variable ``_G``.

Now the table below summarizes what happens:


====================== ============================================================================== ==============================================================================
Function / Interpreter PUC-Rio Lua                                                                    |PROJECT|
====================== ============================================================================== ==============================================================================
``bar()``              ``bar()`` is a called for ``getfenv(1)``. ``bar()``'s environment is returned. ``bar()`` is a called for ``getfenv(1)``. ``bar()``'s environment is returned.
``baz()``              ``baz()`` is a called for ``getfenv(1)``. ``baz()``'s environment is returned. ``foo()`` is a run-time caller for ``getfenv(1)`` (due to tail call implementation in |PROJECT|). ``foo()``'s environment is returned.
====================== ============================================================================== ==============================================================================


The takeaway is simple: Be careful when tail-calling functions that deal with the call stack in |PROJECT|. As caller frame gets overwritten by a tail-called callee, you'll certainly not get what you want. First of all it applies to ``debug`` library and functions like ``getfenv``/``setfenv``.
