.. _vm-layout:

Layout of VM Internals
======================

.. contents:: :local:

Interpreter Frame
-----------------

This layout is also marshaled as ``struct cframe``.

.. container:: table-wrap

    ========= ==================================
    Offset    VARIABLE (64 bits by default)
    ========= ==================================
    ``+0x78`` ``SAVE_RET``
    ``+0x70`` ``SAVE_RBP``
    ``+0x68`` ``SAVE_RBX``
    ``+0x60`` ``SAVE_R15``
    ``+0x58`` ``SAVE_R14``
    ``+0x50`` ``SAVE_CFRAME``
    ``+0x48`` ``SAVE_PC``
    ``+0x40`` ``SAVE_L``
    ``+0x38`` ``SAVE_ERRF``
    ``+0x30`` ``SAVE_NRES``
    ``+0x2c`` ``MULTRES`` (``uint32_t``)
    ``+0x28`` ``SAVE_VMSTATE`` (``int32_t``)
    ``+0x20`` ``TMPTV_TAG``
    ``+0x18`` ``TMPTV``
    ``+0x10`` ``TMPa2``
    ``+0x08`` ``TMPa``
    ``+0x04`` ``TMP2`` (``uint32_t``)
    ``$rsp``  ``TMP1`` (``uint32_t``)
    ========= ==================================

Register Mapping
----------------

.. container:: table-wrap

    ============= ================== ================= ==================== ======================
    Host Register Host Callee-saved? VM State Register VM Op / Aux Register VM C Argument Register
    ============= ================== ================= ==================== ======================
    ``%rax``      ``NO``                               ``RCa``, ``RDa``     ``CRET``
    ``%rbx``      ``YES``            ``PC``
    ``%rcx``      ``NO``                               ``RAa``              ``CARG4``
    ``%rdx``      ``NO``                                                    ``CARG3``
    ``%rbp``      ``YES``            ``OP``            ``RBa``
    ``%rsp``      ``YES``
    ``%rsi``      ``NO``                                                    ``CARG2``
    ``%rdi``      ``NO``                                                    ``CARG1``
    ``%r8``       ``NO``                               ``AUX2``             ``CARG5``
    ``%r9``       ``NO``                               ``AUX1``             ``CARG6``
    ``%r10``      ``NO``             ``BASE``
    ``%r11``      ``NO``                               ``XCHG``
    ``%r12``      ``YES``
    ``%r13``      ``YES``
    ``%r14``      ``YES``            ``DISPATCH``
    ``%r15``      ``YES``            ``KBASE``
    ``%xmm0``     ``NO``                                                    ``CARG1f``
    ============= ================== ================= ==================== ======================

Notes:

- Internal VM op registers ``RA``, ``RB`` and ``RC`` (aliased with ``RD``) correspond to 32-bit host registers ``%ecx``, ``%ebp`` and ``%eax``, respectively.
- 32-bit registers for C arguments are denoted as ``CARG*d`` and correspond to the same host registers as their 64-bit versions.
