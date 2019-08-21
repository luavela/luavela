.. _pub-pkg-config:

Embedding |PROJECT| Using pkg-config
====================================

Introduction
------------

As of version 0.16, a ``pkg-config`` file for |PROJECT| is provided for easier embedding |PROJECT| into other programs. This short tutorial will guide you through this process.

Prerequisites
-------------

You need to have |PROJECT| installed on your system to proceed.

Sample C Program
----------------

Assume we have a following file named ``sample.c``:

.. code::

   #include <ujit/lua.h>
   #include <ujit/lauxlib.h>

   int main()
   {
       lua_State *L = luaL_newstate();
       lua_close(L);
       return 0;
   }

Compilation and Linking
-----------------------

You can compile and link your program which embeds |PROJECT| using following commands:

.. code::

   $ gcc -Wall -Werror -ggdb3 `pkg-config --cflags ujit` -c sample.c -o sample.o
   $ gcc -Wall -Werror sample.o `pkg-config --libs ujit` -o sample

``pkg-config`` will be used to provide all necessary information about paths to include files, libraries, etc.

.. note::

   |PROJECT|'s ``pkg-config`` file provides facilities for **dynamic linking only**.

What's Next
-----------

You can extend your sample program using `C API for Lua <https://www.lua.org/manual/5.1/manual.html#3.6>`_ and/or |PROJECT|'s :ref:`pub-api-reference`.

References
----------

   - `pkg-config Guide <https://people.freedesktop.org/~dbn/pkg-config-guide.html>`_
