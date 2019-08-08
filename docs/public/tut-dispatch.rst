.. _tut-dispatch:

Tutorial: Dispatch Table Layout
===============================

.. contents:: :local:

Overview
---------

Dispatch table is a static array located in struct ``GG_State``. It holds pointers to VM bytecode functions (semantics for all bytecodes are implemented directly in assembler in vm_x86.dasc):

.. code::

    ASMFunction dispatch[GG_LEN_DISP]; /* Instruction dispatch tables. */

When VM wants to execute the next instruction, ins_next macro determines it's opcode and performs jump to the pointer form dispatch table:

.. code::

    |.macro ins_next
    |  _ins_next /* Get OPcode of the next bytecode */
    |  jmp qword [DISPATCH + OP * 8] /* 8 is a size of a pointer on x86_64 */
    |.endmacro

Layout
------

Dispatch table has 2 parts: dynamic and static.

Dynamic part holds pointers for all bytecodes and assembler fast functions, that can be overwritten by some "proxy" functions is some cases: recording, hooks, etc.

Static part, like a dynamic, holds pointers to bytecodes except function prologues and fast functions. They can't be modified during execution (not quite, hotcounting instructions can be switched to force interpreter mode).

.. code::

    #define GG_LEN_DDISP (BC__MAX + GG_NUM_ASMFF) /* Dynamic part, all bytecodes (see lj_bc.h) + asm ffunc's */
    #define GG_LEN_SDISP BC_FUNCF /* Static part, bytecodes without prologues */
    #define GG_LEN_DISP  GG_LEN_DDISP + GG_LEN_SDISP) /* Result: dynamic part (bytecodes + asm ffuncs), static part */

Initializing and Dispatch Modes
-------------------------------

There are two significant functions:

- **lj_dispatch_init**: used for initializing dispatch table. Set default bytecode semantics for dynamic and static parts, and disable hotcounting instructions (use force interpreter flag).
- **lj_dispatch_update**: updates table if dispatch mode has changed.

    * For dynamic part:

        1. return hook: replaces BC_RET* bytecodes with vm_rethook function.
        2. instruction hook: replaces all instructions that located before BC_FUNCF with vm_inshook.
        3. call hook: replaces BC_*FUNC* prologues with vm_callhook.
        4. recording: same as instruction hook, but using vm_record.

    * For static part:

        1. Hotcounting instructions can be changed to force interpreted versions depending on JIT and RECORD states.

Adding New Byte Codes
---------------------
New bytecode must be added before prologues (BC_FUNCF*) in lj_bc.h. See How to add a new bytecode for more info.
