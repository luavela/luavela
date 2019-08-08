.. _code-style:

Coding Style
============

Rationale
---------

We write and we read lots of code. This document tries to summarize our best practices and approaches on how to make our coding life easier. This includes tips and rules on how to format the code, how to make particular design and implementation decisions (in the parts where such guidance seems to give any profit). When in doubt, don't hesitate to communicate and make it a collective decision.

Basis
------

The basis of our coding style for the core code base is the `Linux kernel coding style <https://www.kernel.org/doc/html/v4.10/process/coding-style.html>`_. If you read it closely, you might find it way too strict, but keep in mind - literally tens of thousands of developers all around the world tend to live with it - and still deliver the product good enough to power household appliances, so there's a glimpse of hope these guys know what they are doing. At the time of the writing, the compliance to this style guide is merely adopted, but we're in the constant process of improving our code base.

Automatic Checks
----------------

We use ``clang-format``, a tool for automatic style checks with the configuration file adopted from the Linux kernel source tree. A small convenience wrapper was created to circumvent some |PROJECT|-specific cases. The major specific case is co-existence of two coding styles, which is addressed by blacklisting certain files (see ``scripts/run-clang-format``). Please use following commands for checking and formatting patches, individual files or the entire code base:

.. code-block:: sh

    # Check individual files, with blacklisting:
    $ ./scripts/run-clang-format --pretend --file src/uj_state.c --file src/uj_str.c
    
    # Check individual files, without blacklisting:
    $ ./scripts/run-clang-format --pretend --file src/uj_state.c --file src/uj_str.c --force

    # Check the entire code base, with blacklisting:
    $ make clang_format

    # Ditto
    $ ./scripts/run-clang-format --pretend
    
    # *REFORMAT* the entire code base, with blacklisting:
    $ ./scripts/run-clang-format
    # At this point you do want to review and commit changes


Additional Rules for the Core Code Base
---------------------------------------

Definitions, Initializers, etc
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Do not use comma after the last enumeration member, struct field initializer, etc.
- If a function has attributes (usually they are hidden behind defines like ``LUA_API``, etc.), specify them in the definition as well as in the declaration.

Declarations
^^^^^^^^^^^^

- Use forward declaration for structures in headers. If you suddenly feel that the list of predeclared structures is too long, it is probably time to refactor corresponding code.
- Do not use forward declarations for functions. 

Preprocessor: ``#include``'s
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Do not include headers in the middle of a file.
- Avoid "indirect" includes: If both translation unit and its header depend on, say, ``<stdint.h>``, do not include ``uj_blah.h`` into ``uj_blah.c``, include ``<stdint.h>`` in both ``uj_blah.h`` and ``uj_blah.c``. There are several violations of this rule in the code base and fixes to this are welcome.

Miscellaneous
^^^^^^^^^^^^^^

- Equality checks in conditional expressions: both natural style (``if (answer == 42) {...}``) and Yoda style (``if (42 == answer) {...}``) are allowed, but please be consistent within a header / translation unit.
- Always use ``const type*`` where appropriate, i.e. where program logic assumes read-only memory.

Legacy Coding Style and Code Styling During Transition Period
-------------------------------------------------------------

As mentioned above, we are still in progress of adoption the new guide. It means that a part of our code base is still under governance of the :ref:`legacy-c`. Following order applies during transition between the two coding styles:

All new module and header files are created according to the new coding style. The rule of thumb: All new modules and headers have the ``uj_`` prefix, while the old ones have the ``lj_`` prefix.

All changes in existing modules and header files preserve the current coding style of these files.

All module and header files using the legacy code style should be eventually restyled to the new coding style. Please apply this procedure responsibly. Restyling should not overlap any other changes (except for possible refactoring to comply with the new style guide which is strict about code nesting). Besides, restyling is possible on per-file basis only (see previous rule).