#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2009-2019, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 Lua Debug Library

=head2 Synopsis

    % prove 310-debug.t

=head2 Description

Tests Lua Debug Library

See section "The Debug Library" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#5.9>
L<https://www.lua.org/manual/5.2/manual.html#6.10>,
L<https://www.lua.org/manual/5.3/manual.html#6.10>

=cut

]]

require 'tap'
local profile = require'profile'
local has_getfenv = _VERSION == 'Lua 5.1'
local has_getlocal52 = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_setmetatable52 = _VERSION >= 'Lua 5.2' or (profile.luajit_compat52 and not ujit)
local has_getuservalue = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_getuservalue54 = _VERSION >= 'Lua 5.4'
local has_upvalueid = _VERSION >= 'Lua 5.2' or jit
local has_upvaluejoin = _VERSION >= 'Lua 5.2' or jit

plan'no_plan'

local debug = require 'debug'

-- getfenv
if has_getfenv then
    is(debug.getfenv(3.14), nil, "function getfenv")
    local function f () end
    type_ok(debug.getfenv(f), 'table')
    is(debug.getfenv(f), _G)
    type_ok(debug.getfenv(print), 'table')
    is(debug.getfenv(print), _G)

    local a = coroutine.create(function () return 1 end)
    type_ok(debug.getfenv(a), 'table', "function getfenv (thread)")
    is(debug.getfenv(a), _G)
else
    is(debug.getfenv, nil, "no debug.getfenv (removed)")
end

do -- getinfo
    local info = debug.getinfo(is)
    type_ok(info, 'table', "function getinfo (function)")
    is(info.func, is, " .func")

    info = debug.getinfo(is, 'L')
    type_ok(info, 'table', "function getinfo (function, opt)")
    type_ok(info.activelines, 'table')

    info = debug.getinfo(1)
    type_ok(info, 'table', "function getinfo (level)")
    like(info.func, "^function: [0]?[Xx]?%x+", " .func")

    is(debug.getinfo(12), nil, "function getinfo (too depth)")

    error_like(function () debug.getinfo('bad') end,
               "bad argument #1 to 'getinfo' %(.- expected",
               "function getinfo (bad arg)")

    error_like(function () debug.getinfo(is, 'X') end,
               "bad argument #2 to 'getinfo' %(invalid option%)",
               "function getinfo (bad opt)")
end

do -- getlocal
    local name, value = debug.getlocal(0, 1)
    type_ok(name, 'string', "function getlocal (level)")
    is(value, 0)

    error_like(function () debug.getlocal(42, 1) end,
               "bad argument #1 to 'getlocal' %(level out of range%)",
               "function getlocal (out of range)")

    if has_getlocal52 then
        name, value = debug.getlocal(like, 1)
        type_ok(name, 'string', "function getlocal (func)")
        is(value, nil)
    else
        diag("no getlocal with function")
    end
end

do -- getmetatable
    local t = {}
    is(debug.getmetatable(t), nil, "function getmetatable")
    local t1 = {}
    debug.setmetatable(t, t1)
    is(debug.getmetatable(t), t1)

    local a = true
    is(debug.getmetatable(a), nil)
    debug.setmetatable(a, t1)
    is(debug.getmetatable(t), t1)

    a = 3.14
    is(debug.getmetatable(a), nil)
    debug.setmetatable(a, t1)
    is(debug.getmetatable(t), t1)
end

do -- getregistry
    local reg = debug.getregistry()
    type_ok(reg, 'table', "function getregistry")
    type_ok(reg._LOADED, 'table')
end

do -- getupvalue
    local name = debug.getupvalue(plan, 1)
    type_ok(name, 'string', "function getupvalue")
end

do -- gethook
    debug.sethook()
    local hook, mask, count = debug.gethook()
    is(hook, nil, "function gethook")
    is(mask, '')
    is(count, 0)
    local function f () end
    debug.sethook(f, 'c', 42)
    hook , mask, count = debug.gethook()
    is(hook, f, "function gethook")
    is(mask, 'c')
    is(count, 42)

    local co = coroutine.create(function () print "thread" end)
    hook = debug.gethook(co)
    if jit then
        type_ok(hook, 'function', "function gethook(thread)")
    else
        is(hook, nil, "function gethook(thread)")
    end
end

do -- setlocal
    local name = debug.setlocal(0, 1, 0)
    type_ok(name, 'string', "function setlocal (level)")

    name = debug.setlocal(0, 42, 0)
    is(name, nil, "function setlocal (level)")

    error_like(function () debug.setlocal(42, 1, true) end,
               "bad argument #1 to 'setlocal' %(level out of range%)",
               "function setlocal (out of range)")
end

-- setfenv
if has_getfenv then
    local t = {}
    local function f () end
    is(debug.setfenv(f, t), f, "function setfenv")
    type_ok(debug.getfenv(f), 'table')
    is(debug.getfenv(f), t)
    is(debug.setfenv(print, t), print)
    type_ok(debug.getfenv(print), 'table')
    is(debug.getfenv(print), t)

    t = {}
    local a = coroutine.create(function () return 1 end)
    is(debug.setfenv(a, t), a, "function setfenv (thread)")
    type_ok(debug.getfenv(a), 'table')
    is(debug.getfenv(a), t)

    error_like(function () t = {}; debug.setfenv(t, t) end,
               "^[^:]+:%d+: 'setfenv' cannot change environment of given object",
               "function setfenv (forbidden)")
else
    is(debug.setfenv, nil, "no debug.setfenv (removed)")
end

do -- setmetatable
    local t = {}
    local t1 = {}
    if has_setmetatable52 then
        is(debug.setmetatable(t, t1), t, "function setmetatable")
    else
        is(debug.setmetatable(t, t1), true, "function setmetatable")
    end
    is(getmetatable(t), t1)

    error_like(function () debug.setmetatable(t, true) end,
               "^[^:]+:%d+: bad argument #2 to 'setmetatable' %(nil or table expected")
end

do -- setupvalue
    local r, tb = pcall(require, 'Test.Builder')
    local value = r and tb:new() or {}
    local name = debug.setupvalue(plan, 1, value)
    type_ok(name, 'string', "function setupvalue")

    name = debug.setupvalue(plan, 42, true)
    is(name, nil)
end

-- getuservalue / setuservalue
if has_getuservalue54 then
    local u = io.tmpfile()      -- lua_newuserdatauv(L, sizeof(LStream), 0);
    is(debug.getuservalue(u, 0), nil, "function getuservalue")
    is(debug.getuservalue(true), nil)

    error_like(function () debug.getuservalue(u, 'foo') end,
               "^[^:]+:%d+: bad argument #2 to 'getuservalue' %(number expected, got string%)")

    local data = {}
    is(debug.setuservalue(u, data, 42), nil, "function setuservalue")

    error_like(function () debug.setuservalue({}, data) end,
               "^[^:]+:%d+: bad argument #1 to 'setuservalue' %(userdata expected, got table%)")

    error_like(function () debug.setuservalue(u, data, 'foo') end,
               "^[^:]+:%d+: bad argument #3 to 'setuservalue' %(number expected, got string%)")
elseif has_getuservalue then
    local u = io.tmpfile()
    local old = debug.getuservalue(u)
    if jit then
        type_ok(old, 'table', "function getuservalue")
    else
        is(old, nil, "function getuservalue")
    end
    is(debug.getuservalue(true), nil)

    local data = {}
    local r = debug.setuservalue(u, data)
    is(r, u, "function setuservalue")
    is(debug.getuservalue(u), data)
    r = debug.setuservalue(u, old)
    is(debug.getuservalue(u), old)

    error_like(function () debug.setuservalue({}, data) end,
               "^[^:]+:%d+: bad argument #1 to 'setuservalue' %(userdata expected, got table%)")
else
    is(debug.getuservalue, nil, "no getuservalue")
    is(debug.setuservalue, nil, "no setuservalue")
end

do -- traceback
    like(debug.traceback(), "^stack traceback:\n", "function traceback")

    like(debug.traceback("message\n"), "^message\n\nstack traceback:\n", "function traceback with message")

    like(debug.traceback(false), "false", "function traceback")
end

-- upvalueid
if has_upvalueid then
    local id = debug.upvalueid(plan, 1)
    type_ok(id, 'userdata', "function upvalueid")
else
    is(debug.upvalueid, nil, "no upvalueid")
end

-- upvaluejoin
if has_upvaluejoin and jit then
    diag("jit upvaluejoin")
    -- TODO
elseif has_upvaluejoin then
    debug.upvaluejoin(pass, 1, fail, 1)

    error_like(function () debug.upvaluejoin(true, 1, nil, 1) end,
               "bad argument #1 to 'upvaluejoin' %(function expected, got boolean%)",
               "function upvaluejoin (bad arg)")

    error_like(function () debug.upvaluejoin(pass, 1, true, 1) end,
               "bad argument #3 to 'upvaluejoin' %(function expected, got boolean%)",
               "function upvaluejoin (bad arg)")
else
    is(debug.upvaluejoin, nil, "no upvaluejoin")
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
