.. _tut-ir-loads:

Tutorial: IR Loads and Stores
=============================

.. contents:: :local:

Introduction
------------

Info from http://wiki.luajit.org/SSA-IR-2.0:

    ====== ===== ====== ==================
    OP     Left  Right  Description
    ====== ===== ====== ==================
    ALOAD  aref         Array load
    HLOAD  href         Hash load
    ULOAD  uref         Upvalue load
    FLOAD  obj   #field Object field load
    XLOAD  xref  #flags Extended load
    SLOAD  #slot #flags Stack slot load
    VLOAD  aref         Vararg slot load
    ASTORE aref  val    Array store
    HSTORE href  val    Hash store
    USTORE uref  val    Upvalue store
    FSTORE fref  val    Object field store
    XSTORE xref  val    Extended store
    ====== ===== ====== ==================

.. note::

    Loads and stores operate on memory references and either load a value (result of the instruction) or store a value (the right operand). To preserve higher-level semantics and to simplify alias analysis they are not unified or decomposed into lower-level operations.

    ``FLOAD`` and ``SLOAD`` inline their memory references, all other loads and all stores have a memory reference as their left operand. All loads except ``FLOAD`` and ``XLOAD`` work on tagged values and simultaneously function as a guarded assertion that checks the loaded type.

    ``LOAD`` and ``FSTORE`` access specific fields inside objects, identified by the field ``ID`` of their reference (e.g. the metatable field in table or userdata objects).

    ``XLOAD`` works on lower-level types and the memory reference is either a ``STRREF`` or decomposed into lower-level operations, a combination of ``ADD``, ``MUL`` or ``BSHL`` of pointers, offsets or indexes.

    The slot number of ``SLOAD`` is relative to the starting frame of a trace, where #0 indicates the closure/frame slot and #1 the first variable slot (corresponding to slot 0 of the bytecode).

    Note, that ``RETF`` shifts down ``BASE`` and subsequent ``SLOAD`` instructions refer to slots of the lower frame(s). Also note, there are no store operations for stack slots or vararg slots. All stores to stack slots are effectively sunk into exits or side traces. Snapshots efficiently manage the references that are to be stored. Vararg slots are read-only from the perspective of the called vararg function.

    For the possible values of the field ``ID`` in ``FLOAD`` and the flags in ``SLOAD`` and ``XLOAD``, see ``IRFLDEF``, ``IRSLOAD_*`` and ``IRXLOAD_*`` in ``src/lj_ir.h``.

Use this command line arguments to run code examples:

.. code-block:: sh

    ./ujit -j on -p- example.lua

``FLOAD``, ``SLOAD``
--------------------

Loads some field from of the object, i.e. ``(GCtab *)t->metatable``.

``SLOAD``
---------

Loads payload from stack slot, i.e. ``(TValue *)tv->gcr``. Optionally with type checks (grep for ``IRT_GUARD``).

Let's run this code:

.. code-block:: lua

    jit.opt.start("hotloop=1", "nohrefk")

    local t = {}

    for i = 1, 3 do
            local o = t[0]
    end

And see closer at IR and mcode:

.. code::

    ---- TRACE 1 start example.lua:5
    0012    TGETB    5   0   0
    0014    FORL     1 => 0012
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ]
    0001 rbp      int SLOAD  #2    CI
    0002 rdx   >  tab SLOAD  #1    T
    0003 rsi      int FLOAD  0002  tab.asize
    0004       >  int ULE    0003  +0
    0005 rbx      int FLOAD  0002  tab.hmask
    0006       >  int EQ     0005  +0
    0007 rax      tab FLOAD  0002  tab.meta
    0008       >  tab EQ     0007  [NULL]
    0009 rbp    + int ADD    0001  +1
    ....              SNAP   #1   [ ---- ---- ]
    0010       >  int LE     0009  +3
    ....              SNAP   #2   [ ---- ---- 0009 ---- ---- 0009 ]
    0011 ------------ LOOP ------------
    0012 rbp    + int ADD    0009  +1
    ....              SNAP   #3   [ ---- ---- ]
    0013       >  int LE     0012  +3
    0014 rbp      int PHI    0009  0012
    ---- TRACE 1 mcode 100
    // Standard prologue, see emit_vmstate(..) in asm_head_root() from lj_asm.h
    /* PRL */ 0bd5ff99  mov r11, 0x7f37f9fe3620 // &g->vmstate field VA
    /* PRL */ 0bd5ffa3  mov dword [r11], 0x1 // 1 is a current traceno
    /*  K  */ 0bd5ffaa  xor ecx, ecx // NULL constant
    /* 001 */ 0bd5ffac  cvtsd2si ebp, qword [r10+0x10] // Load 32-bit signed integer from 2-nd slot (counter).
                                                    // (CI means converted double and inherit by exit / size states. Grep IRSLOAD_ for more info.)
    /* 002 */ 0bd5ffb2  cmp dword [r10+0x8], 0xfffffff4 // Typecheck that 1-st slot contains a table
    /* 002 */ 0bd5ffb7  jnz 0xbd50010               ->0 // Guard, jump to the first snapshot
    /* 002 */ 0bd5ffbd  mov rdx, [r10] // Pointer to table from 1-st slot: (TValue *)->gcr
    /* 003 */ 0bd5ffc0  mov esi, [rdx+0x30] // esi = tab->asize
    /* 004 */ 0bd5ffc3  cmp esi, 0x0 // if array part is zero
    /* 004 */ 0bd5ffc6  ja 0xbd50010                ->0 // then exit
    /* 005 */ 0bd5ffcc  mov ebx, [rdx+0x38] // ebx = tab->hmask
    /* 006 */ 0bd5ffcf  test ebx, ebx // same check for zero
    /* 006 */ 0bd5ffd1  jnz 0xbd50010               ->0
    /* 007 */ 0bd5ffd7  mov rax, [rdx+0x18] rax = tab->metatable
    /* 008 */ 0bd5ffdb  cmp rax, rcx // compare with nil
    /* 008 */ 0bd5ffde  jnz 0xbd50010               ->0 // has metatable? exit
    /* 009 */ 0bd5ffe4  add ebp, 0x1 // add step (immediate constant)
    /* 010 */ 0bd5ffe7  cmp ebp, 0x3 // compare for exit (immediate constant)
    /* 010 */ 0bd5ffea  jg 0xbd50014                ->1
    -> LOOP:
    /* 012 */ 0bd5fff0  add ebp, 0x1 // add step (immediate constant)
    /* 013 */ 0bd5fff3  cmp ebp, 0x3 // compare for exit (immediate constant)
    /* 013 */ 0bd5fff6  jle 0xbd5fff0               ->LOOP // continue loop
    /* end */ 0bd5fff8  jmp 0xbd5001c               ->3 // normal exit
    ---- TRACE 1 stop -> loop

``FSTORE``
----------

Stores value to the some field of the given object, i.e. ``(GCtab *)->env = val;``

``HLOAD``
---------

Loads payload from tagged value, uses ``HREF`` as input.

.. code-block:: lua

    jit.opt.start("hotloop=1", "nohrefk")

    local t = {}

    for i = 1, 3 do
        setmetatable(t, nil)
    end

.. code::

    ---- TRACE 1 start example.lua:5
    0012    GGET     5   5      ; "setmetatable"
    0013    MOV      6   0
    0014    KPRI     7   0
    0015    CALL     5   1   3
    0000    . FUNCC               ; setmetatable
    0017    FORL     1 => 0012
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ]
    0001 rbp      int SLOAD  #2    CI
    0002 r12      fun SLOAD  #0    R
    0003 r9       tab FLOAD  0002  func.env
    0004 r8       p32 HREF   0003  "setmetatable"
    0005 rsi   >  fun HLOAD  0004
    0006 rcx   >  tab SLOAD  #1    T
    0007       >  fun EQ     0005  setmetatable
    0008 rbx      u8  FLOAD  0006  gco.marked
    0009          u8  BAND   0008  +64
    0010       >  int EQ     0009  +0
    0011 rdx      tab FLOAD  0006  tab.meta
    0012       >  tab EQ     0011  [NULL]
    0013          p32 FREF   0006  tab.meta
    0014          tab FSTORE 0013  [NULL]
    0015 rbp    + int ADD    0001  +1
    ....              SNAP   #1   [ ---- ---- ]
    0016       >  int LE     0015  +3
    ....              SNAP   #2   [ ---- ---- 0015 ---- ---- 0015 ]
    0017 ------------ LOOP ------------
    0018 rbp    + int ADD    0015  +1
    ....              SNAP   #3   [ ---- ---- ]
    0019       >  int LE     0018  +3
    0020 rbp      int PHI    0015  0018
    ---- TRACE 1 mcode 200
    /* PRL */ 0bd5ff35  mov r11, 0x7ff48c16e620 // Default root trace prologue, see FLOAD / SLOAD example
    /* PRL */ 0bd5ff3f  mov dword [r11], 0x1
    /*  K  */ 0bd5ff46  mov rdi, 0x7ff48c172b10 // constant, setmetatable VA
    /*  K  */ 0bd5ff50  xor eax, eax // NULL constant
    /* 001 */ 0bd5ff52  cvtsd2si ebp, qword [r10+0x10] // load signed integer from slot 2
    /* 002 */ 0bd5ff58  mov r12, [r10-0x10] // calle function object, read-only
    /* 003 */ 0bd5ff5c  mov r9, [r12+0x10] // r9 = (GCfunc *)->env
    /* 004 */ 0bd5ff61  mov r8d, [r9+0x38]
    /* 004 */ 0bd5ff65  and r8d, 0x5950030a
    /* 004 */ 0bd5ff6c  imul r8d, r8d, 0x28
    /* 004 */ 0bd5ff70  add r8, [r9+0x28]
    /* 004 */ 0bd5ff74  cmp dword [r8+0x18], 0xfffffffb
    /* 004 */ 0bd5ff79  jnz 0xbd5ff8b
    /* 004 */ 0bd5ff7b  mov r11, 0x7ff48c172b48
    /* 004 */ 0bd5ff85  cmp r11, [r8+0x10]
    /* 004 */ 0bd5ff89  jz 0xbd5ff9e
    /* 004 */ 0bd5ff8b  mov r8, [r8+0x20]
    /* 004 */ 0bd5ff8f  test r8, r8
    /* 004 */ 0bd5ff92  jnz 0xbd5ff74
    /* 004 */ 0bd5ff94  mov r8, 0x7ff48c16e540
    /* 005 */ 0bd5ff9e  cmp dword [r8+0x8], 0xfffffff7
    /* 005 */ 0bd5ffa3  jnz 0xbd50010               ->0
    /* 005 */ 0bd5ffa9  mov rsi, [r8]
    /* 006 */ 0bd5ffac  cmp dword [r10+0x8], 0xfffffff4
    /* 006 */ 0bd5ffb1  jnz 0xbd50010               ->0
    /* 006 */ 0bd5ffb7  mov rcx, [r10]
    /* 007 */ 0bd5ffba  cmp rsi, rdi
    /* 007 */ 0bd5ffbd  jnz 0xbd50010               ->0
    /* 008 */ 0bd5ffc3  movzx ebx, byte [rcx+0x8]
    /* 009 */ 0bd5ffc7  test ebx, 0x40
    /* 010 */ 0bd5ffcd  jnz 0xbd50010               ->0
    /* 011 */ 0bd5ffd3  mov rdx, [rcx+0x18]
    /* 012 */ 0bd5ffd7  cmp rdx, rax
    /* 012 */ 0bd5ffda  jnz 0xbd50010               ->0
    /* 013-014*/ 0bd5ffe0  mov [rcx+0x18], rax
    /* 015 */ 0bd5ffe4  add ebp, 0x1
    /* 016 */ 0bd5ffe7  cmp ebp, 0x3
    /* 016 */ 0bd5ffea  jg 0xbd50014                ->1
    -> LOOP:
    /* 018 */ 0bd5fff0  add ebp, 0x1
    /* 019 */ 0bd5fff3  cmp ebp, 0x3
    /* 019 */ 0bd5fff6  jle 0xbd5fff0               ->LOOP
    /* end */0bd5fff8  jmp 0xbd5001c            ->3
    ---- TRACE 1 stop -> loop

``ALOAD``, ``ASTORE``
---------------------

May be just type check after ``AREF`` IR, let's use Lua code from ``FLOAD`` again.

Or perform load of the ``GCobj`` from ``TValue``:

.. code-block:: lua

    jit.opt.start("hotloop=1", "nohrefk")

    local t = {1, 2, 3, 4}

    for i = 1, 3 do
        t[i] = t[i + 1]
    end

.. code::

    ---- TRACE 1 start example.lua:6
    0012    KSHORT   5   1
    0013    ADD      5   4   5
    0014    TGETV    5   0   5
    0015    TSETV    5   0   4
    0017    FORL     1 => 0012
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ]
    0001 r9       int SLOAD  #2    CI
    0003 rdi   >  tab SLOAD  #1    T
    0004 [8]    + int ADD    0001  +1
    0005 r10      int FLOAD  0003  tab.asize
    0006       >  p32 ABC    0005  +4
    0007 rax      p32 FLOAD  0003  tab.array
    0008 rbp    + p32 AREF   0007  0004
    0009 xmm0  >  flt ALOAD  0008
    0010 rcx      p32 AREF   0007  0001
    0011 r8       u8  FLOAD  0003  gco.marked
    0012          u8  BAND   0011  +64
    0013       >  int EQ     0012  +0
    0014 rdx      tab FLOAD  0003  tab.meta
    0015       >  tab EQ     0014  [NULL]
    0016          flt ASTORE 0010  0009
    ....              SNAP   #1   [ ---- ---- ]
    0017       >  int LE     0004  +3
    ....              SNAP   #2   [ ---- ---- 0004 ---- ---- 0004 ]
    0018 ------------ LOOP ------------
    0019 rbx    + int ADD    0004  +1
    0020 rbp    + p32 AREF   0007  0019
    0021 xmm7  >  flt ALOAD  0020
    0022          flt ASTORE 0008  0021
    ....              SNAP   #3   [ ---- ---- ]
    0023       >  int LE     0019  +3
    0024 rbx      int PHI    0004  0019
    0025 rbp      p32 PHI    0008  0020
    0026 r14      nil RENAME 0004  #32767
    0027 r15      nil RENAME 0008  #2
    ---- TRACE 1 mcode 215
    0bd5ff28  mov r11, 0x7f9243b22620
    0bd5ff32  mov dword [r11], 0x1
    0bd5ff39  xor esi, esi
    0bd5ff3b  cvtsd2si r9d, qword [r10+0x10]
    0bd5ff41  cmp dword [r10+0x8], 0xfffffff4
    0bd5ff46  jnz 0xbd50010             ->0
    0bd5ff4c  mov rdi, [r10]
    0bd5ff4f  lea ebx, [r9+0x1]
    0bd5ff53  mov [rsp+0x8], ebx
    0bd5ff57  mov r10d, [rdi+0x30]
    0bd5ff5b  cmp r10, 0x4
    0bd5ff5f  jbe 0xbd50010             ->0
    0bd5ff65  mov rax, [rdi+0x10]
    0bd5ff69  mov ebp, ebx
    0bd5ff6b  shl ebp, 0x4
    0bd5ff6e  add rbp, rax
    0bd5ff71  cmp dword [rbp+0x8], 0xfffffff2
    0bd5ff78  jnz 0xbd50010             ->0
    0bd5ff7e  movsd xmm0, qword [rbp]
    0bd5ff84  mov ecx, r9d
    0bd5ff87  shl ecx, 0x4
    0bd5ff8a  add rcx, rax
    0bd5ff8d  movzx r8d, byte [rdi+0x8]
    0bd5ff92  test r8d, 0x40
    0bd5ff99  jnz 0xbd50010             ->0
    0bd5ff9f  mov rdx, [rdi+0x18]
    0bd5ffa3  cmp rdx, rsi
    0bd5ffa6  jnz 0xbd50010             ->0
    0bd5ffac  mov dword [rcx+0x8], 0xfffffff2
    0bd5ffb3  movsd [rcx], xmm0
    0bd5ffb7  cmp ebx, 0x3
    0bd5ffba  jg 0xbd50014              ->1
    -> LOOP:
    0bd5ffc0  mov [rsp+0x8], ebx
    0bd5ffc4  mov r15, rbp
    0bd5ffc7  mov r14d, ebx
    0bd5ffca  add ebx, 0x1
    0bd5ffcd  mov ebp, ebx
    0bd5ffcf  shl ebp, 0x4
    0bd5ffd2  add rbp, rax
    0bd5ffd5  cmp dword [rbp+0x8], 0xfffffff2
    0bd5ffdc  jnz 0xbd50018             ->2
    0bd5ffe2  movsd xmm7, qword [rbp]
    0bd5ffe8  mov dword [r15+0x8], 0xfffffff2
    0bd5fff0  movsd [r15], xmm7
    0bd5fff5  cmp ebx, 0x3
    0bd5fff8  jle 0xbd5ffc0             ->LOOP
    0bd5fffa  jmp 0xbd5001c             ->3
    ---- TRACE 1 stop -> loop

``HLOAD``, ``HSTORE``
---------------------

Same as ``ASTORE``, but for hash part.

.. code-block:: lua

    jit.opt.start("hotloop=1", "nohrefk")

    local t = {["key"] = 1}

    for i = 1, 3 do
        t["newkey"] = t["key"]
    end

And see closer at IR and mcode:

.. code::

    ---- TRACE 1 start example.lua:5
    0012    TGETS    5   0   7  ; "key"
    0013    TSETS    5   0   6  ; "newkey"
    0015    FORL     1 => 0012
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ]
    0001 rbp      int SLOAD  #2    CI
    0002 rax   >  tab SLOAD  #1    T
    0003 r9       p32 HREF   0002  "key"
    0004 xmm0  >  flt HLOAD  0003
    0005 rcx      p32 HREF   0002  "newkey"
    0006 r8       u8  FLOAD  0002  gco.marked
    0007          u8  BAND   0006  +64
    0008       >  int EQ     0007  +0
    0009       >  p32 NE     0005  [0x7f48c628b540]
    0010 rdx      tab FLOAD  0002  tab.meta
    0011       >  tab EQ     0010  [NULL]
    0012          flt HSTORE 0005  0004
    0013          nil TBAR   0002
    0014 rbp    + int ADD    0001  +1
    ....              SNAP   #1   [ ---- ---- ]
    0015       >  int LE     0014  +3
    ....              SNAP   #2   [ ---- ---- 0014 ---- ---- 0014 ]
    0016 ------------ LOOP ------------
    0017 rbp    + int ADD    0014  +1
    ....              SNAP   #3   [ ---- ---- ]
    0018       >  int LE     0017  +3
    0019 rbp      int PHI    0014  0017
    ---- TRACE 1 mcode 302
    0bd5fecf  mov r11, 0x7f48c628b620
    0bd5fed9  mov dword [r11], 0x1
    0bd5fee0  mov rsi, 0x7f48c628b540
    0bd5feea  xor ebx, ebx
    0bd5feec  cvtsd2si ebp, qword [r10+0x10]
    0bd5fef2  cmp dword [r10+0x8], 0xfffffff4
    0bd5fef7  jnz 0xbd50010             ->0
    0bd5fefd  mov rax, [r10]
    0bd5ff00  mov r9d, [rax+0x38]
    0bd5ff04  and r9d, 0x68ca1d79
    0bd5ff0b  imul r9d, r9d, 0x28
    0bd5ff0f  add r9, [rax+0x28]
    0bd5ff13  cmp dword [r9+0x18], 0xfffffffb
    0bd5ff18  jnz 0xbd5ff2a
    0bd5ff1a  mov r11, 0x7f48c628d438
    0bd5ff24  cmp r11, [r9+0x10]
    0bd5ff28  jz 0xbd5ff3d
    0bd5ff2a  mov r9, [r9+0x20]
    0bd5ff2e  test r9, r9
    0bd5ff31  jnz 0xbd5ff13
    0bd5ff33  mov r9, 0x7f48c628b540
    0bd5ff3d  cmp dword [r9+0x8], 0xfffffff2
    0bd5ff45  jnz 0xbd50010             ->0
    0bd5ff4b  movsd xmm0, qword [r9]
    0bd5ff50  mov ecx, [rax+0x38]
    0bd5ff53  and ecx, 0x7eaa3afd
    0bd5ff59  imul ecx, ecx, 0x28
    0bd5ff5c  add rcx, [rax+0x28]
    0bd5ff60  cmp dword [rcx+0x18], 0xfffffffb
    0bd5ff64  jnz 0xbd5ff76
    0bd5ff66  mov r11, 0x7f48c628d5b8
    0bd5ff70  cmp r11, [rcx+0x10]
    0bd5ff74  jz 0xbd5ff89
    0bd5ff76  mov rcx, [rcx+0x20]
    0bd5ff7a  test rcx, rcx
    0bd5ff7d  jnz 0xbd5ff60
    0bd5ff7f  mov rcx, 0x7f48c628b540
    0bd5ff89  movzx r8d, byte [rax+0x8]
    0bd5ff8e  test r8d, 0x40
    0bd5ff95  jnz 0xbd50010             ->0
    0bd5ff9b  cmp rcx, rsi
    0bd5ff9e  jz 0xbd50010              ->0
    0bd5ffa4  mov rdx, [rax+0x18]
    0bd5ffa8  cmp rdx, rbx
    0bd5ffab  jnz 0xbd50010             ->0
    0bd5ffb1  mov dword [rcx+0x8], 0xfffffff2
    0bd5ffb8  movsd [rcx], xmm0
    0bd5ffbc  test byte [rax+0x8], 0x4
    0bd5ffc0  jz 0xbd5ffe4
    0bd5ffc2  and byte [rax+0x8], 0xfb
    0bd5ffc6  mov r11, 0x7f48c628b4e0
    0bd5ffd0  mov rdi, [r11]
    0bd5ffd3  mov r11, 0x7f48c628b4e0
    0bd5ffdd  mov [r11], rax
    0bd5ffe0  mov [rax+0x20], rdi
    0bd5ffe4  add ebp, 0x1
    0bd5ffe7  cmp ebp, 0x3
    0bd5ffea  jg 0xbd50014              ->1
    -> LOOP:
    0bd5fff0  add ebp, 0x1
    0bd5fff3  cmp ebp, 0x3
    0bd5fff6  jle 0xbd5fff0             ->LOOP
    0bd5fff8  jmp 0xbd5001c             ->3
    ---- TRACE 1 stop -> loop
