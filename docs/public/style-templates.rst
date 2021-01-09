.. _style-templates:

Code Base Templates
===================

.. contents:: :local:

Introduction
-------------

This document extends and provides examples to the :ref:`style-main`.

Template for a Translation Unit (aka Module)
--------------------------------------------

.. code-block:: c

    /*
     * Any appropriate description of the module.
     * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
     * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
     *
     * Portions taken verbatim or adapted from LuaJIT.
     * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
     *
     * Portions taken verbatim or adapted from the Lua interpreter.
     * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
     */

    #include "..."
    #include "..."

    static void module_assert_something(lua_State *L, const GCtab *t)
    {
    /* Please comment all #else and #endif directives. */
    #ifndef NDEBUG
         lua_assert(...);
         lua_assert(...);
         lua_assert(...);
    #else /* NDEBUG */
         UNUSED(L);
         UNUSED(t);
    #endif /* !NDEBUG */
    }

    /*
     * 1. Please do not append uj_ prefix for static routines.
     * 2. Please do append module_ prefix for static routines.
     * 3. Please do not forget about const.
     * 4. If your routine (either static or a part of the module's public API)
     *    notifies a caller with a return code, use non-0 values
     *    for indicating errors, and 0 for indicating success.
     */
    static int module_do_something(lua_State *L, const GCtab *t)
    {
         /* ...code... */

         if (something_wrong)
                 return 1;

         module_assert_something(L, t);
         return 0;
    }

    /*
     * If your routine answers a Yes/No question,
     * use non-0 values for "yes", and 0 for "no".
     */
    static int module_is_something(...)
    {
         return check_condition() ? 1 : 0;
    }

    /*
     * For members of the module's public API,
     * please append uj_module_ prefix.
     */
    lua_State *uj_module_public_api()
    {
         /* ...code... */
    }

Template for a Header File
--------------------------

.. code-block:: c

    /*
     * Any appropriate description of the header.
     * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
     * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
     *
     * Portions taken verbatim or adapted from LuaJIT.
     * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
     *
     * Portions taken verbatim or adapted from the Lua interpreter.
     * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
     */

    #ifndef _UJ_MODULE_H
    #define _UJ_MODULE_H

    #include "..."
    #include "..."

    /* Forward-declare where it makes sense: */

    struct lua_State;
    struct GCtab;
    union TValue;

    /*
     * Please avoid using macros. For lightweight, always
     * inlined interfaces use the following idiom:
     */
    static LJ_AINLINE const TValue *uj_module_fast(lua_State *L, GCtab *mt)
    {
         return ...;
    }

    /* Function description. */
    lua_State *uj_module_public_api();

    #endif /* !_UJ_MODULE_H */

Flower Boxes
------------

The Contents of Flower Boxes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In general a flower box (a comment placed right in the beginning of a file) consists of four parts:

1. Module/Header description + LuaVela's copyright note. Must be present in all files (both modules and headers) across the |PROJECT| code base.
2. IPONWEB's copyright note. Must be present in all files (both modules and headers) across the |PROJECT| code base created before the project was transferred to LuaVela organization on GitHub.
3. LuaJIT-related copyright note. Must be present in all files that were adopted from the LuaJIT code base.
4. PUC-Rio Lua-related copyright note. Must be preserved in all cases when Mike Pall placed this notice in the LuaJIT code base files.

Flower Boxes for Non-Core Code Base
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CMake Files
"""""""""""

.. code-block:: cmake

    # Purpose of the file.
    # Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

Tools
"""""

**Perl**

.. code-block:: perl

    #!/usr/bin/perl -w
    #
    # This is a tool for doing blah-blah-blah
    # Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

**C**

Follows the same rules as for the core code base. Obviously LuaJIT and PUC-Rio Lua must **not** be mentioned.

Tests
"""""

**Lua**

*Verbose Version*

.. code-block:: lua

    -- Description of the chunk (what it actually tests, etc.).
    -- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

*Acceptable Default*

.. code-block:: lua

    -- This is a part of uJIT's testing suite.
    -- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

**Perl**

*Verbose Version*

.. code-block:: perl

    #!/usr/bin/perl
    #
    # Description of the test (what it actually tests, etc.).
    # Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

*Acceptable Default*

.. code-block:: perl

    #!/usr/bin/perl
    #
    # This is a part of uJIT's testing suite.
    # Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

**C**

*Verbose Version*

Follows the same rules as for the core code base. Obviously LuaJIT and PUC-Rio Lua must  **not** be mentioned.

*Acceptable Default*

.. code-block:: c

    /*
     * This is a part of uJIT's testing suite.
     * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
     */

**Shell Runners**

*Verbose Version*

.. code-block:: sh

    #!/bin/bash
    #
    # Description of the runner.
    # Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

*Acceptable Default*

.. code-block:: sh

    #!/bin/bash
    #
    # This is a part of uJIT's testing suite.
    # Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT

Copyright Notice Format
^^^^^^^^^^^^^^^^^^^^^^^

There is no explicit policy on the matter, but for the sake of cross-project consistency following format/spelling is adopted:

.. code-block:: none

    Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
