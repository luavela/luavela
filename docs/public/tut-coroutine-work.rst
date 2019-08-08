.. _tut-coroutine-work:

Tutorial: How ``coroutine.resume`` and ``coroutine.yield`` Work
===============================================================

.. contents:: :local:

Introduction
------------

This article discusses implementation of built-in functions ``coroutine.resume`` and ``coroutine.yield``. Before going further, please make sure you understand following ideas:

- There are two kinds of stack frames in |PROJECT|:

1. *Host stack frames* are placed on x64 stack and comply with x64 ABI. They are used for running byte-code inside the VM's interpreter (VM frames) and JIT-compiled code (JIT frames). VM frames and JIT frames have different layout, this is beyond the scope of this article. For now it is enough to remember just one thing: each Lua coroutine is run in a separate VM frame.
2. *Guest stack frames* are placed on stacks of Lua states (Lua stacks) and correspond to call-graph of Lua code.

- Each Lua coroutine has its own Lua stack, which grows from lower addresses to higher addresses.

Please note also that following notation will used throughout the article:

            -  L: coroutine that resumes another coroutine
            -  L1: coroutine that is resumed by L; this coroutine can yield and transfer control back to L (after that L can resume L1 again, of course)
            -  Functional object passed to ``coroutine.create`` is called coroutine's main function.
            -  Notation of frame link layout (``func``, ``tp``) follows declarations in lj_obj.h.

coroutine.resume: Preparations
------------------------------

As soon as ``coroutine.resume(L1, arg1, arg2, ...)`` starts executing in context of L, the Lua stack of L looks like this:

.. code::

                     TOP       |\\\\\\\\\\\\\| <-- first free slot on the stack
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- arguments to be passed to L1
                     ...       |   TValue    | {
                     BASE      |   TValue    | <-- L1
                     BASE - 1  |  tp  | func | <-- tp points to the coroutine.resume's caller; func is coroutine.resume's functional object

First ``coroutine.resume`` runs sanity checks, e.g.:

            -  L1 must be a thread.
            -  L1 must by yield-able.
            -  Slot at L1->base must be defined.
            -  L1's stack must be large enough to accept all arguments
               passed from L.

If all these checks pass, following happens:

            -  L's base is moved one slot up to ensure that L1 is not wiped out by GC
            -  All values passed as arguments to ``coroutine.resume``in context of ``L`` are copied to ``L1``'s stack

After that, the interpreter executes ``call ->vm_resume`` (this DynASM symbol will be converted at compile time to the real symbol ``lj_vm_resume``). ``lj_vm_resume`` constructs  new host VM frame to run L1, initializes the frame with some data and actually resumes L1. Please note that we have not drawn L1's stack layout so far. This was done deliberately, as "actual resuming" of L1 can happen in two modes (both are handled by ``lj_vm_resume``):

1. The very first resume of L1. Actually this could have been called "coroutine start", but there is no corresponding API, so the term "resume" is used for this case, too.
2. Resume after L1 yielded.

coroutine.resume: Resuming for the First Time
---------------------------------------------

If we are resuming a coroutine for the very first time, its main function has not been started, this this our task to start its execution now. L1's stack can look as follows in this case:

.. code::

                     TOP       |\\\\\\\\\\\\\| <-- first free slot on the stack
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- arguments passed from L via coroutine.resume(L1, ...)
                     ...       |   TValue    | {
                     BASE      |   TValue    | <-- L1's main function
                     BASE - 1  |  0   |  L   | <-- L1's stack bottom

After that L1's main function is started with a regular ``lj_vm_call_*`` path.

.. note::

    This paragraph has to be extended.

coroutine.yield: Back to L
--------------------------

Let's assume that once started, L1's main function executes some code and decides to call ``coroutine.yield``. As soon as ``coroutine.yield(arg1, arg2, ...)`` starts executing in context of L1, the Lua stack of L1 looks like this:

.. code::

            TOP       |\\\\\\\\\\\\\| <-- first free slot on the stack
            ...       |   TValue    | {
            ...       |   TValue    | { <-- arguments passed from L1 via coroutine.yield(...)
            BASE      |   TValue    | {
            BASE - 1  |  tp  | func | <-- tp points to the coroutine.yield's caller; func is coroutine.yield's functional object
            ...       |   TValue    | {
            ...       |   TValue    | { <-- slots occupied by coroutine.yield's caller during its work
            ...       |   TValue    | {
            CBASE     |   TValue    | <-- BASE of coroutine.yield's caller
            CBASE - 1 |  tp  | func | <-- tp points to the coroutine.yield caller's caller; func is coroutine.yield caller's functional object
            ...       |   TValue    | {
            ...       |   TValue    | { <-- arbitrary number of guest stack frames
            ...       |   TValue    | {
            BOTTOM + 1|  tp  | func | <-- tp points to BOTTOM; func is main function's functional object
            BOTTOM    |  0   |  L   | <-- L1's stack bottom

The implementation of ``coroutine.yield`` does following:

1. Syncs in-register values used inside the interpreter with ``L1->base`` and ``L1->top``.
2. Sets ``L1->status`` to ``LUA_YIELD``.
3. Executes ``ret`` which destroys the host VM frame created for running L1 and returns control to the next instruction after ``call ->vm_resume`` in ``create.resume``.

.. note::

    Please note that L1's guest frame is not modified in any manner during ``coroutine.yield``.

Stitching coroutine.yield and coroutine.resume
----------------------------------------------

As it was shown in the previous paragraph, after L1 has yielded, control is transferred to create.resume in context of L. Following is done after this:

1. Values passed as arguments to ``coroutine.yield`` in context of L1 are copied from L1's stack to L's stack.
2. Values passed as arguments to ``coroutine.yield`` in context of L1 are removed from L1's stack.
3. ``coroutine.resume`` transfers control back to its caller returning all arguments it received from ``coroutine.yield``.

After all this is done, L1's stack looks like this:

.. code::

                     BASE|TOP  |\\\\\\\\\\\\\| <-- first free slot on the stack
                     BASE - 1  |  tp  | func | <-- tp points to the coroutine.yield's caller; func is coroutine.yield's functional object
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- slots occupied by coroutine.yield's caller during its work
                     ...       |   TValue    | {
                     CBASE     |   TValue    | <-- BASE of coroutine.yield's caller
                     CBASE - 1 |  tp  | func | <-- tp points to the coroutine.yield caller's caller; func is coroutine.yield caller's functional object
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- arbitrary number of guest stack frames
                     ...       |   TValue    | {
                     BOTTOM + 1|  tp  | func | <-- tp points to BOTTOM; func is main function's functional object
                     BOTTOM    |  0   |  L   | <-- L1's stack bottom

At this point, the first "resume/yield" cycle for L1 is over. Let's see what happens if L decides to resume L1 one more time.

coroutine.resume: Resuming a Yielded Coroutine
----------------------------------------------

When we resume an already yielded coroutine, preparation steps are exactly the same as it was described above. The first different thing happens inside ``lj_vm_resume``. Let's see how L1's stack looks after ``coroutine.resume`` has copied all its arguments to L1's stack:

.. code::

                     TOP       |\\\\\\\\\\\\\| <-- first free slot on the stack
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- arguments passed from L via coroutine.resume(L1, ...)
                     BASE      |   TValue    | {
                     BASE - 1  |  tp  | func | <-- tp points to the coroutine.yield's caller; func is coroutine.yield's functional object
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- slots occupied by coroutine.yield's caller during its work
                     ...       |   TValue    | {
                     CBASE     |   TValue    | <-- BASE of coroutine.yield's caller
                     CBASE - 1 |  tp  | func | <-- tp points to the coroutine.yield caller's caller; func is coroutine.yield caller's functional object
                     ...       |   TValue    | {
                     ...       |   TValue    | { <-- arbitrary number of guest stack frames
                     ...       |   TValue    | {
                     BOTTOM + 1|  tp  | func | <-- tp points to BOTTOM; func is main function's functional object
                     BOTTOM    |  0   |  L   | <-- L1's stack bottom

After L1's stack is set up, lj_vm_resume enforces a return from ``coroutine.yield`` by "calling" a part of ``BC_RET`` semantics. Control is transferred to the ``coroutine.yield``'s caller and L1 resumes execution from the byte code instruction that follows a call to ``coroutine.yield``.
