.. _test-benchmark:

Case Study: Writing Benchmarks for |PROJECT|
============================================

Introduction
------------

Suppose that you want to micro-benchmark a certain part of the platform, say you want to know how much ``tonumber`` costs you. You know that |PROJECT| can be downloaded and used as a stand-alone application through Obtaining |PROJECT| and you have prepared a simple benchmark, like this:

.. code-block:: lua

    -- file1.lua:
    local x
    for i = 1, 100000000 do
          x = tonumber("320")
    end

    -- file2.lua:
    local x
    for i = 1, 100000000 do
          x = 320
    end

Then you run it like this:

.. code::

    $ /usr/bin/time -f%U ujit file1.lua
    ...
    $ /usr/bin/time -f%U ujit file2.lua
    ...

Will the results be representative? Definitely not. Interested why? Then read on!

Analyzing the Benchmark
-----------------------

First, please note that the body of the loop in any original benchmark contains only loop invariants, i.e. expressions that do not depend on the loop variable. It means that the first benchmark can be rewritten as follows, with the body loop *hoisted* out of the loop:

.. code-block:: lua

    local x = tonumber("320")
    for i = 1, 100000000 do
    end

Fortunately, the JIT compiler is smart enough to perform this hoisting, too. Unfortunately, for this particular case, ``ujit`` stand-alone application starts with JIT compile on. This means effectively that the two original benchmarks compare the speed of executing the same empty loop, which is definitely not what you wanted. If you dump the progress of the JIT compiler, you will see something like this:

.. code::

    $ ./ujit -p- -e 'local x; for i = 1, 100000000 do x = tonumber("320") end'
    ---- TRACE 1 start =(command line):1
    0006    GGET     5   0      ; "tonumber"
    0007    KSTR     6   1      ; "320"
    0008    CALL     5   2   2
    0000    . FUNCC               ; tonumber
    0009    MOV      0   5
    0011    FORL     1 => 0006
    ---- TRACE 1 IR
    ....              SNAP   #0   [ ---- ]
    0001 rbp      int SLOAD  #2    CI
    0002 r9       fun SLOAD  #0    R
    0003 rsi      tab FLOAD  0002  func.env
    0004 r8       int FLOAD  0003  tab.hmask
    0005       >  int EQ     0004  +63
    0006 rbx      p32 FLOAD  0003  tab.node
    0007 rdx   >  p32 HREFK  0006  "tonumber" @13
    0008 rax   >  fun HLOAD  0007
    0009       >  fun EQ     0008  tonumber
    0010 rbp    + int ADD    0001  +1
    ....              SNAP   #1   [ ---- 320 ]
    0011       >  int LE     0010  +100000000
    ....              SNAP   #2   [ ---- 320 0010 ---- ---- 0010 ]
    0012 ------------ LOOP ------------
    0013 rbp    + int ADD    0010  +1
    ....              SNAP   #3   [ ---- 320 ]
    0014       >  int LE     0013  +100000000
    0015 rbp      int PHI    0010  0013
    ---- TRACE 1 mcode 155
    0bd6ff65  mov r11, 0x7fc692377620
    0bd6ff6f  mov dword [r11], 0x1
    0bd6ff76  mov rcx, 0x7fc69237be98
    0bd6ff80  cvtsd2si ebp, qword [r10+0x10]
    0bd6ff86  mov r9, [r10-0x10]
    0bd6ff8a  mov rsi, [r9+0x10]
    0bd6ff8e  mov r8d, [rsi+0x38]
    0bd6ff92  cmp r8d, 0x3f
    0bd6ff96  jnz 0xbd60010             ->0
    0bd6ff9c  mov rbx, [rsi+0x28]
    0bd6ffa0  mov rdi, 0x7fc69237bed0
    0bd6ffaa  cmp rdi, [rbx+0x218]
    0bd6ffb1  jnz 0xbd60010             ->0
    0bd6ffb7  cmp dword [rbx+0x220], 0xfffffffb
    0bd6ffbe  jnz 0xbd60010             ->0
    0bd6ffc4  lea rdx, [rbx+0x208]
    0bd6ffcb  cmp dword [rdx+0x8], 0xfffffff7
    0bd6ffcf  jnz 0xbd60010             ->0
    0bd6ffd5  mov rax, [rdx]
    0bd6ffd8  cmp rax, rcx
    0bd6ffdb  jnz 0xbd60010             ->0
    0bd6ffe1  add ebp, 0x1
    0bd6ffe4  cmp ebp, 0x5f5e100
    0bd6ffea  jg 0xbd60014              ->1
    -> LOOP:
    0bd6fff0  add ebp, 0x1
    0bd6fff3  cmp ebp, 0x5f5e100
    0bd6fff9  jle 0xbd6fff0             ->LOOP
    0bd6fffb  jmp 0xbd6001c             ->3
    ---- TRACE 1 stop -> loop

Without diving into the details, please pay attention to the ``------------ LOOP ------------`` line: Everything above it is, roughly speaking, a loop invariant code. As you can see, everything that resembles a ``tonumber`` call is exactly above that line, while only some few pieces of ehm...  something (IR instructions, but this really does not matter now) is below the line. Luckily, there is no need to examine dumps and dive into gory details of the compiler each time you benchmark something, just remember this:

.. note::

   When benchmarking any code by wrapping the code into a loop, do one of the following:

      1. Switch the compiler off;
      2. Ensure that the benchmarked code is **not** loop-invariant, i.e. it must depend on the looping variable

Benchmarking with Compiler Off
------------------------------

Running ``ujit`` with compiler off lets you avoid any invisible side effects on your code, in this case the interpreter literally executes what you have written. Simply start ``ujit`` with ``-joff``, or switch the compiler off directly in the Lua chunk with ``jit.off()``.

With this technique, you gain following:

    - You can estimate, how your hypotheses perform relatively to each other.
    - You can estimate, how your code will perform if this particular part of the code fails to JIT-compile (say, in case it unluckily becomes a part of the trace that contains a non-JITtable thing like a call to C API).

With this technique, you obviously lose following:

    - You'll never know what performance you can achieve if the benchmarked code JIT compiles.

Now let' see how we can fix the benchmark:

.. code-block:: lua

    assert(jit.status() == false, "This benchmark is designed to run without JIT, please either -joff in command line, or jit.off() in Lua")

    local N = 1e8

    local function assign1()
          print("With tonumber")
          local n
          for i = 1, 100000000 do
             n = tonumber("320")
          end
          return n
    end

    local function assign2()
          print("Without tonumber")
          local n
          for i = 1, N do
             n = 320
          end
          return n
    end

    local function benchmark(name, t1, t2)
          if name == "tonumber" then
             assign1(t1)
          else
             assign2(t2)
          end
    end

    benchmark(arg[1], t1, t2)

And run it:

.. code::

    $ /usr/bin/time -f%U ./ujit -joff tonumber-no-jit.lua
    ...
    $ /usr/bin/time -f%U ./ujit -joff tonumber-no-jit.lua tonumber
    ...

Benchmarking with Compiler On
-----------------------------

As demonstrated above, running the benchmark with compiler on immediately exposes your code to various transformations done by the compiler. On other hand, this is much more fun!  Just be careful:

    - No loop-invariants, remember?
    - You may want to read :ref:`Reduce test cases <test-reduce-cases>` to gain more inspiration and knowledge about interacting with the compiler.
    - You may ultimately want to learn the dump format, at least to estimate that you benchmark what you intended to. This one may be really tricky, |PROJECT| team understands that it is too cruel to MAKE you do so and always welcomes you to ask any questions of you feel you are stuck with benchmarking with the compiler turned on.

And despite these pitfalls, you definitely gain following:

    -  You can estimate, how your hypotheses perform relatively to each other with JIT compilation on.
    -  You can estimate, how your code will perform if this particular part of the code is lucky to JIT-compile.

So let's see one possible variant of the benchmark.

First, let's generate some data:

.. code::

    $ ujit -e 'print("local t1 = {");
    for i = 100, 999 do print("\t\"" .. i .. "\",") end;
    print "}";
    print "local t2 = {";
    for i = 100, 999 do print("\t" .. i .. ",") end;
    print "}";
    print("return {t1 = t1, t2 = t2}")
    ' >data.lua

And the benchmark itself:

.. code-block:: lua

    assert(jit.status() == true, "This benchmark is designed to run with JIT, please either -jon in command line, or jit.on() in Lua")

    local N = 1e8
    local data = require("data")
    assert(type(data) == "table")
    assert(type(data.t1) == "table")
    assert(type(data.t2) == "table")

    local function assign1(t)
          print("With tonumber")
          local nels = #t
          local n
          for i = 1, N do
             n = tonumber(t[i % nels + 1])
          end
          return n
    end

    local function assign2(t)
          print("Without tonumber")
          local nels = #t
          local n
          for i = 1, N do
             n = t[i % nels + 1]
          end
          return n
    end

    local function benchmark(name, t1, t2)
          if name == "tonumber" then
             assign1(t1)
          else
             assign2(t2)
          end
    end

    benchmark(arg[1], data.t1, data.t2)

And run it:

.. code::

            $ /usr/bin/time -f%U ./ujit -jon tonumber-jit.lua
            ...
            $ /usr/bin/time -f%U ./ujit -jon tonumber-jit.lua tonumber
            ...

Conclusion
----------

- Be careful when benchmarking with the compiler turned on: In particular, do not allow loop hoisting to spoil you benchmark and make it totally non-representative.
-  Do two-fold benchmarking:

   - To evaluate worst-case performance, benchmark with JIT compiler off;
   - To evaluate best-case performance, benchmark with JIT compiler on.

- Do not hesitate to contact |PROJECT| team if you get puzzled by any result you observe.
