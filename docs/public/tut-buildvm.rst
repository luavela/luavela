.. _tut-buildvm:

Tutorial: buildvm Gotchas
=========================

.. contents:: :local:

Introduction
------------

This document sheds some light on how functions from Lua standard library (builtins) are "registered" to become available to the platform's end users. Following platform peculiarities should be taken into account:

    -  Implementation of some builtins may be scattered across
       the code base. E.g. a "fast path" may be implemented as a
       fast function inside the VM code, and the "slow path"
       requiring some more processing or throwing an error may
       be implemented in C. However, some builtins may be "fast
       functions"-only, and finally, the third group is C-only
       builtins.
    -  Some builtins may share a common "slow path".
    -  Implementation of some builtins may require access to
       certain upvalues.

You may be interested in this talk about `LuaJIT's original build chain <https://www.youtube.com/watch?v=8Q0KLTma_FA>`_.

``buildvm`` Utility
-------------------

Prior to building the platform's core, a special utility called buildvm is built and ran. This utility scans the core codebase and, based on special macros, generates some headers (``*def.h``) which are used inside ``uj_lib.c`` for building the standard library.

Sharing Common Fallback
------------------------

Consider following code:

.. code-block:: c

    LJLIB_ASM(math_abs) LJLIB_REC(.)
    {
        lj_lib_checknum(L, 1);
        return FFH_RETRY;
    }
    LJLIB_ASM_(math_floor) LJLIB_REC(.)

In this notation, ``LJLIB_ASM_`` means that a Lua function``math.floor`` is implemented as a fast function inside the VM (the ``LJLIB_ASM`` part of the macro), but its fallback is a body of the last functions declared with the ``LJLIB_ASM`` macro. Please note presence/absence of the final underscore. Here, this character carries information about where to find the fallback part:

.. code::

            $ nm ujit | fgrep -e ffh_math_floor -e ffh_math_abs
            000000000041715d t lj_ffh_math_abs
            $ gdb --args ./ujit -e 'print(math.floor("1.5"))'
            ...
            (gdb) b lj_ffh_math_abs
            ...
            (gdb) r
            ...
            Breakpoint 1, lj_ffh_math_abs (L=0x0) at /vagrant/ujit/src/lib/math.c:28
            28 {
            (gdb) bt
            #0 lj_ffh_math_abs (L=0x0) at /vagrant/ujit/src/lib/math.c:28
            #1 0x00000000004c4d38 in lj_fff_fallback ()
            #2 0x000000000040ebda in lua_pcall (L=0x7ffff7fcc378, nargs=0, nresults=0, errfunc=2) at /vagrant/ujit/src/uj_capi.c:884
            #3 0x0000000000405f5b in docall (L=0x7ffff7fcc378, narg=0, clear=1) at /vagrant/ujit/src/ujit.c:118
            ...

Please be careful modifying code in ``src/lib/*.c``.

Builtins Accessing Upvalues
----------------------------

As you probably know, ``pairs`` and ``ipairs`` are implemented via a call to to ``next``. Because of late binding, it is crucial to know the exact location of the original ``next`` in run-time, and ``_G.next`` is obviously not an option. To solve the issue, the "native" implementation of ``next`` is set as an upvalue for both ``pairs`` and ``ipairs``. In other words, the platform executes something like:

.. code-block:: lua

    local next = _G.next -- here, _G.next is our builtin
    _G.pairs = function(...)
        -- next is used somewhere here
    end

But how can we emulate this behaviour? Using following magic macros:

.. code-block:: c

    LJLIB_ASM(next)
    {
        lj_lib_checktab(L, 1);
        return FFH_UNREACHABLE;
    }
    ...
    LJLIB_PUSH(lastcl)
    LJLIB_ASM(pairs)
    {
        return ffh_pairs(L, MM_pairs);
    }


``buildvm`` scans ``src/lib/base.c`` and upon parsing ``LJLIB_PUSH(lastcl)``, stores a byte 253 (``0xfd`` aka ``LIBINIT_LASTCL``) into the array ``lj_lib_init_base`` (auto-generated in ``lj_libdef.h``). After that, ``uj_lib_register`` will be run inside ``luaopen_base``: At this time, the ``LIBINIT_LASTCL`` byte will be read from the ``lj_lib_init_base`` array, and the last registered builtin (``next`` in our case, the order of definitions does matter here) will be pushed on the coroutine's stack, which will be accessed an an upvalue during registering the ``pairs`` builtin.
