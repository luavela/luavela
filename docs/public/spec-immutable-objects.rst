.. _spec-immutable-objects:

|PROJECT| Immutable Objects
===========================

Introduction
------------

This article describe support for immutable objects in |PROJECT|. The feature was released as a part of |PROJECT| 0.14.

Motivation
----------

Consider the following cases:

    -  Implementing read-only objects (first of all tables).
    -  Protecting the environment making it read-only (e.g. to ensure security when running some sandboxed code).

Lua provides no language-level facilities to create immutable tables, but both problems can be addressed using metatables. However, |PROJECT| already has a mechanism of sealing that combines two features: Sealed objects (a) are not garbage collected and (b) are immutable.

It means that all Lua code executed by |PROJECT| already pays the price of run-time immutability checks, but it also has to pay an extra price in case of using metatables for solving problems like the ones mentioned above. This leads to the idea of decomposing sealing into two independent features and exposing interfaces for creating immutable objects to the client code. Of course, mechanism of metatables must be left intact to preserve compatibility with the language.

For a broader discussion of immutability in general, see `here <https://en.wikipedia.org/wiki/Immutable_object>`__.

For a broader discussion of immutability in Lua, see `here <http://lua-users.org/wiki/ImmutableObjects>`__.

Terms and Definitions
---------------------

.. glossary::

    Immutable object
      An object is  *immutable* if it is impossible to mutate its state through any of the platform's interfaces. An object is called *mutable* otherwise. Note that the concept of "object state" in this definition does not include a "private" part of the object that is accessible only within the platform. Please make no assumptions about immutability of this private part.

Interfaces
----------

Lua-level: ``ujit.immutable``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: lua

    v = ujit.immutable(v)

Makes value stored in ``v`` immutable in-place. Returns ``v`` for convenience. The table below outlines effects of applying this interfaces to values of all basic Lua types (listed according to the `Reference Manual <https://www.lua.org/manual/5.1/>`_):

    ============ =======================================================================================================================================================================================================================================================================================================
    Type         Description
    ============ =======================================================================================================================================================================================================================================================================================================
    ``nil``      No effect: All values of this type are always immutable.
    ``boolean``  No effect: All values of this type are always immutable.
    ``number``   No effect: All values of this type are always immutable.
    ``string``   No effect: All values of this type are always immutable.
    ``function`` No effect: All values of this type are always immutable.
    ``userdata`` No effect: All values of this type are considered immutable.
    ``thread``   Not supported: cannot be made immutable, a run-time error is thrown.
    ``table``    Tables are made recursively immutable unless no key or value is of the type that cannot be made immutable (see above). If there is a key or value of the type that cannot be made immutable, a run-time error is thrown. If a table has a metatable, this metatable is made recursively immutable, too.
    ============ =======================================================================================================================================================================================================================================================================================================

Notes on Immutable Tables
"""""""""""""""""""""""""

Since immutability is applied recursively, a table nested into some immutable table is also immutable:

.. code-block:: lua

    local tbl = ujit.immutable{ foo = { bar = "baz" } }
    local foo = tbl.foo
    tbl.x = "y" -- ERROR!
    foo.y = "x" -- ERROR!

However, all tables that are created anew are mutable by default, so creating a deep copy of an immutable table will not preserve immutability.

Besides, following is true for immutable tables:

    -  Immutable tables can have metatables. However, ``__newindex`` metamethod will not be reachable for immutable tables.
    -  After a table is made immutable, new metatables cannot be set for the table. In particular, a metatable cannot be unset for the table that had that metatable at the time of being made immutable.
    -  After a table is made immutable, the contents of its metatable is also immutable.

Rationale: A table without a metatable represent some piece of data. A table with a metatable represents some piece of data and some additional semantics on this data. For the sake of consistency, we should force immutability on both data and any additional semantics if it is defined.

Legacy Notes on Functions and ``userdata``
""""""""""""""""""""""""""""""""""""""""""

For functions, following was true prior to |PROJECT| 0.16: If a function has no upvalues, applying this interface has no effect. Otherwise applying this interface is not supported, and a run-time error is thrown.

For ``userdata``, following was true prior to |PROJECT| 0.16: ``userdata`` could not be made immutable, a run-time error was thrown.

Preserving Consistent Object State
""""""""""""""""""""""""""""""""""

If an attempt to make an object immutable fails, it is guaranteed that its state will be left intact:

.. code-block:: lua

    local co = coroutine.create(function () end)
    local t = {
        nested = {
            nested = {
                oops = co, -- expected to fail
            },
        },
    }

    local status, err_msg = pcall(ujit.immutable, t)
    assert(status == false)
    t.foo = "bar"                -- OK
    t.nested.foo = "bar"         -- OK
    t.nested.nested.foo = "bar"  -- OK

In particular, if a mutable object holds a reference to an immutable object and fails to become immutable, state of both objects is preserved:

.. code-block:: lua

    local co = coroutine.create(function () end)
    local t = {
        nested = {
            nested = ujit.ummutable{},
            oops = co, -- expected to fail
        },
    }

    local status, err_msg = pcall(ujit.immutable, t)
    assert(status == false)
    t.foo = "bar"                -- OK
    t.nested.foo = "bar"         -- OK
    t.nested.nested.foo = "bar"  -- ERROR!

Immutability and Sealing
""""""""""""""""""""""""

Sealing implies immutability: If an object is made immutable and if sealing can be applied to that object, this object will be successfully sealed. If an object is successfully sealed, an attempt to make it immutable is a no-op.

Immutability and Garbage Collector
""""""""""""""""""""""""""""""""""

Immutable and mutable objects are treated equally by garbage collector.

Making Immutable Objects Immutable Again
""""""""""""""""""""""""""""""""""""""""

Applying this interface to the already immutable object has no effect.

C API:  ``luaE_immutable``
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    void luaE_immutable(lua_State *L, int idx);

Makes the value stored at index ``idx`` immutable in-place. It has exactly the same semantics as the Lua-level interface.

Turning Immutability Off
^^^^^^^^^^^^^^^^^^^^^^^^

There are deliberately no interfaces for making immutable objects mutable again. Here is an example why introducing such interface is potentially dangerous:

.. code-block:: lua

    local t = ujit.immutable{ a = { b = "c" }, foo = "bar" }
    local x = ujit.mutable(t.a) -- contents of t.a is now mutable, but t is in inconsistent state

Support by JIT Compiler
-----------------------

Prior to |PROJECT| 0.18, a call to ``ujit.immutable`` was not  JIT-compiled. As of |PROJECT| 0.18, a call to ``ujit.immutable`` is JIT-compiled.

Implementation Notes
--------------------

No extra notes currently.

Performance Considerations
--------------------------

Please remember that marking an object immutable requires a full traversal in case of tables. It means that using the features for large tables or tables with deep nesting should be done carefully, and performance of according code must be monitored closely. However, please note that accessing immutable objects will not bring any overhead since appropriate checks are already in place to support sealing (see above).

Discussed and Abandoned Ideas
-----------------------------

Semi-read-only Tables
^^^^^^^^^^^^^^^^^^^^^

There was an idea to implement "semi-read-only tables" that would allow insertion and prohibit modification of existing values:

.. code-block:: lua

    local t = semireadonly{a = "b", c = "d"}
    t.e = "f" -- OK
    t.e = "F" -- ERROR!
    t.a = "B" -- ERROR!

However, the idea was abandoned because of following reasons:

1. This approach erodes the idea of immutability in principle.
2. A need to implement such interface indicates that the bad code design.

References
----------

1. https://en.wikipedia.org/wiki/Immutable_object
2. http://lua-users.org/wiki/ImmutableObjects
3. `Lua 5.1 Reference Manual <https://www.lua.org/manual/5.1/>`_
