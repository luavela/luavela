-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

assert(jit.status() == false) -- just to stay on the safe side

local usesfenv = ujit.usesfenv

local function assert_usesfenv_error_argt(...)
    local status, errmsg = pcall(...)
    assert(status == false)
    assert(string.match(errmsg, "function expected"))
end

assert_usesfenv_error_argt(usesfenv)
assert_usesfenv_error_argt(usesfenv, 42, 1984)
assert_usesfenv_error_argt(usesfenv, nil)
assert_usesfenv_error_argt(usesfenv, true)
assert_usesfenv_error_argt(usesfenv, false)
assert_usesfenv_error_argt(usesfenv, 42)
assert_usesfenv_error_argt(usesfenv, "42")
assert_usesfenv_error_argt(usesfenv, {42})

GLOBAL_FLAG1  = true
GLOBAL_FLAG2  = true
local upvalue = true

-- A typical function that is assumed to *not* use its fenv:
--  * No access to globals
--  * No upvalues
--  * All calls are performed through delegates
local function foo(delegate, arg1, arg2, ...)
    return arg1 + delegate(arg2, ...)
end

local function foo_gget()
    return GLOBAL_FLAG1
end

local function foo_gset()
    GLOBAL_FLAG2 = false
end

local function foo_uget()
    return upvalue and 42 or 1984
end

local function foo_uset()
    upvalue = false
end

assert(usesfenv(foo) == false)
assert(usesfenv(math.sin) == false) -- built-ins are assumed fenv-independent

assert(usesfenv(foo_gget) == true)
assert(usesfenv(foo_gset) == true)
assert(usesfenv(foo_uget) == true)
assert(usesfenv(foo_uset) == true)
