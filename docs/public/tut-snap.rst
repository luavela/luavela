.. _tut-snap:

Tutorial: Understanding SNAP
============================

This tutorial is heavily based on this thread in the `LuaJIT mailing list <https://www.freelists.org/post/luajit/Understanding-SNAP>`_.

The Basics of Reading a Snapshot
--------------------------------

.. code::

    ujit -p- -e 'local x = 1.2 for i=1, 1e3 do x = x * -3 end'

    ---- TRACE 1 start =(command line):1
    0006 KSHORT 5 -3
    0007 MUL 0 0 5
    0009 FORL 1 => 0006
    ---- TRACE 1 IR
    .... SNAP #0 [ ---- ]
    0001 rbp int SLOAD #2 CI
    0002 xmm7 > flt SLOAD #1 T
    0003 xmm7 + flt MUL 0002 -3
    0004 rbp + int ADD 0001 +1
    .... SNAP #1 [ ---- 0003 ]
    0005 > int LE 0004 +1000
    .... SNAP #2 [ ---- 0003 0004 ---- ---- 0004 ]
    0006 ------------ LOOP ------------
    0007 xmm7 + flt MUL 0003 -3
    0008 rbp + int ADD 0004 +1
    .... SNAP #3 [ ---- 0007 ]
    0009 > int LE 0008 +1000
    0010 rbp int PHI 0004 0008
    0011 xmm7 flt PHI 0003 0007

Entries in a snapshot are local variables (*) whose values have changed since the start of the trace. I'm using "local variables" from the point of view of the interpreter, which includes:

    - Call frame metadata for function calls inlined into the trace, or for tail calls performed on trace
    - Function arguments
    - Actual local variables declared with "local" which are still in scope
    - Hidden for-loop control variables
    - Temporary stack slots required by the current expression/statement

How can one track snapshots to original stack slots? In first snapshot second position is written by IR at 0003. Going by the argument of IR at 0003, 0002 is x. For simplicity, start counting snapshot entries from -1, at which point non-negative indices correspond exactly to stack slots of the function which initiated the trace. "x" in this case is local variable number #0 - you can see this from e.g. cross-referencing ``x = x * -3`` with ``0007 MUL 0 0 5`` - the first 0 is the destination slot (x), and the second 0 is the source slot (also x).

Following this logic, what are 3rd and 6th position in the SNAP #2?

Please note, there is no IR operation which can store to a local variable - such stores are elided, and get expressed as a snapshot immediately preceding the next operation which can fail. If there were explicit stores in the IR, then between 0004 and SNAP #2, you'd see two SSTOREs which stored the result of 0004 (these two stores corresponding to the internal and external copies of the for-loop control variable, which naturally happen after 0004 as 0004 is incrementing the loop control variable). As it happens, SNAP #2 is somewhat unusual, in that it precedes a loop marker rather than preceding a failable instruction, and so it is never taken by any exit. Regardless, it describes the interpreter state at the start of the loop body (or at the end of the loop body, depending on your view of where the loop header occurs):

.. code::

    [-1] (call frame metadata) unchanged
    [0] (x) result of 0003
    [1] (internal copy of i) result of 0004
    [2] (for loop end index) unchanged
    [3] (for loop iteration increment) unchanged
    [4] (external copy of i) result of 0004

SNAP #3 has fewer entries than SNAP #2, as it exits to after the loop has finished (you can't actually tell where a snapshot exits to based on ``-p`` output - I do wish such information was included in the output.), and and therefore all the for-loop variables have dropped out of scope. SNAP #1 has fewer entries for the same reason: it exits to after the loop has finished, rather than exiting into the body of the loop.

Emphasis: SNAP #1 and SNAP #3 both exit to the same place in the bytecode (as a general rule, each snapshot after "``--- LOOP ---``" is a copy of one of snapshots before "---LOOP ---", and will exit to the same place). You can infer where they exit to by looking at the instruction which follows them, which is ``LE``. The snapshot is taken if the instruction fails, and if this particular ``LE`` fails, it means that the loop condition is false, i.e. execution continues from immediately after the loop.

If the exit becomes warm and causes a side trace, then yes, the start location of the side trace tells you the exit location of the snapshot.

Summary
^^^^^^^

Start counting from -1, and non-negative indices correspond exactly to stack slots of the function which initiated the trace. Starting at zero, function arguments occupy one slot each (in their declared order), and then in-scope local variables occupy one slot each (in their declared order).

These are interleaved with for-loop control variables, if any. Each for-loop has three hidden variables - numeric for-loops have the internal copy of the loop variable, the end index, and the increment, whereas generic for-loops have a function, the state, and the internal copy of the first loop variable. Then temporary stack slots required by the current expression / statement occupy some number of slots, then if you're in an inlined call, call frame metadata takes one slot, followed by arguments/locals/etc. of the inlined call.

Reading a Snapshot: Advanced Example
------------------------------------

.. code::

    [ app.lua:379|---- ---- 0001 0002 0003 0004 0005 0006 0007 0008 app.lua:471|0009 0004 0002 0008 0007 0015 heap.lua:145|0010 +1 0016 0017 0011 +4 ]

Each ``filename:num|`` separation represents one stack frame from the function at that ``filename:num`` called previously. I.e. frames are marked with vertical bars. The ``filename:num`` isn't part of the marker though; it denotes a value to be restored to the stack if the snapshot exit is taken (namely, it restores what I've been referring to as a call frame metadata slot).

Besides, "+1" and "+4" denote the integer 1 and the integer 4, respectively. For example, "+1" means that there would have been an SSTORE which stored the constant integer 1 to a particular Lua stack slot; continuing the theme that every entry in a SNAP represents an SSTORE, sometimes the thing being stored is the result of an IR instruction, and sometimes the thing being stored is a constant (``filename:num`` counts as a kind of constant).

SLOAD Gotchas
-------------

What is SLOAD? Why no SSTORE?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

An SLOAD conceptually represents loading a value from the Lua stack. In root traces, it always is loading a value from the Lua stack. In side traces, it can end up meaning "load" the value which would have been in the Lua stack, had the parent trace actually actually done the SSTOREs which were implied by the snapshot leading to the side trace" (again, SSTOREs aren't a thing, and don't happen, but you can think of SNAPs as indicating which SSTOREs would have happened, if they were a thing).

Stores to the Lua stack do not happen in between the parent trace and a side trace - instead |PROJECT| combines the hypothetical SSTORE and the actual SLOAD into a single register-to-register move. That said, if values are in C stack spill slots rather than CPU registers, an additional load and/or store can be required on either side of the move. At a slightly higher level, you can consider the register / C stack layout at the exit point in the parent trace, and the register / C stack layout at the entry point in the side trace, and |PROJECT|'s job is to shuffle things around in order to reach the layout desired by the side trace, starting from the layout in the parent trace. The current shuffling algorithm is overly simplistic, and the corresponding NYI aborts indicate that a more complex algorithm is required.

Number of SLOADs and Snapshot Size
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Consider the original example:

.. code::

    ujit -p- -e 'local x = 1.2 for i=1, 1e3 do x = x * -3 end'

    ---- TRACE 1 start =(command line):1
    0006 KSHORT 5 -3
    0007 MUL 0 0 5
    0009 FORL 1 => 0006
    ---- TRACE 1 IR
    .... SNAP #0 [ ---- ]
    0001 rbp int SLOAD #2 CI
    0002 xmm7 > flt SLOAD #1 T
    0003 xmm7 + flt MUL 0002 -3
    0004 rbp + int ADD 0001 +1
    .... SNAP #1 [ ---- 0003 ]
    0005 > int LE 0004 +1000
    .... SNAP #2 [ ---- 0003 0004 ---- ---- 0004 ]
    0006 ------------ LOOP ------------
    0007 xmm7 + flt MUL 0003 -3
    0008 rbp + int ADD 0004 +1
    .... SNAP #3 [ ---- 0007 ]
    0009 > int LE 0008 +1000
    0010 rbp int PHI 0004 0008
    0011 xmm7 flt PHI 0003 0007

SNAP #0 has just one entry, while there are IRs SLOAD #2 and SLOAD #1. Why? A SNAP indicates the SSTOREs which would have occurred preceding the SNAP, if there were SSTOREs in the IR. As #0 is at the start of the IR, nothing precedes it, so it is empty. An SLOAD is not an SSTORE, so it does not really interact with snapshots. An SLOAD indicates a load from the stack rather than a load from any preceding snapshot, so the number of entries in a preceding snapshot bears no resemblance to the stack slot index in an SLOAD. In fact, you should always see that the index referred-to in an SLOAD is either "----" or not present in the most recent preceding snapshot - if there was anything else, then there would already be an IR instruction giving the value of that stack slot, and so an SLOAD would not be required.

Linking Traces With Snapshots
-----------------------------

Let's use the following example:

.. code::

    ---- TRACE 1 IR
    .... SNAP #0 [ ---- ---- ]
    0001 rbp int SLOAD #8 CI
    0002 xmm7 num CONV 0001 num.int
    0003 xmm3 + num ADD 0002 +1.5
    0004 xmm4 + num ADD 0002 +2.5
    0005 xmm5 + num ADD 0002 +3.5
    0006 xmm6 + num ADD 0002 +4.5
    0007 xmm7 + num ADD 0002 +5.5
    .... SNAP #1 [ ---- ---- ---- 0003 0004 0005 0006 0007 0001 ---- ---- ---- ]
    ...

    ---- TRACE 2 start 1/1 code:4
    0014 UNM 1 1
    0015 JFORL 6 1
    ---- TRACE 2 IR
    0001 xmm7 num SLOAD #3 PI
    0002 xmm4 num SLOAD #4 PI
    0003 xmm5 num SLOAD #5 PI
    0004 xmm6 num SLOAD #6 PI
    0005 xmm0 num SLOAD #7 PI
    0006 rbp int SLOAD #8 PI
    ...

Based on these ``SLOAD`` s, the start of trace 2 wants xmm7 to contain the result of SLOAD #3, which based on trace 1 snap 1 (noting that trace 2 starts at trace 1 exit 1) is trace 1's instruction 0003, which is in xmm3 at the time of trace 1 snap 1, hence |PROJECT| needs to do "mov xmm7, xmm3" at the start of trace 2. The start of trace 2 also wants xmm0 to contain #7, which is 0007 based on the snap, which is in xmm7 (that these numbers line up is purely a coincidence of the example at hand) at the time of trace 1 snap 1. Hence |PROJECT| also needs to do "mov xmm0, xmm7" at the start of trace 2.

Crucially, it needs to do "mov xmm0, xmm7" before it does "mov xmm7, xmm3" (as otherwise it would overwrite xmm7 before it had pulled the value out). As it happens, trace 2 wants SLOAD 4 though 6 in xmm4 through xmm6, and these end up being 0004 through 0006, which trace 1 already had in xmm4 through xmm6, so no shuffling was necessary for them (this is not pure chance). In general, there will be some collection of register-to-register moves, C-stack-to-C-stack moves, and register/C-stack moves which need to happen, and various constraints on which moves have to happen before which other moves. |PROJECT|'s current algorithm for scheduling all of these moves is quite simple, and cannot handle various cases.

More About Current Coalescing Algorithm
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Consider the case where a particular value is in say the register "rax" in the parent trace (at the point of the snapshot), and the side trace wants that value to be in say the register "rcx". This is an easy case to handle: |PROJECT| just needs to emit "mov rcx, rax" (though there is slightly more complexity: it also has to ensure that nobody needed the old value of "rcx" before doing so). Now consider a different case: a particular value is in memory location "[rsp + 0x40]" in the parent trace, whereas the side trace wants it to be in "[rsp + 0x24]". This is rather more awkward, for a few reasons:

    -  "rsp" need not have the same value in the parent and child traces.
    -  There is no CPU instruction to move a value from one memory address to another, so the value has to go via a register, i.e. load from [rsp + 0x40] to temp, then store from temp to [rsp + 0x24]. Pedants will point out instructions like push and pop and movs, which can each reference two memory locations in a single instruction, but they are not easily applicable.)
    -  There are potentially more stack locations to keep track of than there are registers to keep track of (~256 versus ~32 on x64).
    -  What was an adjacent pair of independent 4-byte spill slots in the parent trace might become a single 8-byte spill slot in the side trace (or vice versa).

Rather than try to tackle this complexity, the current algorithm always turns a stack-to-stack move into a stack-to-register move plus a register-to-stack move, and it performs all stack-to-register moves before performing any register-to-stack moves. If you consider the machine code which |PROJECT| emits in this scenario, it'll be along the lines of:

1. Do all the simple register-to-register moves.
2. pass3: Do all stack-to-register moves.
3. Adjust rsp, update traceno, and other bookkeeping.
4. pass2: Do register-to-stack moves.
5. Do register-to-stack copies.

NB: As |PROJECT| generates machine code backwards, this order is the inverse order of what you see if you read ``asm_head_side`` from top to bottom.

Because all stack-to-register moves happen before any register-to-stack moves, there is a point in time where all values exist in registers (in my list above, this is the point in time numbered "3.", whereas in the comment  you reference, this point in time is called "between pass 2 and pass 3"). As an x64 CPU only (conceptually) has 16 general-purpose registers and 16 floating-point registers, the limitation that all values need to simultaneously exist in a register at this point in time means that a side trace can inherit no more than 16 floating-point values from a parent trace, and no more than 15 non-floating-point values from a parent trace. You're seeing an error thrown in pass2 because pass2 is the point where registers get allocated for keeping a stack spill slot, and all the registers have already been given out.

If you're feeling adventurous, you could try play with |PROJECT|'s coalescing algorithm so as to not require all inherited values to exist in a register simultaneously. Peter Cawley has taken a small stab at this in https://github.com/corsix/LuaJIT/commits/nyicoal, which may or may not help your situation (and may or may not introduce strange bugs to your program - use at your own risk).

Disadvantages of the Coalescing Algorithm
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can ask: Why don't side traces inherit the same register/stack assignments from the parent trace? Couldn't the side trace accept that SLOAD#3 is in xmm3 instead of shuffling it over to xmm7? (Wouldn't this solve the problem of aborting traces due to excessively complex register/stack shuffling?)

In short, the problem is that |PROJECT| assembles a trace in reverse order. If it assembled in the "conventional order, then it could start from the register/stack assignments as of the parent exit, and work forwards.

Instead, it assembles in reverse order, and has to hope that all of the choices it makes along the way result in a final state which matches the parent exit. For register assignments, it does actually make a small effort to make things line up (namely, when it chooses a register for  something, it considers the assignment in the parent to be a hint). Stack assignments are much harder to try and line up: if the child requires more stack space than the parent, then "[rsp+X]" doesn't mean the same thing in the parent and the child, but the difference in rsp between parent and child isn't known until assembly of the child trace has finished, and references to "rsp+X" need to be emitted **during** assembly of the child trace.
