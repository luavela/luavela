.. _tut-trace-link-types:

Tutorial: Trace Link Types
==========================

.. contents:: :local:

``LJ_TRLINK_LOOP``
------------------

Triggered by ``LOOP``, ``ITERL`` and ``FORL`` instructions. Only for traces, that starts from interpreter (i.e. root traces).

.. code-block:: lua

    local sum = 5
    for i = 1, 5 do
      sum = sum + i
    end

And run in with:

.. code::

    ujit -Ohotloop=2 -p- example.lua

Example of an output:

.. code::

    ---- TRACE 1 start example.lua:2
    0006    ADD      0   0   4
    0007    FORL     1 => 0006
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ]
    0001 rbp      int SLOAD  #2    CI
    0002 xmm0  >  flt SLOAD  #1    T
    0003 xmm7     flt CONV   0001  flt.int
    0004 xmm7   + flt ADD    0003  0002
    0005 rbp    + int ADD    0001  +1
    ....              SNAP   #1   [ ---- 0004 ]
    0006       >  int LE     0005  +5
    ....              SNAP   #2   [ ---- 0004 0005 ---- ---- 0005 ]
    0007 ------------ LOOP ------------
    0008 xmm6     flt CONV   0005  flt.int
    0009 xmm7   + flt ADD    0008  0004
    0010 rbp    + int ADD    0005  +1
    ....              SNAP   #3   [ ---- 0009 ]
    0011       >  int LE     0010  +5
    0012 rbp      int PHI    0005  0010
    0013 xmm7     flt PHI    0004  0009
    ---- TRACE 1 mcode 89
    0bd5ff9f  mov r11, 0x7fe4d33a4608
    0bd5ffa9  mov dword [r11], 0x1
    0bd5ffb0  cvtsd2si ebp, qword [r10+0x10]
    0bd5ffb6  cmp dword [r10+0x8], 0xfffffff2
    0bd5ffbe  jnz 0xbd50010             ->0
    0bd5ffc4  movsd xmm0, qword [r10]
    0bd5ffc9  xorps xmm7, xmm7
    0bd5ffcc  cvtsi2sd xmm7, ebp
    0bd5ffd0  addsd xmm7, xmm0
    0bd5ffd4  add ebp, 0x1
    0bd5ffd7  cmp ebp, 0x5
    0bd5ffda  jg 0xbd50014              ->1
    -> LOOP:
    0bd5ffe0  xorps xmm6, xmm6
    0bd5ffe3  cvtsi2sd xmm6, ebp
    0bd5ffe7  addsd xmm7, xmm6
    0bd5ffeb  add ebp, 0x1
    0bd5ffee  cmp ebp, 0x5
    0bd5fff1  jle 0xbd5ffe0             ->LOOP
    0bd5fff3  jmp 0xbd5001c             ->3
    ---- TRACE 1 stop -> loop

``LJ_TRLINK_ROOT``
------------------

Side trace jumps to the first machine instruction of its "root" trace.

.. code-block:: lua

    local sum = 0

    function foo(x)
        return x + 0x11
    end

    for i = 1, 0x44 do
        sum = sum + i

        if i > 0x7 then
            sum = sum + foo(i)
        end
    end

.. code::

    ujit -Ohotloop=6 -p- example.lua

.. code::

    ---- TRACE 2 start 1/1 example.lua:11
    0012    GGET     5   1      ; "foo"
    0013    MOV      6   4
    0014    CALL     5   2   2
    0000    . FUNCF    2          ; example.lua:3
    0001    . KSHORT   1  17
    0002    . ADD      1   0   1
    0003    . RET1     1   2
    0015    ADD      0   0   5
    0016    JFORL    1   1
    ---- TRACE 2 IR
    0001 xmm7     flt SLOAD  #1    PI
    0002 rbp      int SLOAD  #2    PI
    ....              SNAP   #0   [ ---- 0001 0002 ---- ---- 0002 ]
    0003 rbx      fun SLOAD  #0    R
    0004 rbx      tab FLOAD  0003  func.env
    0005 r14      int FLOAD  0004  tab.hmask
    0006       >  int EQ     0005  +63
    0007 rbx      p32 FLOAD  0004  tab.node
    0008 rbx   >  p32 HREFK  0007  "foo" @41
    0009 rbx   >  fun HLOAD  0008
    0010       >  fun EQ     0009  example.lua:3
    0011 rbx   >  int ADDOV  0002  +17
    0012 xmm6     flt CONV   0011  flt.int
    0013 xmm7     flt ADD    0012  0001
    0014 rbp      int ADD    0002  +1
    ....              SNAP   #1   [ ---- 0013 ]
    0015       >  int LE     0014  +68
    0016 xmm6     flt CONV   0014  flt.int
    ....              SNAP   #2   [ ---- 0013 0016 ---- ---- 0016 ]
    ---- TRACE 2 mcode 205
    0bd5feb2  mov r11, 0x7f13033dc608
    0bd5febc  mov dword [r11], 0x2
    0bd5fec3  mov r15, 0x7f13033df268
    0bd5fecd  mov rbx, [r10-0x10]
    0bd5fed1  mov rbx, [rbx+0x10]
    0bd5fed5  mov r14d, [rbx+0x38]
    0bd5fed9  cmp r14d, 0x3f
    0bd5fedd  jnz 0xbd50010             ->0
    0bd5fee3  mov rbx, [rbx+0x28]
    0bd5fee7  mov rdi, 0x7f13033df218
    0bd5fef1  cmp rdi, [rbx+0x678]
    0bd5fef8  jnz 0xbd50010             ->0
    0bd5fefe  cmp dword [rbx+0x680], 0xfffffffb
    0bd5ff05  jnz 0xbd50010             ->0
    0bd5ff0b  add rbx, 0x668
    0bd5ff12  cmp dword [rbx+0x8], 0xfffffff7
    0bd5ff16  jnz 0xbd50010             ->0
    0bd5ff1c  mov rbx, [rbx]
    0bd5ff1f  cmp rbx, r15
    0bd5ff22  jnz 0xbd50010             ->0
    0bd5ff28  mov ebx, ebp
    0bd5ff2a  add ebx, 0x11
    0bd5ff2d  jo 0xbd50010              ->0
    0bd5ff33  xorps xmm6, xmm6
    0bd5ff36  cvtsi2sd xmm6, ebx
    0bd5ff3a  addsd xmm7, xmm6
    0bd5ff3e  add ebp, 0x1
    0bd5ff41  cmp ebp, 0x44
    0bd5ff44  jg 0xbd50014              ->1
    0bd5ff4a  xorps xmm6, xmm6
    0bd5ff4d  cvtsi2sd xmm6, ebp
    0bd5ff51  mov dword [r10+0x48], 0xfffffff2
    0bd5ff59  movsd [r10+0x40], xmm6
    0bd5ff5f  mov dword [r10+0x18], 0xfffffff2
    0bd5ff67  movsd [r10+0x10], xmm6
    0bd5ff6d  mov dword [r10+0x8], 0xfffffff2
    0bd5ff75  movsd [r10], xmm7
    0bd5ff7a  jmp 0xbd5ff86 /* Jump to the first machine instruction of TRACE 1 */
    ---- TRACE 2 stop -> 1

``LJ_TRLINK_RETURN``
--------------------

This type is set when recording RET\* bytecode and frame depth is zero (trace recording started by FUNCF), then we must return to the interpreter (jump vm_exit_interp at the end).

.. code-block:: lua

    local sum = 0

    function foo(x)
        return x + 0x11
    end

    for i = 1, 0x44 do
        sum = sum + i

        if i > 0x7 then
            sum = sum + foo(i)
        end
    end

.. code::

    ujit -Ohotloop=2 -Ohotexit=100 -p- example.lua

Second trace was:

. code::

    ---- TRACE 2 start example.lua:3
    0001    KSHORT   1  17
    0002    ADD      1   0   1
    0003    RET1     1   2
    ---- TRACE 2 IR
    ....              SNAP   #0   [ ---- ---- ]
    0001 xmm7  >  flt SLOAD  #1    T
    0002 xmm7     flt ADD    0001  17
    ....              SNAP   #1   [ ---- ---- 0002 ]
    ---- TRACE 2 mcode 96
    0bd5ff1f  mov r11, 0x7f9bfd297608
    0bd5ff29  mov dword [r11], 0x2
    0bd5ff30  mov r11, 0x7f9bfd29a118
    0bd5ff3a  movsd xmm6, qword [r11]
    0bd5ff3f  cmp dword [r10+0x8], 0xfffffff2
    0bd5ff47  jnz 0xbd50010             ->0
    0bd5ff4d  movsd xmm7, qword [r10]
    0bd5ff52  addsd xmm7, xmm6
    0bd5ff56  mov dword [r10+0x18], 0xfffffff2
    0bd5ff5e  movsd [r10+0x10], xmm7
    0bd5ff64  xor eax, eax
    0bd5ff66  mov rbx, 0x7f9bfd29a44c
    0bd5ff70  mov r14, 0x7f9bfd2993d8
    0bd5ff7a  jmp 0x4c499b /* vm_exit_interp */
    ---- TRACE 2 stop -> return

``LJ_TRLINK_INTERP``
--------------------

This is a "stub" trace emitted after a number of failed attempts to compile side trace.

Uncompiled fast function example:

.. code-block:: lua

    function foo(x)
        print(x)
    end

    for i = 1, 0x30 do
        if i > 0x4 then
            foo(i)
        end
    end

.. code::

    ujit -Ohotloop=2 -Ohotexit=2 -p- example.lua

.. code::

    /* Some filed compilation */
    ---- TRACE 2 start 1/1 example.lua:7
    0010    GGET     4   1      ; "foo"
    0011    MOV      5   3
    0012    CALL     4   1   2
    0000    . FUNCF    3          ; example.lua:1
    0001    . GGET     1   0      ; "print"
    0002    . MOV      2   0
    0003    . CALL     1   1   2
    0000    . . FUNCC               ; print
    ---- TRACE 2 IR
    0001 rax      int SLOAD  #1    PI
    ....              SNAP   #0   [ ---- 0001 ---- ---- 0001 ]
    0002 [200]    fun SLOAD  #0    R
    0003 rax      tab FLOAD  0002  func.env
    0004 [200]    int FLOAD  0003  tab.hmask
    0005 rax   >  int EQ     0004  +63
    0006 [200]    p32 FLOAD  0003  tab.node
    0007 rax   >  p32 HREFK  0006  "foo" @41
    0008 rax   >  fun HLOAD  0007
    0009 [200] >  fun EQ     0008  example.lua:1
    0010 [200]    tab FLOAD  example.lua:1  func.env
    0011 [200]    int FLOAD  0010  tab.hmask
    0012 [200] >  int EQ     0011  +63
    0013 [200]    p32 FLOAD  0010  tab.node
    0014 [200] >  p32 HREFK  0013  "print" @55
    0015 [200] >  fun HLOAD  0014
    0016 [200] >  fun EQ     0015  print
    ---- TRACE 2 abort example.lua:2 -- NYI: FastFunc print

    /* After 5 failed compilations */
    ---- TRACE 2 start 1/1 example.lua:7
    ---- TRACE 2 IR
    0001 rbp      int SLOAD  #1    PI
    ....              SNAP   #0   [ ---- 0001 ---- ---- 0001 ]
    0002 xmm7     flt CONV   0001  flt.int
    ....              SNAP   #1   [ ---- 0002 ---- ---- 0002 ]
    ---- TRACE 2 mcode 78
    0bd5ff60  mov r11, 0x7fbcf88af608
    0bd5ff6a  mov dword [r11], 0x2
    0bd5ff71  xorps xmm7, xmm7
    0bd5ff74  cvtsi2sd xmm7, ebp
    0bd5ff78  mov dword [r10+0x38], 0xfffffff2
    0bd5ff80  movsd [r10+0x30], xmm7
    0bd5ff86  mov dword [r10+0x8], 0xfffffff2
    0bd5ff8e  movsd [r10], xmm7
    0bd5ff93  xor eax, eax
    0bd5ff95  mov rbx, 0x7fbcf88b2570
    0bd5ff9f  mov r14, 0x7fbcf88b13d8
    0bd5ffa9  jmp 0x4c499b
    ---- TRACE 2 stop -> interpreter

``LJ_TRLINK_UPREC`` / ``LJ_TRLINK_DOWNREC``
-------------------------------------------

An up-recursion trace is formed when a traced ``CALL`` byte code is dispatched to the same prototype as the ``CALL`` byte code that originally triggered recording (this check is performed after several unroll steps – governed by the the ``JIT_P_recunroll`` parameter).

A down-recursion trace is a companion for the up-recursion trace formed when the recursion's terminating condition warms up a side exit in the parent up-recursion trace. A side-trace is spawned, unrolled several times, checked for the down recursion type, aborted(!) and immediately restarted as a new root trace. In fact this is the case when recording can start at a byte code other than a loop or a
function prologue: down recursion traces are started at ``RET*`` byte codes.

.. code-block:: lua

    function fact(x)
        if x == 1 then
            return 1
        end

        return x * fact(x - 1)
    end

    fact(20)

.. code::

    ujit -Ohotloop=1 -Ohotexit=1 -p- example.lua

.. code::

    /* Up recursion */
    ---- TRACE 1 start example.lua:1
    0001    ISNEN    0   0      ; 1
    0005    GGET     1   0      ; "fact"
    0006    KSHORT   2   1
    0007    SUB      2   0   2
    0008    CALL     1   2   2
    0000    . FUNCF    3          ; example.lua:1
    0001    . ISNEN    0   0      ; 1
    0005    . GGET     1   0      ; "fact"
    0006    . KSHORT   2   1
    0007    . SUB      2   0   2
    0008    . CALL     1   2   2
    0000    . . FUNCF    3          ; example.lua:1
    0001    . . ISNEN    0   0      ; 1
    0005    . . GGET     1   0      ; "fact"
    0006    . . KSHORT   2   1
    0007    . . SUB      2   0   2
    0008    . . CALL     1   2   2
    0000    . . . FUNCF    3          ; example.lua:1
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ---- ]
    0001 xmm7  >  flt SLOAD  #1    T
    ....              SNAP   #1   [ ---- ---- ]
    0002       >  flt NE     0001  1
    ....              SNAP   #2   [ ---- ---- ]
    0003 rbp      fun SLOAD  #0    R
    0004 rbp      tab FLOAD  0003  func.env
    0005 r15      int FLOAD  0004  tab.hmask
    0006       >  int EQ     0005  +63
    0007 rbp      p32 FLOAD  0004  tab.node
    0008 rbp   >  p32 HREFK  0007  "fact" @41
    0009 rbp   >  fun HLOAD  0008
    0010 xmm7     flt SUB    0001  1
    0011       >  fun EQ     0009  example.lua:1
    ....              SNAP   #3   [ ---- ---- example.lua:1|---- ]
    0012       >  flt NE     0010  1
    ....              SNAP   #4   [ ---- ---- example.lua:1|0010 ]
    0013 rbp      tab FLOAD  example.lua:1  func.env
    0014 r15      int FLOAD  0013  tab.hmask
    0015       >  int EQ     0014  +63
    0016 rbp      p32 FLOAD  0013  tab.node
    0017 rbp   >  p32 HREFK  0016  "fact" @41
    0018 rbp   >  fun HLOAD  0017
    0019 xmm6     flt SUB    0010  1
    0020       >  fun EQ     0018  example.lua:1
    ....              SNAP   #5   [ ---- ---- example.lua:1|0010 example.lua:1|---- ]
    0021       >  flt NE     0019  1
    0022 xmm5     flt SUB    0019  1
    ....              SNAP   #6   [ ---- ---- example.lua:1|0010 example.lua:1|0019 example.lua:1|0022 ]
    ---- TRACE 1 mcode 488
    0bd5fe11  mov r11, 0x7f7e49cbf608
    0bd5fe1b  mov dword [r11], 0x1
    0bd5fe22  mov r11, 0x7f7e49ccd430
    0bd5fe2c  movsd xmm4, qword [r11]
    0bd5fe31  mov rbx, 0x7f7e49cc2658
    0bd5fe3b  cmp dword [r10+0x8], 0xfffffff2
    0bd5fe43  jnz 0xbd50010             ->0
    0bd5fe49  movsd xmm7, qword [r10]
    0bd5fe4e  ucomisd xmm7, xmm4
    0bd5fe52  jp 0xbd5fe5a
    0bd5fe54  jz 0xbd50014              ->1
    0bd5fe5a  mov rbp, [r10-0x10]
    0bd5fe5e  mov rbp, [rbp+0x10]
    0bd5fe62  mov r15d, [rbp+0x38]
    0bd5fe66  cmp r15d, 0x3f
    0bd5fe6a  jnz 0xbd50018             ->2
    0bd5fe70  mov rbp, [rbp+0x28]
    0bd5fe74  mov rdi, 0x7f7e49cc20e0
    0bd5fe7e  cmp rdi, [rbp+0x678]
    0bd5fe85  jnz 0xbd50018             ->2
    0bd5fe8b  cmp dword [rbp+0x680], 0xfffffffb
    0bd5fe92  jnz 0xbd50018             ->2
    0bd5fe98  add rbp, 0x668
    0bd5fe9f  cmp dword [rbp+0x8], 0xfffffff7
    0bd5fea3  jnz 0xbd50018             ->2
    0bd5fea9  mov rbp, [rbp]
    0bd5fead  subsd xmm7, xmm4
    0bd5feb1  cmp rbp, rbx
    0bd5feb4  jnz 0xbd50018             ->2
    0bd5feba  ucomisd xmm7, xmm4
    0bd5febe  jp 0xbd5fec6
    0bd5fec0  jz 0xbd5001c              ->3
    0bd5fec6  mov rbp, [rbx+0x10]
    0bd5feca  mov r15d, [rbp+0x38]
    0bd5fece  cmp r15d, 0x3f
    0bd5fed2  jnz 0xbd50020             ->4
    0bd5fed8  mov rbp, [rbp+0x28]
    0bd5fedc  mov rdi, 0x7f7e49cc20e0
    0bd5fee6  cmp rdi, [rbp+0x678]
    0bd5feed  jnz 0xbd50020             ->4
    0bd5fef3  cmp dword [rbp+0x680], 0xfffffffb
    0bd5fefa  jnz 0xbd50020             ->4
    0bd5ff00  add rbp, 0x668
    0bd5ff07  cmp dword [rbp+0x8], 0xfffffff7
    0bd5ff0b  jnz 0xbd50020             ->4
    0bd5ff11  mov rbp, [rbp]
    0bd5ff15  movaps xmm6, xmm7
    0bd5ff18  subsd xmm6, xmm4
    0bd5ff1c  cmp rbp, rbx
    0bd5ff1f  jnz 0xbd50020             ->4
    0bd5ff25  ucomisd xmm6, xmm4
    0bd5ff29  jp 0xbd5ff31
    0bd5ff2b  jz 0xbd50024              ->5
    0bd5ff31  movaps xmm5, xmm6
    0bd5ff34  subsd xmm5, xmm4
    0bd5ff38  mov r11, 0x7f7e49cbf618
    0bd5ff42  mov rax, [r11]
    0bd5ff45  mov rax, [rax+0x38]
    0bd5ff49  sub rax, r10
    0bd5ff4c  cmp rax, 0x90
    0bd5ff53  jb 0xbd50028              ->6
    0bd5ff59  mov dword [r10+0x68], 0xfffffff2
    0bd5ff61  movsd [r10+0x60], xmm5
    0bd5ff67  mov dword [r10+0x5c], 0x7f7e
    0bd5ff6f  mov dword [r10+0x58], 0x49cc25b4
    0bd5ff77  mov dword [r10+0x54], 0x7f7e
    0bd5ff7f  mov dword [r10+0x50], 0x49cc2658
    0bd5ff87  mov dword [r10+0x48], 0xfffffff2
    0bd5ff8f  movsd [r10+0x40], xmm6
    0bd5ff95  mov dword [r10+0x3c], 0x7f7e
    0bd5ff9d  mov dword [r10+0x38], 0x49cc25b4
    0bd5ffa5  mov dword [r10+0x34], 0x7f7e
    0bd5ffad  mov dword [r10+0x30], 0x49cc2658
    0bd5ffb5  mov dword [r10+0x28], 0xfffffff2
    0bd5ffbd  movsd [r10+0x20], xmm7
    0bd5ffc3  mov dword [r10+0x1c], 0x7f7e
    0bd5ffcb  mov dword [r10+0x18], 0x49cc25b4
    0bd5ffd3  mov dword [r10+0x14], 0x7f7e
    0bd5ffdb  mov dword [r10+0x10], 0x49cc2658
    0bd5ffe3  add r10, 0x60
    0bd5ffe7  mov r11, 0x7f7e49cbf620
    0bd5fff1  mov [r11], r10
    0bd5fff4  jmp 0xbd5fe11             ->LOOP
    ---- TRACE 1 stop -> up-recursion

    /* Down recursion */
    ---- TRACE 2 start 1/5 example.lua:3
    0003    . . KSHORT   1   1
    0004    . . RET1     1   2
    0009    . MUL      1   0   1
    0010    . RET1     1   2
    0009    MUL      1   0   1
    0010    RET1     1   2
    0009    MUL      1   0   1
    0010    RET1     1   2
    ---- TRACE 2 IR
    0001 rax      flt SLOAD  #3    PI
    ....              SNAP   #0   [ ---- ---- example.lua:1|0001 example.lua:1|---- ]
    0002 [200] >  flt SLOAD  #1    T
    0003 rax      flt MUL    0002  0001
    ....              SNAP   #1   [ ---- ---- 0003 ]
    0004 rax   >  p32 RETF   example.lua:1  [0x7f7e49cc25b4]
    ....              SNAP   #2   [ ---- ---- 0003 ]
    0005 [200] >  flt SLOAD  #1    T
    0006 [200]    flt MUL    0005  0003
    ---- TRACE 2 abort example.lua:6 -- down-recursion, restarting

    ---- TRACE 2 start example.lua:6
    0010    RET1     1   2
    0009    MUL      1   0   1
    0010    RET1     1   2
    0009    MUL      1   0   1
    0010    RET1     1   2
    0009    MUL      1   0   1
    0010    RET1     1   2
    ---- TRACE 2 IR
    ....              SNAP   #0   [ ---- ---- ---- ]
    0001 xmm7  >  flt SLOAD  #2    T
    ....              SNAP   #1   [ ---- ---- ---- ]
    0002       >  p32 RETF   example.lua:1  [0x7f7e49cc25b4]
    ....              SNAP   #2   [ ---- ---- 0001 ]
    0003 xmm6  >  flt SLOAD  #1    T
    0004 xmm7     flt MUL    0003  0001
    ....              SNAP   #3   [ ---- ---- 0004 ]
    0005       >  p32 RETF   example.lua:1  [0x7f7e49cc25b4]
    ....              SNAP   #4   [ ---- ---- 0004 ]
    0006 xmm6  >  flt SLOAD  #1    T
    0007 xmm7     flt MUL    0006  0004
    ....              SNAP   #5   [ ---- ---- 0007 ]
    0008       >  p32 RETF   example.lua:1  [0x7f7e49cc25b4]
    ....              SNAP   #6   [ ---- ---- 0007 ]
    0009 xmm6  >  flt SLOAD  #1    T
    0010 xmm7     flt MUL    0009  0007
    ....              SNAP   #7   [ ---- ---- 0010 ]
    ---- TRACE 2 mcode 236
    0bd5fd1e  mov r11, 0x7f7e49cbf608
    0bd5fd28  mov dword [r11], 0x2
    0bd5fd2f  cmp dword [r10+0x18], 0xfffffff2
    0bd5fd37  jnz 0xbd50010             ->0
    0bd5fd3d  movsd xmm7, qword [r10+0x10]
    0bd5fd43  mov r11, 0x7f7e49cc25b4
    0bd5fd4d  cmp r11, [r10-0x8]
    0bd5fd51  jnz 0xbd50014             ->1
    0bd5fd57  add r10, 0xffffffffffffffe0
    0bd5fd5b  mov r11, 0x7f7e49cbf620
    0bd5fd65  mov [r11], r10
    0bd5fd68  cmp dword [r10+0x8], 0xfffffff2
    0bd5fd70  jnz 0xbd50018             ->2
    0bd5fd76  movsd xmm6, qword [r10]
    0bd5fd7b  mulsd xmm7, xmm6
    0bd5fd7f  mov r11, 0x7f7e49cc25b4
    0bd5fd89  cmp r11, [r10-0x8]
    0bd5fd8d  jnz 0xbd5001c             ->3
    0bd5fd93  add r10, 0xffffffffffffffe0
    0bd5fd97  mov r11, 0x7f7e49cbf620
    0bd5fda1  mov [r11], r10
    0bd5fda4  cmp dword [r10+0x8], 0xfffffff2
    0bd5fdac  jnz 0xbd50020             ->4
    0bd5fdb2  movsd xmm6, qword [r10]
    0bd5fdb7  mulsd xmm7, xmm6
    0bd5fdbb  mov r11, 0x7f7e49cc25b4
    0bd5fdc5  cmp r11, [r10-0x8]
    0bd5fdc9  jnz 0xbd50024             ->5
    0bd5fdcf  add r10, 0xffffffffffffffe0
    0bd5fdd3  mov r11, 0x7f7e49cbf620
    0bd5fddd  mov [r11], r10
    0bd5fde0  cmp dword [r10+0x8], 0xfffffff2
    0bd5fde8  jnz 0xbd50028             ->6
    0bd5fdee  movsd xmm6, qword [r10]
    0bd5fdf3  mulsd xmm7, xmm6
    0bd5fdf7  mov dword [r10+0x18], 0xfffffff2
    0bd5fdff  movsd [r10+0x10], xmm7
    0bd5fe05  jmp 0xbd5fd1e             ->LOOP
    ---- TRACE 2 stop -> down-recursion

``LJ_TRLINK_TAILREC``
---------------------

Recurring tail call. In |PROJECT| ``CALLT`` bytecode doesn't create new stack frame for tail call.

.. code-block:: lua

    function bar(x)
        if (x < 0) then return 0 end
        return bar(x - 1)
    end

    bar(5)

.. code::

    ujit -Ohotloop=2 -p- example.lua

.. code::

    ---- TRACE 1 start example.lua:1
    0001    KSHORT   1   0
    0002    ISGE     0   1
    0003    JMP      1 => 0006
    0006    GGET     1   0      ; "bar"
    0007    KSHORT   2   1
    0008    SUB      2   0   2
    0009    CALLT    1   2
    0000    FUNCF    3          ; example.lua:1
    0001    KSHORT   1   0
    0002    ISGE     0   1
    0003    JMP      1 => 0006
    0006    GGET     1   0      ; "bar"
    0007    KSHORT   2   1
    0008    SUB      2   0   2
    0009    CALLT    1   2
    0000    FUNCF    3          ; example.lua:1
    0001    KSHORT   1   0
    0002    ISGE     0   1
    0003    JMP      1 => 0006
    0006    GGET     1   0      ; "bar"
    0007    KSHORT   2   1
    0008    SUB      2   0   2
    0009    CALLT    1   2
    0000    FUNCF    3          ; example.lua:1
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ---- ]
    0001 xmm2  >  flt SLOAD  #1    T
    ....              SNAP   #1   [ ---- ---- ]
    0002       >  flt UGE    0001  0
    ....              SNAP   #2   [ ---- ---- ]
    0003 r14      fun SLOAD  #0    R
    0004 r12      tab FLOAD  0003  func.env
    0005 r13      int FLOAD  0004  tab.hmask
    0006       >  int EQ     0005  +63
    0007 r10      p32 FLOAD  0004  tab.node
    0008 r9    >  p32 HREFK  0007  "bar" @41
    0009 r8    >  fun HLOAD  0008
    0010 xmm2     flt SUB    0001  1
    0011       >  fun EQ     0009  example.lua:1
    ....              SNAP   #3   [ example.lua:1|---- ]
    0012       >  flt UGE    0010  0
    ....              SNAP   #4   [ example.lua:1|0010 ]
    0013 rbp      tab FLOAD  example.lua:1  func.env
    0014 rsi      int FLOAD  0013  tab.hmask
    0015       >  int EQ     0014  +63
    0016 rbx      p32 FLOAD  0013  tab.node
    0017 rdx   >  p32 HREFK  0016  "bar" @41
    0018 rax   >  fun HLOAD  0017
    0019 xmm7     flt SUB    0010  1
    0020       >  fun EQ     0018  example.lua:1
    ....              SNAP   #5   [ example.lua:1|---- ]
    0021       >  flt UGE    0019  0
    0022 xmm7   + flt SUB    0019  1
    ....              SNAP   #6   [ example.lua:1|0022 ]
    0023 ------------ LOOP ------------
    ....              SNAP   #7   [ example.lua:1|0022 ]
    0024       >  flt UGE    0022  0
    0025 xmm7     flt SUB    0022  1
    ....              SNAP   #8   [ example.lua:1|0022 ]
    0026       >  flt UGE    0025  0
    0027 xmm7     flt SUB    0025  1
    ....              SNAP   #9   [ example.lua:1|0022 ]
    0028       >  flt UGE    0027  0
    0029 xmm7   + flt SUB    0027  1
    0030 xmm7     flt PHI    0022  0029
    0031 xmm6     nil RENAME 0022  #8
    ---- TRACE 1 mcode 336
    0bd5feaf  mov r11, 0x7f886b930608
    0bd5feb9  mov dword [r11], 0x1
    0bd5fec0  xorps xmm1, xmm1
    0bd5fec3  mov r11, 0x7f886b93e480
    0bd5fecd  movsd xmm0, qword [r11]
    0bd5fed2  mov rcx, 0x7f886b9331b8
    0bd5fedc  cmp dword [r10+0x8], 0xfffffff2
    0bd5fee4  jnz 0xbd50010             ->0
    0bd5feea  movsd xmm2, qword [r10]
    0bd5feef  ucomisd xmm1, xmm2
    0bd5fef3  ja 0xbd50014              ->1
    0bd5fef9  mov r14, [r10-0x10]
    0bd5fefd  mov r12, [r14+0x10]
    0bd5ff01  mov r13d, [r12+0x38]
    0bd5ff06  cmp r13d, 0x3f
    0bd5ff0a  jnz 0xbd50018             ->2
    0bd5ff10  mov r10, [r12+0x28]
    0bd5ff15  mov rdi, 0x7f886b9330b0
    0bd5ff1f  cmp rdi, [r10+0x678]
    0bd5ff26  jnz 0xbd50018             ->2
    0bd5ff2c  cmp dword [r10+0x680], 0xfffffffb
    0bd5ff34  jnz 0xbd50018             ->2
    0bd5ff3a  lea r9, [r10+0x668]
    0bd5ff41  cmp dword [r9+0x8], 0xfffffff7
    0bd5ff46  jnz 0xbd50018             ->2
    0bd5ff4c  mov r8, [r9]
    0bd5ff4f  subsd xmm2, xmm0
    0bd5ff53  cmp r8, rcx
    0bd5ff56  jnz 0xbd50018             ->2
    0bd5ff5c  ucomisd xmm1, xmm2
    0bd5ff60  ja 0xbd5001c              ->3
    0bd5ff66  mov rbp, [rcx+0x10]
    0bd5ff6a  mov esi, [rbp+0x38]
    0bd5ff6d  cmp esi, 0x3f
    0bd5ff70  jnz 0xbd50020             ->4
    0bd5ff76  mov rbx, [rbp+0x28]
    0bd5ff7a  mov rdi, 0x7f886b9330b0
    0bd5ff84  cmp rdi, [rbx+0x678]
    0bd5ff8b  jnz 0xbd50020             ->4
    0bd5ff91  cmp dword [rbx+0x680], 0xfffffffb
    0bd5ff98  jnz 0xbd50020             ->4
    0bd5ff9e  lea rdx, [rbx+0x668]
    0bd5ffa5  cmp dword [rdx+0x8], 0xfffffff7
    0bd5ffa9  jnz 0xbd50020             ->4
    0bd5ffaf  mov rax, [rdx]
    0bd5ffb2  movaps xmm7, xmm2
    0bd5ffb5  subsd xmm7, xmm0
    0bd5ffb9  cmp rax, rcx
    0bd5ffbc  jnz 0xbd50020             ->4
    0bd5ffc2  ucomisd xmm1, xmm7
    0bd5ffc6  ja 0xbd50024              ->5
    0bd5ffcc  subsd xmm7, xmm0
    -> LOOP:
    0bd5ffd0  ucomisd xmm1, xmm7
    0bd5ffd4  ja 0xbd5002c              ->7
    0bd5ffda  movaps xmm6, xmm7
    0bd5ffdd  subsd xmm7, xmm0
    0bd5ffe1  ucomisd xmm1, xmm7
    0bd5ffe5  ja 0xbd50030              ->8
    0bd5ffeb  subsd xmm7, xmm0
    0bd5ffef  ucomisd xmm1, xmm7
    0bd5fff3  ja 0xbd50034              ->9
    0bd5fff9  subsd xmm7, xmm0
    0bd5fffd  jmp 0xbd5ffd0             ->LOOP
    ---- TRACE 1 stop -> tail-recursion

``LJ_TRLINK_NONE``
------------------

**Trace compilation aborted / loop optimizations failed.**
