.. _tut-assembling-lookup:

Tutorial: Assembling Lookup in Tables
=====================================

.. contents:: :local:

Introduction
------------

Table is the only data structure in Lua, which makes understanding of its internals crucial for understanding our platform. Loads from / stores to tables are supported in the JIT compiler, but it may be difficult to understand corresponding code right off (the most obvious reason is that IR recording and machine code generation happen in opposite directions). This article provides a commented example.

Table Lookup in Lua, Byte Code and IR
-------------------------------------

Consider following Lua code:

.. code-block:: lua

    function Class:foo(sbc, request)
        -- ...
        local user_id = request.user.id
        -- ...
    end

We will be interested in how |PROJECT| interprets and compiles a load of value ``request.user.id``. The byte code from accessing this value will look like:

.. code::

    0063 TGETS 9 2 25 ; "user" -- Class:foo has an implicit "self" argument, hence 'request' occupies the 2nd slot in frame
    0064 TGETS 9 9 12 ; "id"

During trace recording, this byte code will be converted to an IR which will look like (see here for notation):

.. code::

    0009 [8]   >  tab SLOAD  #3    T
    ...
    0166 rbx      p32 HREF   0009  "user"
    0167 rdx   >  tab HLOAD  0166
    0168 rbx      p32 HREF   0167  "id"
    0169 rbx   >  str HLOAD  0168

Following should be taken into account:

- Typed IR results like tab, str etc. should be interpreted as ``GCobj*``. In other words, IR instruction #167 specifies that if the type assertion (denoted with >) succeeds, register rdx will contain a ``GCtab*`` value.
- The ``HREF`` instruction returns a reference to a value in a table, which is currently a ``TValue*`` (see definition of ``struct Node`` for details).

That said, IR instruction #166 can be read as: Take the result of #9 (if we have reached #166, then we know for sure that this is a table), take the string key "user" (this is a compile-time constant as it originates from a string literal in the Lua code), look up a value corresponding to this key and place a pointer to this value to rbx. And the next instruction #167 is simply a (pseudo-assembly, Intel syntax):

.. code::

    mov rdx, tabV(rbx)

Assembling ``HREF``
-------------------

Machine code for various cases of ``HREF`` is one of the most tricky things in the code generator. Here is the commented example of the output for instruction #168:

.. code::

    ; Prerequisites:
    ;  * rdx is a (GCtab *)
    ;  * Information about the "id" literal will be spread across the machine code
    ; size_t rbx = rdx->hmask
    ; 32-bit ebx is enough for holding hmask and derivatives, but I'll denote it as rbx for consistency
    05fdeeaf mov ebx, [rdx+0x38]

    ; rbx &= hash("id")
    ; As we have a string literal as a key, we can insert its hash at the time of code generation:
    05fdeeb2 and ebx, 0x9fcae5e1

    ; rbx *= sizeof(struct Node)
    05fdeeb8 imul ebx, ebx, 0x28

    ; Node *rbx = (Node *)(((char *)(rdx->node))[rbx]) /* NB! rbx changes its type now */
    05fdeebb add rbx, [rdx+0x28]

    ; if (!tvisstr(rbx->key)) goto next_key_in_bucket
    check_key_in_bucket:
    05fdeebf cmp dword [rbx+0x18], 0xfffffffb
    05fdeec3 jnz 0x5fdeed5

    ; GCstr *r11 = (GCstr *)"id"
    ; As we have a string literal as a key, we can insert its address directly into machine code:
    05fdeec5 mov r11, 0x7f7fdc36e670

    ; if (r11 == strV(rbx->key)) goto value_found
    ; Because strings are interned, we can compare them by comparing corresponding pointers to GCstr *
    05fdeecf cmp r11, [rbx+0x10]
    05fdeed3 jz 0x5fdeee8

    ; rbx = rbx->next
    next_key_in_bucket:
    05fdeed5 mov rbx, [rbx+0x20]
    ; if (rbx != NULL) goto check_key_in_bucket
    05fdeed9 test rbx, rbx
    05fdeedc jnz 0x5fdeebf

    ; rbx = niltvg(g)
    05fdeede mov rbx, 0x7f7fdc3554f0

    value_found:
    ; rbx is either a (TValue *) pointing to a value corresponding to the given key (offsetof(struct Node, val) == 0)
    ; ... or a pointer to a common nil value (a single value per global_State).
    05fdeee8 ...

For completeness, here is the same code as it looks in the raw dump:

.. code::

    05fdeeaf mov ebx, [rdx+0x38]
    05fdeeb2 and ebx, 0x9fcae5e1
    05fdeeb8 imul ebx, ebx, 0x28
    05fdeebb add rbx, [rdx+0x28]
    05fdeebf cmp dword [rbx+0x18], 0xfffffffb
    05fdeec3 jnz 0x5fdeed5
    05fdeec5 mov r11, 0x7f7fdc36e670
    05fdeecf cmp r11, [rbx+0x10]
    05fdeed3 jz 0x5fdeee8
    05fdeed5 mov rbx, [rbx+0x20]
    05fdeed9 test rbx, rbx
    05fdeedc jnz 0x5fdeebf
    05fdeede mov rbx, 0x7f7fdc3554f0
    05fdeee8 ...
