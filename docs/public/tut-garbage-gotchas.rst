.. _tut-garbage-gotchas:

Tutorial: Garbage Collector Gotchas
===================================

.. contents:: :local:

Which Garbage Collector is used in |PROJECT|?
---------------------------------------------

|PROJECT| uses tri-color incremental mark and sweep garbage collector (also used in Lua 5.1, LuaJIT 1.x and LuaJIT 2.0). Each time Garbage collector (from here and after - GC) is called, it performs a limited amount of incremental GC *steps* moving between some fixed set of GC *states*. The state is obviously preserved between GC invocations. A complete path from the initial state to the final state forms a full GC *cycle*.

How Does Garbage Collector Start?
---------------------------------

|PROJECT|'s virtual machine is single-threaded, and garbage collector is started synchronously at some fixed points, a more or less comprehensive list of these points can be found with a following command:

.. code::

    [.../ujit/src]$ fgrep -RI -e _gc_step -e _gc_check . | fgrep -v -e buildvm_arch.h -e lj_vm.s -e _gc.c -e _gc.h

A brief description of functions:

    ====================== =========================================================================================================
    Function               Description
    ====================== =========================================================================================================
    ``lj_gc_step``         The main entry point to GC.
    ``lj_gc_step_jit``     Ditto, but called from JIT-compiled code, which means some extra setup before entering GC.
    ``lj_gc_check``        Checks if it is time to run GC. If it is (see below), calls ``lj_gc_step``, otherwise does nothing.
    ``lj_gc_check_fixtop`` Ditto, but called from the interpreter, which requires explicit synchronization of coroutine's stack top.
    ====================== =========================================================================================================

When Does Garbage Collector Start?
-----------------------------------

GC starts only if the total amount of currently allocated memory (allocator's fragmentation not counted) exceeds a certain threshold. By default this threshold is set for the next GC cycle at the end of the previous cycle using formula

.. code::

    ``threshold = mem_total * (pause / 100)``

where ``pause`` is a memory growth factor, expressed in percentage units (naming is taken from the `Lua 5.1 Reference Manual <https://www.lua.org/manual/5.1/>`__), so the formula reads as follows: Start the next GC cycle when the amount of allocated memory changes ``(pause / 100)`` times compared to the current value.

When Does Garbage Collector Return Control?
-------------------------------------------

Once invoked, GC starts to execute GC steps until some limit is reached. This limit is calculated using formula

.. tip::

           ``limit = GCSTEPSIZE * (stepmul / 100)``

where ``GCSTEPSIZE`` is some fixed constant (measured in bytes) and ``stepmul`` (aka step multiplier) is a step duration factor, expressed in percentage units (naming is taken from
the `Lua 5.1 Reference Manual <https://www.lua.org/manual/5.1/>`__), so the formula reads as follows: Execute GC steps until ``limit`` bytes are reported to be processed. As soon this limit is reached, GC returns control to the caller, and the platform resumes the operation interrupted by the GC invocation.

What Are Garbage Collector States?
----------------------------------

   =============== ================================================================================================================================================================================================================
   State           Description
   =============== ================================================================================================================================================================================================================
   ``pause``       Initial / Final state.
   ``propagate``   Mark phase of the GC cycle.
   ``atomic``      Transitioning from mark to sweep phase.
   ``sweepstring`` Sweep phase of the GC cycle, strings only.
   ``sweep``       Sweep phase of the GC cycle, all collectible object types except strings.
   ``finalize``    Execution of finalizers for userdata (aka ``__gc`` metamethods) before they are swept for good. See `here <Lua-5.1-Reference-Manual_106563971.html#Lua5.1ReferenceManual-2.10–GarbageCollection>`__ for details.
   =============== ================================================================================================================================================================================================================

Can Coroutine's Stack Shrink During Garbage Collection?
-------------------------------------------------------

Yes. If during the mark phase GC detects that coroutine's stack is too big, it will be shrunk. Although this operation does not typically move the stack to a new location in memory, **never ever rely on this behavior** because the allocator provides absolutely no guarantees about it.

Can Coroutine's Stack Grow During Garbage Collection?
-----------------------------------------------------

**Yes.** If GC cycle moves through the ``finalize`` state, an arbitrary code may be executed. One of possible side effects may be stack growth. Please note also that while finalizers are executed, garbage collector is turned off.

Using pointers to TValue's Across GC-checkpoints
------------------------------------------------

.. code-block:: c

    TValue *base = L->base;

    /* any call that can check/invoke GC under the hood, i.e. (pseudo-code): */
    TValue *top = uj_meta_cat(...);

    /* OOPS! base may (or may not) be invalidated by stack resize, happy debugging! */

Instead, use ``uj_..._stack_save`` / ``uj_..._stack_restore``.

How Many White Colors Do We Use Actually?
------------------------------------------

**Two**. This is somewhat explained in the `original LuaJIT paper <http://wiki.luajit.org/New-Garbage-Collector>`__:

The sweep phase can be made incremental by using two whites and flipping between them just before entering the sweep phase. Objects with the 'current' white need to be kept. Only objects with the 'other' white should be freed.

So we always have two white colors, a "current white" and an "other white", with following properties:

#. All newly allocated objects are marked with the current white.
#. Before entering the sweep phase (i.e. in the non-interruptible atomic phase) the whites are flipped, i.e. the "current white" becomes the "other white", and vice versa.
#. During the sweep phase, following logic applies:

    - If the object is black, it is re-marked as "current white" and preserved (obviously).
    - If the object is "current white", it is preserved(these are objects allocated in-between runs of the sweep phase).
    - If the object is "other white" (i.e. it was "current white" before the atomic phase), it is freed.

How Many Black Colors Do We Use Actually (aka What is ``GC_FIXED``)?
--------------------------------------------------------------------

One and a half. We have one "true" black color which means that the object has been recursively traversed by the GC during the mark/propagate phase and is reachable. And here is another blackish entity called ``GC_FIXED`` which is used to mark things that must never ever be purged by the GC even if they formally may not be reachable. The most frequent case are tokens reserved by the language. Appending ``GC_FIXED`` to the "current white" (see above) allows us to treat any fixed object as "current white" regardless of which of two white colors is current at the moment.

How Does Garbage Collector Traverse Objects?
--------------------------------------------

List of All Collectible Objects (Except Strings)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This list is anchored at ``global_State.root`` and allows to traverse all collectible objects allocated within given ``global_State``, except strings. Initially the list consists of a single element, the main coroutine (aka ``mainL``), subsequent stores preserve following invariant:

.. code::

    ANCHOR->object1->object2->...->mainL->userdata1->userdata2->...->sealed1->...->sealed2->...->NULL

This list is used in following cases, among others:

    -  Sweep phase.
    -  Separation of ``userdata`` for finalization.

String Hashes
^^^^^^^^^^^^^^

Two string hashes, ``global_State.strhash`` and ``global_State.strhash_sealed`` provide a per-``global_State`` storage of interned strings (unsealed and sealed, respectively). During a GC cycle, strings are marked like all other objects. This happens actually while other objects (like coroutine stacks or tables) are traversed during propagation phase (strings per se do not hold references to other objects and thus do not propagate their marks). However, since strings are stored separately from the main list of collectible objects, there is a dedicated ``sweepstring`` state for freeing all unused strings.

Auxiliary Lists
^^^^^^^^^^^^^^^

Data structures described above grow when new objects are created (with respective dynamic memory allocation) and shrink when no longer used objects are swept from the platform. There are, however, other lists populated and emptied by GC itself for traversing some subset of the entire set of objects, for example:

    -  List anchored at ``GCState.gray``, for traversing
       objects marked gray.
    -  List anchored at ``GCState.grayagain``, for
       re-traversing objects that have already been marked
       black, but received a reference to a white object
       in-between GC phases during a cycle.
    -  List anchored at ``GCState.gcweak`` for traversing weak
       tables (see the `Reference
       Manual <https://www.lua.org/manual/5.1/>`__ for more info about weak table).

GC types intrinsics (valid for |PROJECT| 0.17)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To track garbage collectible objects the following entities
are used at per-object level:

.. _gcheader_gc_gotchas:

-  ``GCHeader``

    .. code-block:: c

        #define GCHeader   GCobj *nextgc; uint8_t marked; uint8_t gct

    which is put at the beginning of any garbage collectible type struct, for instance,

    .. code-block:: c

        typedef struct GCproto {
            GCHeader;
            ...
        };

    where

    ================= ========================================================================================================================================================
    ``GCobj *nextgc`` Next garbage collectible object. Used for quick traversing of objects. Connected objects are not necessarily dependent in terms of Lua chunk data objects (e.g. value field of a table and the table) but are coupled due to the GC manages memory. In other words, this is the GC who has connected these objects at their creation i.e. memory allocation stage. 
    ``uin8_t marked`` Used for objects marking for the purposes of the GC. For details, see ``uj_obj_marks.h``.
    ``uint8_t gc``    Type of garbage collectible object.
    ================= ========================================================================================================================================================

-  ``GCobj *gclist``

    Field which is present in any ``GCobj`` type which can have dependent ``GCobj`` objects (thus, it's present for ``GCtab`` but not for ``GCstring``). In this case, it's put in special lists in the course of GC traversal.

    This field serves as a pointer to the next object in such a list (see ``lj_gc_push`` and code utilizing it). The field (if present) should obey the following:

    .. code-block:: c

        /* The gclist field MUST be at the same offset for all GC objects. */
        LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(lua_State, gclist));
        LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCproto, gclist));
        LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCfuncL, gclist));
        LJ_STATIC_ASSERT(offsetof(GChead, gclist) == offsetof(GCtab, gclist));

It may seem that ``nextgc`` and ``gclist`` serve quite the same purpose but is the difference:

    -  ``nextgc`` is used for building the global VM-level chain
       of objects allocated within the VM.
    -  ``gclist`` is used for keeping objects of certain types
       in sub-chain of that global object chain for additional
       processing.
