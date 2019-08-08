.. _add-new-bytecode:

Add a New Bytecode
==================

1. Add new opcode to the ``src/lj_bc.h``.
-----------------------------------------

For example:

.. code-block:: c

    /* 0xNUM */ _(NEWBC, ___, ___, ___, ___) \


Max mnemonic length is 6 symbols. New BC should be added before ``FUNCF`` (see ``src/lj_dispatch.h:35``:``#define GG_LEN_SDISP BC_FUNCF``).

**TODO: _macro**

2. Add new bytecode to JIT recorder (``src/jit/lj_record.c``)
-------------------------------------------------------------

a) if instruction needs to be skipped, just add:

.. code-block:: c

    case BC_NEWBC:
    break;

b) NYI case (make bytecode to abort recording). Go to the:

.. code-block:: c

    /* fallthrough */
    case BC_ITERN:
    case BC_ISNEXT:
    case BC_UCLO:
    case BC_FNEW:
    case BC_TSETM:
      lj_trace_err_info_op(J, LJ_TRERR_NYIBC, (int32_t)op);
    break;

and put new BC here.

3. Add BC to VM (``vm_x86.dasc``)
---------------------------------

.. code-block:: c

    case BC_NEWBC:
    /* Add semantics here*/
    |  ins_next
    break;

**TODO: ins_xxx**.