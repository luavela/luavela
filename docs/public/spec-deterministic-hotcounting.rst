.. _spec-deterministic-hotcounting:

Deterministic Hotcounting
=========================

.. contents:: :local:

Introduction
------------

``HOTCNT`` will use down-counting mechanism.

By default |PROJECT| decreases counter by 2 for loops and by 1 for functions. New implementation will decrease 1 for loops and functions, but multiply source counter by 2 for functions.

If compiler blacklists or compiles bytecode, ``HOTCNT`` will continue to count calls, but it doesn't affect anything.

``HOTCNT`` instruction:

- 8 bits - opcode.
- 8 bits - internal flags.
- 16 bits - counter.

Affected bytecodes
------------------

-  **Loops**

   -  ``FORL``

      Lua code example:

      .. code-block:: lua 

               local a = 0

               for i = 1, 10 do
                   a = a + 1
               end

               print(a)

      .. code-block:: lua 

               -- BYTECODE -- /home/user/tmp.lua:0-8
               0001    KSHORT   0   0
               0002    KSHORT   1   1
               0003    KSHORT   2  10
               0004    KSHORT   3   1
               0005    FORI     1 => 0009
               0006    KSHORT   5   1
               0007    ADD      0   0   5
               0008    FORL     1 => 0006
               0009    GGET     1   0      ; "print"
               0010    MOV      2   0
               0011    CALL     1   1   2
               0012    RET0     0   1

               10

      .. code:: 

               -- BYTECODE -- /home/user/tmp.lua:0-8
               0001    KSHORT   0   0
               0002    KSHORT   1   1
               0003    KSHORT   2  10
               0004    KSHORT   3   1
               0005    FORI     1 => 0010
               0006    KSHORT   5   1
               0007    ADD      0   0   5
               0008    HCNT   
               0009    FORL     1 => 0006
               0010    GGET     1   0      ; "print"
               0011    MOV      2   0
               0012    CALL     1   1   2
               0013    RET0     0   1

               10

   -  ``LOOP``
   
      Emitted in 3 cases:

      -  ``goto``

         .. code-block:: lua 

                  local a = 0

                  ::lable1::
                  a = a + 1
                  if (a > 10) then
                      goto lable2
                  end

                  goto lable1

                  ::lable2::
                  print(a)

         .. code::

                  -- BYTECODE -- /home/user/tmp.lua:0-13
                  0001    KSHORT   0   0
                  0002    KSHORT   1   1
                  0003    ADD      0   0   1
                  0004    KSHORT   1  10
                  0005    ISGE     1   0
                  0006    JMP      1 => 0008
                  0007    JMP      1 => 0010
                  0008    LOOP     1 => 0008
                  0009    JMP      1 => 0002
                  0010    GGET     1   0      ; "print"
                  0011    MOV      2   0
                  0012    CALL     1   1   2
                  0013    RET0     0   1

                  11

         .. code::

                  -- BYTECODE -- /home/user/tmp.lua:0-13
                  0001    KSHORT   0   0
                  0002    KSHORT   1   1
                  0003    ADD      0   0   1
                  0004    KSHORT   1  10
                  0005    ISGE     1   0
                  0006    JMP      1 => 0008
                  0007    JMP      1 => 0011
                  0008    HCNT   
                  0009    LOOP     1 => 0009
                  0010    JMP      1 => 0002
                  0011    GGET     1   0      ; "print"
                  0012    MOV      2   0
                  0013    CALL     1   1   2
                  0014    RET0     0   1

                  11

      -  ``while``

         .. code-block:: lua 

                  local a = 0

                  while (a < 10) do
                      a = a + 1
                  end
                   
                  print(a)

         .. code:: 

                  -- BYTECODE -- /home/user/tmp.lua:0-8
                  0001    KSHORT   0   0
                  0002    KSHORT   1  10
                  0003    ISGE     0   1
                  0004    JMP      1 => 0009
                  0005    LOOP     1 => 0009
                  0006    KSHORT   1   1
                  0007    ADD      0   0   1
                  0008    JMP      1 => 0002
                  0009    GGET     1   0      ; "print"
                  0010    MOV      2   0
                  0011    CALL     1   1   2
                  0012    RET0     0   1

                  10

         .. code:: 

                  -- BYTECODE -- /home/user/tmp.lua:0-8
                  0001    KSHORT   0   0
                  0002    KSHORT   1  10
                  0003    ISGE     0   1
                  0004    JMP      1 => 0010
                  0005    HCNT   
                  0006    LOOP     1 => 0010
                  0007    KSHORT   1   1
                  0008    ADD      0   0   1
                  0009    JMP      1 => 0002
                  0010    GGET     1   0      ; "print"
                  0011    MOV      2   0
                  0012    CALL     1   1   2
                  0013    RET0     0   1

                  10

      -  ``repeat``

         .. code-block:: lua 

                  local a = 0

                  repeat
                      a = a + 1
                  until a > 10

                  print(a)

         .. code:: 

                  -- BYTECODE -- /home/user/tmp.lua:0-8
                  0001    KSHORT   0   0
                  0002    LOOP     1 => 0008
                  0003    KSHORT   1   1
                  0004    ADD      0   0   1
                  0005    KSHORT   1  10
                  0006    ISGE     1   0
                  0007    JMP      1 => 0002
                  0008    GGET     1   0      ; "print"
                  0009    MOV      2   0
                  0010    CALL     1   1   2
                  0011    RET0     0   1

                  11

         .. code:: 

                  -- BYTECODE -- /home/user/tmp.lua:0-8
                  0001    KSHORT   0   0
                  0002    HCNT   
                  0003    LOOP     1 => 0009
                  0004    KSHORT   1   1
                  0005    ADD      0   0   1
                  0006    KSHORT   1  10
                  0007    ISGE     1   0
                  0008    JMP      1 => 0002
                  0009    GGET     1   0      ; "print"
                  0010    MOV      2   0
                  0011    CALL     1   1   2
                  0012    RET0     0   1

                  11

   -  ``ITERL``

   Is emitted immediately after ``ITERC`` or ``ITERN`` instructions. Adding ``HOTCNT`` between ``ITERN`` and ``ITERL`` will produce a core dump. But it's possible to add ``HOTCNT`` before ``ITERN``/``ITERC``:

      .. code-block:: lua 

               local a = {5, 5, 5, 5, 5}
               local sum = 0
                 
               for k, v in ipairs(a) do
                   sum = sum + v
               end

               print(sum)

      .. code:: 

               -- BYTECODE -- /home/user/tmp.lua:0-9
               0001    TDUP     0   0
               0002    KSHORT   1   0
               0003    GGET     2   1      ; "ipairs"
               0004    MOV      3   0
               0005    CALL     2   4   2
               0006    JMP      5 => 0008
               0007    ADD      1   1   6
               0008    ITERC    5   3   3
               0009    ITERL    5 => 0007
               0010    GGET     2   2      ; "print"
               0011    MOV      3   1
               0012    CALL     2   1   2
               0013    RET0     0   1

               25

      .. code:: 

               -- BYTECODE -- /home/user/tmp.lua:0-9
               0001    TDUP     0   0
               0002    KSHORT   1   0
               0003    GGET     2   1      ; "ipairs"
               0004    MOV      3   0
               0005    CALL     2   4   2
               0006    JMP      5 => 0009
               0007    ADD      1   1   6
               0008    HCNT   
               0009    ITERC    5   3   3
               0010    ITERL    5 => 0007
               0011    GGET     2   2      ; "print"
               0012    MOV      3   1
               0013    CALL     2   1   2
               0014    RET0     0   1

               25

      .. note::

            Emitted immediately before ``ITERC`` or ``ITERN``
            instructions. Adding HOTCNT between ``ITERN`` and
            ITERL will produce a core dump. But it's possible
            to add ``HOTCNT`` before ``ITERN``/``ITERC``.

            ``HOTCNT`` will be executed after ``ITERL``.

-  **Function headers**

   -  ``FUNCF``

      .. code-block:: lua 

               function foo(a)
                   print(a)
               end

               foo(1)

      .. code::

               -- BYTECODE -- /home/user/tmp.lua:1-3
               0001    GGET     1   0      ; "print"
               0002    MOV      2   0
               0003    CALL     1   1   2
               0004    RET0     0   1

               -- BYTECODE -- /home/user/tmp.lua:0-6
               0001    FNEW     0   0      ; /home/user/tmp.lua:1
               0002    GSET     0   1      ; "foo"
               0003    GGET     0   1      ; "foo"
               0004    KSHORT   1   1
               0005    CALL     0   1   2
               0006    RET0     0   1

               1

      .. code:: 

               -- BYTECODE -- /home/user/tmp.lua:1-3
               0001    HCNT       
               0002    GGET     1   0      ; "print"
               0003    MOV      2   0
               0004    CALL     1   1   2
               0005    RET0     0   1

               -- BYTECODE -- /home/user/tmp.lua:0-6
               0001    FNEW     0   0      ; /home/user/tmp.lua:1
               0002    GSET     0   1      ; "foo"
               0003    GGET     0   1      ; "foo"
               0004    KSHORT   1   1
               0005    CALL     0   1   2
               0006    RET0     0   1

               1

.. note::

      Seems that is hard to emit ``HOTCNT`` before ``FUNCF`` since a lot of |PROJECT| mechanics depends on ``FUNCF`` is a first instruction (will be fixed in future).

      Default value of ``HOTCNT``'s counter after ``FUCNF`` needs to be decreased by 2, because ``HOTCNT`` executes after ``FUNCF`` and need one more iteration to start recording.

Affected interfaces
-------------------

-  **string.dump**
   Original implementation saves bytecode as is. So ``HOTCNT`` has different values each time. To avoid it let's write 0 to the ``HOTCNT``'s counter and flags in the dump without changing original instruction.

-  **string.load**
   Find ``HOTCNT`` and set flags and counter to some default values.

-  **jit.opt.start**
   We need a new mechanism to work with hotcounters after removing hotcount table. Let's iterate over all GC objects, find ``GCproto`` and patch it. 

Miscellaneous
-------------

-  **Late binding issue**

   Not affected.
-  **Comparison of the dumped and original bytecode**

   Seems that is not impossible to compare bytecodes:

   .. code-block:: lua 

            local function foo()
              local t = {}
              for i = 1,100 do t[i] = i end
              for a, b in ipairs(t) do end
              local m = 0
              while m < 100 do m = m + 1 end
            end

            local d1 = string.dump(foo)
            foo()
            assert(string.dump(foo) == d1) -- fail

-  **Hotcounting and hooks** 

   Since we added new bytecode (before ``FORL``), total number of executed instructions differs from the reference value.

   .. code-block:: lua 

            local a = 0
            debug.sethook(function (e) a = a + 1 end, "", 1)
            a = 0; for i = 1,1000 do end; assert(1000 < a and a < 1012)
            debug.sethook(function (e) a = a + 1 end, "", 4)
            a = 0; for i = 1,1000 do end; assert(250 < a and a < 255)

   ``HOTCNT`` is ignored by instruction hooks.

-  **First line**

   ``HOTCNT`` as a part of prologue.

   Status: fixed?