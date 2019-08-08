.. _tut-for-iter:

Tutorial: for-iterators
=======================

.. contents:: :local:

Generic for-iterator
--------------------

Consider the following piece of lua code:

.. code-block:: lua 

  -- test.lua
  local tbl = {'a', 'b'}
  for k,v in ipairs(tbl) do
      print(v)
  end

This code is translated in the following bytecode by the |PROJECT| frontend:

.. code::

    $ ./luajit -bl ./test.lua
    -- BYTECODE -- test.lua:0-6
    0001    TDUP     0   0
    0002    GGET     1   1      ; "ipairs"
    0003    MOV      2   0
    0004    CALL     1   4   2
    0005    JMP      4 => 0009
    0006    GGET     6   2      ; "print"
    0007    MOV      7   5
    0008    CALL     6   1   2
    0009    ITERC    4   3   3
    0010    ITERL    4 => 0006
    0011    RET0     0   1

Instruction 0001 fetches table constant (described by ``'tbl'`` in the original code) and places it into stack slot 0 (referred to as #0). Instructions 0002 through 0004 perform the call to ``ipairs`` described by ``'ipairs(tbl)'`` in the original code. After this call, #1 contains ``ipairs`` iterator function, #2 contains ``'tbl'`` (it was moved there by 0003 and did not change since then) and #3 contains numeric zero. This behaviour of ``ipairs`` is described in the `Lua reference
manual <http://www.lua.org/manual/5.1/manual.html#pdf-ipairs>`_.

The return values of ``ipairs`` is everything that is need to iterate through the array part of the table: it contains the table itself, the induction variable being the array index of the current element (already defaulted to zero) and the function that is able to deduce the next element.

Once the loop context is prepared, 0005 jumps to the end of iteration instruction 0009 to trigger the first iteration.

Iterator function is designed to perform the induction by returning either ``nil`` (which designates end of the loop), or new value of the induction variable and corresponding array value (these are actually the ``'k,v'`` pair in the initial Lua code). Two output values are stored in #4 and #5 (``ipairs_aux`` iterator is merely a standard Lua function and ``ITERC`` sets its base to (4+1) in order to get the return values right behind the loop context). After that, ``ITERL`` checks that #4 is not ``nil``, copies it to the induction variable in loop context and jumps to the iteration body. Within the iteration body, lua might freely operate with ``k`` and ``v`` as stored in #4 and #5 by the iterator function implicitly called by ``ITERC``.

As a result, the following stack layout is maintained during the loop:

  -  #1, #2 and #3 contains the loop context. #3 is updated by
     ``ITERL`` before the new iteration starts.
  -  #4 and #5 contains the values of ``k`` and ``v`` as
     returned by the iterator function (called by ``ITERC``)
     and as seen by the loop body.
  -  #6+ are at the disposal of the loop body (local
     variables, parameters and return values of the callee
     functions etc.).

.. note:: 

    In order to get better understanding of for-iterators, let's write a code that uses custom for-iterator to only query elements from the array until some particular element is encountered:

.. code-block:: lua 

    -- test-hates-seven.lua 
    function ipairs_hates_seven_aux(table, index)
    new_index = index + 1
    if (new_index > #table or table[new_index] == 7) then
        return nil
    else
        return new_index, table[new_index]
    end
        end
    function ipairs_hates_seven(table)
        return ipairs_hates_seven_aux, table, 0
    end
    local tbl = {1, 2, 3, 4, 7, 12, 13}
    for index,value in ipairs_hates_seven(tbl) do
        print('tbl[' .. index .. '] = ' .. value)
    end

.. code::

    $ ./luajit ./test-hates-seven.lua
    tbl[1] = 1
    tbl[2] = 2
    tbl[3] = 3
    tbl[4] = 4

There are no restrictions on how the induction variable is incremented:

.. code-block:: lua 

  -- test-even-index.lua
  function ipairs_even_index_aux(table, index)
   new_index = index + 2
   if (new_index > #table) then
     return nil
   else
     return new_index, table[new_index]
   end
  end
  function ipairs_even_index(table)
   return ipairs_even_index_aux, table, 0
  end
  local tbl = {1, 2, 3, 4, 7, 12, 13}
  for index,value in ipairs_even_index(tbl) do
   print('tbl[' .. index .. '] = ' .. value)
  end

.. code::

  $ ./luajit ./test-even-index.lua
  tbl[2] = 2
  tbl[4] = 4
  tbl[6] = 12

                   
In fact, there are no restrictions on induction variable at all, except that the iterator-generating function must properly setup the initial value of the induction variable, the iterator must know how to use it and that the ``nil`` value of the induction variable designates the end of the iteration.

There are no restrictions on the loop state (table object in ``pairs`` and ``ipairs``), including its type.

There are no restrictions on the iterator, except it must be callable either directly or via metamethods.

Pairs for-iterator
-------------------

Language perspective
^^^^^^^^^^^^^^^^^^^^

Lua reference manual starts to get really nasty when it comes to ``pairs.`` It claims that the iterator function for ``pairs`` is `next <http://www.lua.org/manual/5.1/manual.html#pdf-next>`_ and introduces ambiguous "index" parameter of ``next`` and decides that that's enough.

In fact, that's how ``next`` works:

  1. If the second argument is nil, return the "first"
     key-value pair of the table. The definition of "first" is
     implementation- and situation- dependent and will be
     discussed later.
  2. If the second argument is not nil, it is treated as a key
     in the table. In this case, ``next`` searches for this
     key in the table and returns "next" key-value pair.
     Again, "next" is implementation- and situation- dependent
     in the same way.
  3. If current key-value pair (defined by the second argument
     as a key) is the "last", return ``nil``.

As long as there is a common mechanism that allows to convert the table contents to the pseudo-array of key-value pairs, "first", "next" and "last" key-value pairs of the table in terms of next are first, next and last key-value pairs of this array.

With this being said, ``pairs(tbl)`` might be written in Lua `as follows <http://www.lua.org/pil/7.3.html>`__:

.. code-block:: lua 

  function pairs(table)
      return next, table, nil 
      -- here 'next' is the reference to the library function
  end

Also, one might change the semantics of ``pairs`` by placing custom iterator function on top of ``next`` (or ``pairs``). However, changing the semantics of ``next`` from within the loop will not make a difference, since the reference to ``next`` is resolved within iterator-generating ``pairs`` only once per loop.