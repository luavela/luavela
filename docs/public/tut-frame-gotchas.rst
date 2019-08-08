.. _tut-frame-gotchas:

Tutorial: Frame Gotchas
=======================

.. contents:: :local:

Guest Frame Types
-----------------

Or, more correctly, types of activation records of frames on the Lua stack. Please note that these types are caller types, the callee can be of any type: a Lua function ( ``LFUNC`` ), a registered C function ( ``CFUNC`` ) or a builtin (aka fast function, ``FFUNC`` ).

.. list-table::
   :widths: 25 50
   :header-rows: 1

   * - Type
     - Description
   * - ``FRAME_LUA``
     - VM performs a call as a result of bytecode execution.
   * - ``FRAME_C``
     - VM performs a call as a result of ``lj_vm_call``. At least following cases should be kept   in mind: lua_call or luaL_callmeta were invoked from the host application. A metamethod was called as a side effect of executing some C API: The most trivial example is lua_getfield, but this applies to all APIs that can invoke metamethods (see the Reference Manual for more details).
   * - ``FRAME_CONT``
     - VM performs a call to a metamethod as a result of bytecode execution. In this case, a special helper is required to return to the metamethod's implicit caller. These helpers are called continuations, hence the mnemonic. The activation record occupies 2 slots on the stack:

            .. code::

              +---------+---------+
              |   ftsz  | GCfunc* |
              +---------+---------+
              |   TNIL  |   cont  |
              +---------+---------+

        Where ``cont`` is an address relative to some function (``lj_vm_asm_begin`` in |PROJECT| 0.17) pointing to the actual continuation.
   * - ``FRAME_VARG``
     - Indicates that currently executed function has an auxiliary variable-length frame for storing arguments of a variadic function:

            .. code::

              +---------+---------+
              |   TAGN  | ....... |
              +---------+---------+
              |        ...        |
              +---------+---------+
              |   TAG2  | ....... |
              +---------+---------+
              |   TAG1  | ....... |
              +---------+---------+
              |   TNIL  | ....... |
              +---------+---------+
              |   TNIL  | ....... |
              +---------+---------+
              |   ftsz  | GCfunc* |
              +---------+---------+

            The number of ``TNIL`` slots is equal to the number of the function's fixed arguments. Values ``TAG1``, ... ``TAGN`` are actual variadic arguments. This frame is created inside ``IFUNCV`` prologue:

            .. code::

                Before IFUNCV (1 frame):


                                    /= +---------+---------+
                                    |  |   TAGN  | ....... |
                                    |  +---------+---------+
                Variadic arguments  |        ...        |
                                    |  +---------+---------+
                                    |  |   TAG2  | ....... |
                                    |  +---------+---------+
                                    |  |   TAG1  | ....... |
                                    \= +---------+---------+ =\
                                        |   TAG2' | ....... |  |
                                        +---------+---------+  | Fixed arguments
                                        |   TAG1' | ....... |  |
                                        +---------+---------+ =/
                                        |   ftsz  | GCfunc* |
                                        +---------+---------+


                After IFUNCV (2 frames):


                                        +---------+---------+ =\
                                        |   TAG2' | ....... |  |
                                        +---------+---------+  | Fixed arguments
                                        |   TAG1' | ....... |  |
                                        +---------+---------+ =/
                                        |   VARG  | GCfunc* |
                                    /= +---------+---------+
                                    |  |   TAGN  | ....... |
                                    |  +---------+---------+
                Variadic arguments  |  |        ...        |
                                    |  +---------+---------+
                                    |  |   TAG2  | ....... |
                                    |  +---------+---------+
                                    |  |   TAG1  | ....... |
                                    \= +---------+---------+ =\
                                        |   TNIL  | ....... |  |
                                        +---------+---------+  | Padding for fixed arguments
                                        |   TNIL  | ....... |  |
                                        +---------+---------+ =/
                                        |   LUA   | GCfunc* |
                                        +---------+---------+

            In the layout above, ``GCfunc*`` is the same for both frames, and variadic arguments can be retrieved with the ``VARG`` bytecode (implementation of the ``...`` operator).
   * - ``FRAME_LUAP``
     - Not used.
   * - ``FRAME_CP``
     - Protected C frame. There are several very different cases which are denoted with this frame type:

                                1. ``lua_pcall`` (an C API equivalent of both ``pcall`` and ``xpcall``) was invoked from the host application.
                                2. ``lua_cpcall`` was invoked from the host application.
                                3. A coroutine was resumed (via ``lua_resume`` or ``coroutine.resume``). To distinguish this case from the others, ``SAVE_CFRAME`` in the VM frame is or'ed with the ``CFRAME_RESUME`` flag.
                                4. ``lj_vm_cpcall`` was invoked internally without Lua payload (aka "protected C frame without Lua frame"). To distinguish this case from the others, ``SAVE_NRES`` in the VM frame is negated. By the way, this is how our platform implements ``try { ... } catch (...) { ... }``.
   * - ``FRAME_PCALL``
     - VM performs a call as a result of executing ``pcall`` or ``xpcall``. For ``xpcall``, Stack layout is as follows:

            .. code::

                                    pcall:

                                    +---------+---------+
                                    |  PCALL  | GCfunc* |
                                    +---------+---------+
                                    |  LUA    | pcall   |
                                    +---------+---------+

                                    xpcall:

                                    +---------+---------+
                                    |  PCALL  | GCfunc* |
                                    +---------+---------+
                                    |  TFUNC  | GCfunc* | <-- error handler (xpcall's second argument)
                                    +---------+---------+
                                    |  LUA    | xpcall  |
                                    +---------+---------+

   * - ``FRAME_PCALLH``
     - Same as above, but indicates that ``pcall``/``xpcall`` was invoked inside an active hook. This is needed for **not** leaving the hook when an error is caught with a ``pcall``/``xpcall`` inside it:

            .. code-block:: lua

                -- pcall creates FRAME_PCALL:
                local status = pcall(function ()
                    debug.sethook(function()
                        error("ERROR!")
                        print("Never reached 1")
                    end, "c")
                    -- Following happens before executing the next line of code:
                    -- 1. The hook is entered.
                    -- 2. The hook throws.
                    -- 3. Platform enforces an exit from the hook
                    -- 4. Control will be transferred outside pcall
                    assert(true)
                    print("Never reached 2")
                end)
                --
                -- At this point, however, the hook is still active.
                --
                -- Following happens before executing the next line of code:
                -- 1. The hook is entered.
                -- 2. The hook throws.
                -- 3. Platform enforces an exit from the hook
                -- 4. The script aborts because the error in the hook was not caught by any error handler
                assert(true)
                print("Never reached 3")

            .. code-block:: lua

                debug.sethook(function()
                    -- pcall creates FRAME_PCALLH:
                    pcall(error, "ERROR!")
                    print("Reached")
                end, "c")
                -- Following happens before executing the next line of code:
                -- 1. The hook is entered.
                -- 2. The hook throws.
                -- 3. Platform catches the error with pcall and continues executing the hook
                assert(true)