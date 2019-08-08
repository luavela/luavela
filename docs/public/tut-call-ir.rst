.. _tut-call-ir:

Tutorial: CALL* IR Instructions
===============================

.. contents:: :local:

Introduction
------------

JIT recorder may benefit from the fact that bytecode or fast function semantics (or some parts of it) is already implemented via C function. In order to do that, the following IRs can be used:

               ===== ==== ======= ===============================
               OP    Left Right   Description
               ===== ==== ======= ===============================
               CALLN args #ircall Call internal function (normal)
               CALLL args #ircall Call internal function (load)
               CALLS args #ircall Call internal function (store)
               CARG  args arg     Call argument extension
               ===== ==== ======= ===============================

.. note::

    Note that this tutorial does not cover ``CALLXS``, deferred until at least FFI is enabled.

``IRCALLDEF`` macro
-------------------

A function to call should be registered in ``src/jit/lj_ircall.h`` file in ``IRCALLDEF`` macro:

.. code-block:: c

            #define IRCALLDEF(_) \
            _(ANY,        uj_str_cmp,             2,         N, INT, 0) \
            _(ANY,        uj_str_new,             3,         S, STR, CCI_L) \
            ...
            _(FFI,        lj_crecord_errno,       0,         S, INT, CCI_NOFPRCLOBBER)
            \
            /* End of list. */

``IRCALLDEF`` arguments:

   -  Condition. Used for conditional inclusion of functions.
      Currently can be ANY or FFI.
   -  Function name.
   -  Number of function arguments. Redefine ``CCI_NARGS_MAX`` if
      you use functions with more than 32 non-vararg arguments.
   -  Call IR type. Can be ``N`` (for normal), ``L`` (for load) or ``S``
      (for store). (See below for details.)
   -  Type of IR reference corresponding to the function return
      type. Use ``NIL`` for ``void``, ``INT`` for ``int32_t``/``uint32_t``, ``NUM`` for
      ``double``, ``TAB`` for ``GCtab*``, ``PTR`` for generic pointers, etc.
   -  Set of flags:

      -  ``CCI_L``. Used for functions which have lua_State\* as
         the first argument.
      -  ``CCI_CASTU64``. Used when reference to IR with type NUM
         is actually provided by uint64_t function return type.
         (Used only for ``lj_math_random_step`` currently.)
      -  ``CCI_NOFPRCLOBBER``. Used when function is assumed not to
         use SSE/AVX/etc. registers. Use it when function 1)
         doesn't use floating point operations, 2) is not
         auto-vectorized by host C compiler and 3) don't call
         other functions that violate 1) or 2) (also double
         check the function with disassembler). If you don't
         use the flag for function that satisfy requirements,
         assembler will likely generate sub-optimal code. If
         you use the flag for function that actually clobbers
         FP registers, assembler will likely generate incorrect
         code.
      -  ``CCI_VARARG``. Used for functions with variable
         arguments. (Not used currently.)
      -  ``CCI_IMMUTABLE``. Used for functions that make their
         argument immutable. Should be also ``CALLS``.
      -  ``CCI_ALLOC``. Used when function allocates garbage
         collectible object.

Function declared inside ``IRCALLDEF`` macro will have its own identifier ``IRCALL_##name`` which can be a ``CALL*`` IR argument.

Function call types
--------------------

Rule of thumb for choosing right call type:

.. tip::

    N: ``output ircall(const input arg)`` where ``input`` is anything but ``GCtab *`` and ``output`` is non-void.

    L: ``output ircall(const GCtab *tab)`` where ``output`` is non-void.

    S: ``output ircall(GCtab *tab)``.

Another way to choose correct type is to consider what optimizations would be applied to ``CALL*`` instruction:

   -  N calls can be commoned and eliminated like most
      arithmetic operations. In particular, they are subject
      for common subexpression elimination (CSE) and for loop
      invariant code motion (as it is based on CSE). Also CALLN
      with unused result (and all CALLN that return NIL) will
      be definitely eliminated by dead-code elimination
      optimization (DCE). If this behavior is not expected you
      may either check function call result in a guard
      assertion, write custom folding rule for this function
      call or consider choosing another call type.
   -  L calls are assumed to be a subject of alias analysis as
      LOADs in general. It means in particular that

      -  CALLLs bypass common CSE as other loads. Instead
         CALLLs are subject for specifically crafted FWD
         optimizations, as it is done for ``lj_tab_len``
         calls. In case there are no special FWD optimizations
         for particular function, its CALLLs will be simply
         emitted. As a consequence, loop invariant code motion
         will also not happen for such IRs.
      -  Conflicting CALLL will prevent STORE from dead-store
         elimination (DSE).
      -  CALLLs (that were not previously optimized out) and
         their arguments are not eliminated by allocation and
         store sinking (SINK) optimization.
      -  CALLLs may be eliminated by DCE if result is proved to
         be unused (like CALLN ones).

   -  S calls are assumed to have side effects (may be
      non-visible to JIT optimizer) and hence could not be
      eliminated. CALLS and its arguments cannot be eliminated
      by SINK. And as for other stores, recorder will add a
      snapshot after recording CALLS.

Recording with CALL\* IRs
-------------------------

In order to emit CALL\* IR use ``lj_ir_call``:

.. code-block:: c

   TRef tab = ...;
   TRef len = lj_ir_call(J, IRCALL_lj_tab_len, tab);

``lj_ir_call`` makes a work for you to emit CARG IRs for function arguments and CALL IR itself. It also implicitly passes ``lua_State *L`` argument if ``CCI_L`` flag was specified.

Please note that:

   -  One should not throw errors from inside IR_CALL\*
      functions. Remember that traces have no unwinding
      information. If you need to throw an error, you may guard
      IR_CALL result on error conditions (additional :ref:`fold rules <tut-folding-engine>`
      may be required to prevent those guards from
      'dropfolding') and throw error in interpreter. Throwing
      an error from IR_CALL function leads to unprotected error
      PANIC unwinding, which is not what you want in most
      cases. Perhaps, the only exception from the rule is 'not
      enough memory' error.
   -  One may want to make CALL\* IR functions as small as
      possible and put as much work as possible into existing
      IRs (especially for loads and stores). Motivation: even
      if ``IR_CALL`` itself cannot be a subject for optimizations,
      accompanied IRs still may be DCEd, CSEd or moved out from
      the loop. 

Let's consider ``table.concat`` recording as an example:

Preferable:

.. code::

            BUF    RESET global
            CALLL  lj_tab_concat  (tab sep start end [NULL])
            BUFSTR global

Not so preferable, but simple:

.. code::

   CALLL  lj_tab_concat  (tab sep start end [NULL]) // performs buffer initialization and conversion to string inside lj_tab_concat call

Calls to C function may be generated not only as IR_CALL\* result, but as a part of semantics of other IRs, such as ``TNEW``, ``SNEW``, ``TDUP``, etc.  One may even find IRCALLDEFs for such functions inconsistent in their types and flags. This is kind of ok as long as they never called through CALL\* IRs, although one should choose the semantically closest definitions for new IRCALLDEFs.
