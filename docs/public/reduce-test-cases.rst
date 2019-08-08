.. _reduce-test-cases:

Reducing Test Cases for Bugs
=============================

.. contents:: :local:

.. note::

    This is a copy of `LuaJIT doc <http://wiki.luajit.org/Reducing-Testcases>`_, with some |PROJECT|-specific amendments

So you think you've found a bug in |PROJECT|?

Run your code with PUC ``lua``, and with ``ujit -joff`` and check that you don't see the bug, and that you do with ``ujit``.

Check that you're not relying on any undefined behaviour. I once thought I had found a bug, but in fact I was relying on the order in which ``pairs `` traversed a hash table. Both ``lua`` and ``ujit -joff`` always traversed the table in exactly the same order, but sometimes ``ujit`` legitimately traversed the table in a different order.

If you still think it might be |PROJECT|'s fault, read on...

These things are hard to catch
------------------------------

JIT bugs can be hard to catch. The JIT compiler uses a number of heuristics to decide which traces to compile, and your bug showing up will probably depend on a particular trace starting at the right line being chosen for compilation at the right time.

Almost anything can tickle the trace-choosing heuristics and make the bug disappear, so the process of reducing your code to a smaller test case can be slow. Even removing unused code can move or hide the problem - but sometimes code you couldn't remove at one stage can be removed later after other changes.

Persistence does win in the end.

Don't print
-----------

If you're like me, you'll be tempted to sprinkle ``print`` s in your code to see if you can find out what's going on. ``print`` isn't implemented as a fast function, so traces containing ``print`` are aborted. So if you put a ``print`` in the middle of the problem trace, you'll stop the trace being compiled and hide your problem.

If you want to get some output use ``io.write`` which doesn't abort traces, but beware - it does tickle the heuristics.

Dump it
-------

Try to get a dump of your code: ``ujit -p <dump_file_name> <file.lua>``. The smaller the dumpfile, the easier it'll be to finally track down the problem, use the size of the file as a metric of how well you're doing at reducing your case.

Try all the options
-------------------

Different compiler options to LuaJIT can move, hide or expose the problem, so it's worth trying a few of them to ferret it out. I've found that there's usually a value of ``-Ohotloop=X`` that'll set the bug off so it's worth trying a few.

Here's a shell script that'll run your code with a whole range of compiler options. It assumes your code has a non-zero exit code when the bug occurs.

.. code::

        lua $1 > /dev/null
        echo "lua OK"
        ujit -joff $1 > /dev/null
        echo "ujit -joff OK"

        for i in {1..50}
        do
        echo -Ohotloop=$i
        ujit -Ohotloop=$i -p test.dump $1 > /dev/null
        if [ $? -ne 0 ]
        then
            echo "Error on " ujit -Ohotloop=$i -p test.dump $1 "> /dev/null"
            mv test.dump error.dump
            for o in "" "1" "2" "3" "-fold" "-cse" "-dce" "-narrow" "-loop" "-fwd" "-dse" "-abc" "-fuse"
            do
            echo -O$o -Ohotloop=$i
            ujit -O$o -Ohotloop=$i -p test.dump $1 > /dev/null
            done
            break
        fi
        done

        rm test.dump

Turn the compiler off
----------------------

Even if you have a large test case and can't reduce it much, the problem trace could still be a short piece of code. You can reduce the size of the dump and track down the problem by turning the jit compiler off for your modules one-by-one. Put ``if jit then jit.off(true, true) end`` at the top of your modules, and see if the problem goes away. If it does go away, remove the line. If the problem persists, that module doesn't need to be compiled to exhibit the problem, and you've just made the dump smaller.

Find unused code
-----------------

Sorry for the shameless plug, but I use `luatrace <http://github.com/geoffleyland/luatrace>`__ to find unused code that can (possibly) be removed, again reducing the size of the test case.

Run ``lua -luatrace.profile <file.lua>`` and look at ``annotated-source.txt`` - you'll see which lines aren't executed.

Of course, as I said earlier, not all unused code can be removed.
