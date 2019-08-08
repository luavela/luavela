.. _tut-error-handling:

Tutorial: Error Handling Internals
==================================

.. contents:: :local:

Introduction
------------

Catching/Non-catching Interfaces 
---------------------------------

The table below summarizes Lua-level and C API interfaces in relation to exception handling:

=========================== ================= ===================
Lua-level                   C API counterpart Catches exceptions?
=========================== ================= ===================
"Regular" call ``foo(bar)`` ``lua_call``      No
``pcall``                   ``lua_pcall``     Yes
``xpcall``                  ``lua_pcall``     Yes
–                           ``lua_cpcall``    Yes
``coroutine.resume``        ``lua_resume``    Yes
=========================== ================= ===================

Notes:

1. Unlike ``pcall``, when ``xpcall`` catches an exception, the platform tries to execute a callback (a so-called "error function") before unwinding the stack and returning to the caller. ``lua_pcall`` interface (a common counterpart for both ``pcall`` and ``xpcall``) allows to implement both scenarios.
2. C API provides ``lua_cpcall``, an interface to safely call an arbitrary ``lua_CFunction`` which naturally has no Lua-level counterpart.

Exception types
---------------

Our platform currently distinguishes between following exceptions (don't be surprised that these are exactly status codes returned e.g. by ``lua_load*`` or ``lua_*call`` interfaces) :

            -  LUA_ERRRUN
            -  LUA_ERRSYNTAX
            -  LUA_ERRMEM
            -  LUA_ERRERR

Please refer to the `Lua Reference Manual <https://www.lua.org/manual/5.1/>`_ for exact semantics of these exceptions. Let's discuss in details how runtime errors (``LUA_ERRRUN``) are handled.

Handling Runtime Errors
-----------------------

Here is how the most common run-time exception (``LUA_ERRRUN``) is handled:

            - An arbitrary function throws a ``LUA_ERRRUN`` exception.
               Reasons may vary, for example:

               - Illegal operation (e.g. ``local foo = "bar" .. {}``).
               - Call to ``error`` from a Lua function.
               - Call to ``lua_error`` from a C function.

            - Error handler traverses the Lua stack searching for the
               error function. The search will succeed as soon as any of
               these cases is met:

               - A frame created by ``xpcall`` is found on the stack.
               - A frame created by ``lua_pcall`` with ``errfunc != 0`` is found on the stack.

            - If the error function is found, the platform attempts to
               execute it on the stack of the function that threw the
               exception.
            - If the error function succeeds, the ``LUA_ERRRUN``
               exception is propagated to an external unwinder (see
               below).
            - If the error function fails, the ``LUA_ERRERR`` exception
               is propagated to an external unwinder (see below).

Now let's take a closer look at some points of the algorithm outlined above.

External Unwinder
-----------------

When the error function returns (or as soon as no error function is found on the stack), the platform should do the following:

1. Unwind the stack to find a protected frame that is closest to the caller. A frame is protected if it was created by ``pcall``, ``xpcall`` or any other interface that is able to catch exceptions (see above). This step is called a search phase, at this moment the stack is inspected, but not touched in any manner.
2. "Truly" unwind the stack until the protected frame. This  step is called a cleanup phase.
3. Return control to the caller of the protected function.

This is done using an external unwinder. Its interfaces (``_Unwind_*``) are specified by the System V ABI  Specification (for details, see `the specification for AMD64 platform 6.2 Unwind Library Interface <https://www.uclibc.org/docs/psABI-x86_64.pdf>`_). This specification provides a good high-level description of the unwinding process, so it may be a good idea to read it before you go further with this article. On our target platform these interfaces are implemented as a part of ``libgcc_s`` library.

Once invoked, the external unwinder enters the search phase and starts inspecting the host stack (x64 in our case). For each stack frame, it has to decide if this frame can handle the exception or if the unwinding should be continued further. To make that decision, the unwinder searches for a
so-called personality routine, a function provided by the application (or its runtime) and calls it. This function performs language-specific job and returns a hint to the external unwinding signalling if stack inspection should be stopped or continued.

In |PROJECT|, the personality routine is called ``uj_dwarf_personality`` (or, in older days, ``lj_err_unwind_dwarf``). To assist the external unwinder, it uses an internal unwinder (``uj_unwind.c``). This internal unwinder is aware of the Lua stack (which the external unwinder obviously has no idea of) and works as follows:

1. On each external inspection step during search phase, it is called with the ``%rsp`` value for the currently inspected host frame.
2. It inspects the Lua stack until it finds the first protected Lua frame. Please note that during inspecting the Lua stack, we keep track of corresponding VM frames (which are host x64 frames) to ensure that inspection does not go further than ``%rsp`` provided by the   external unwinder.

Once a protected frame is found, a corresponding value is returned to the external unwinder signalling to move further to the cleanup phase. During this phase, the host stack frame gets unwound "for real" (e.g. for an application written in C++ this would imply calling destructors). Please note that during the cleanup phase the external unwinder also invokes the personality routine to perform language-specific cleanup actions. It means that the internal unwinder is run again, this time in cleanup mode.

How Personality Routine is Found
--------------------------------

Information about the personality routine is stored in the ``.eh_frame`` of the executable binary:

.. code::

                $ readelf --debug-dump=frames ./luajit | less # ...and search for "zPR" in the output
                000043e0 000000000000001c 00000000 CIE
                Version:               1
                Augmentation:          "zPR"
                Code alignment factor: 1
                Data alignment factor: -8
                Return address column: 16
                Augmentation data:     1b 8d fa f0 ff 1b

                DW_CFA_def_cfa: r7 (rsp) ofs 8
                DW_CFA_offset: r16 (rip) at cfa-8
                DW_CFA_nop
                DW_CFA_nop
                DW_CFA_nop
                DW_CFA_nop


                00004400 000000000000001c 00000024 FDE cie=000043e0 pc=0000000000436fe0..000000000043b466
                DW_CFA_def_cfa_offset: 112
                DW_CFA_offset: r6 (rbp) at cfa-16
                DW_CFA_offset: r3 (rbx) at cfa-24
                DW_CFA_offset: r15 (r15) at cfa-32
                DW_CFA_offset: r14 (r14) at cfa-40
                DW_CFA_nop
                DW_CFA_nop
                DW_CFA_nop
                DW_CFA_nop
                DW_CFA_nop

The range of addresses ``[0x436fe0; 0x43b466]`` in the frame description entry (aka FDE) covers the entire assembly code of the VM. Whenever during host stack inspection we find a frame with program counter fitting this range, the external unwinder checks with the common information entry
(aka CIE) referenced by the FDE and retrieves the personality routine address (encoded within the so-called augmentation data). The .eh_frame layout is also specified by the System V ABI Specification (4.2.4 EH_FRAME sections), a bit more verbose description can be found in the `Linux Standard Base <http://refspecs.linuxfoundation.org/LSB_5.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html>`_ (a standard which incorporates parts of the System V ABI Specification, but not supported by many Linux distributions, including Ubuntu 14.04 and higher).

The contents of the ``.eh_frame`` section for |PROJECT| is emitted by the ``emit_asm_debug`` function at compile time. By the way, if one looks a bit closer at the FDE contents, one can easily see information about location of callee-saved registers which perfectly matches with the code of the ``saveregs`` macro which is executed on each VM frame creation. This information should be reported to the unwinder, too.

Further Reading
---------------

 Although this material belongs to the very core functionality of runtime environments, there are not so many information about it. Some documents (System V ABI Specification) have unclear official status and other documents and initiatives (Linux Standard Base) seem to have low adoption rate. There is also some grade of confusion between similar concepts: For example, one day you may find out that ``.debug_frame`` specified by the DWARF standard and ``.eh_frame`` (not a part of the DWARF standard) provide almost the same functionality, with subtle differences (se e.g. `here <http://wiki.dwarfstd.org/index.php?title=Exception_Handling>`__).


Still, here are some links to shed more light on the topic (please contribute more):

            -  `C++ exception handling
               internals <https://monoinfinito.wordpress.com/series/exception-handling-in-c/>`__
               (really impressive write-up)
            -  `A bit more documentation for
               .eh_frame <http://www.airs.com/blog/archives/460>`__
            -  `Implementation of the \_Unwind_\* interfaces in
               GCC <https://github.com/gcc-mirror/gcc/blob/master/libgcc/unwind.inc>`__

             
