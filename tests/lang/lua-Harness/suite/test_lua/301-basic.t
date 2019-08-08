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

=head1 Lua Basic Library

=head2 Synopsis

    % prove 301-basic.t

=head2 Description

Tests Lua Basic Library

See section "Basic Functions" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#5.1>,
L<https://www.lua.org/manual/5.2/manual.html#6.1>,
L<https://www.lua.org/manual/5.3/manual.html#6.1>

=cut

--]]

require'tap'
local profile = require'profile'
local has_error53 = _VERSION >= 'Lua 5.3'
local has_gcinfo = _VERSION == 'Lua 5.1'
local has_getfenv = _VERSION == 'Lua 5.1'
local has_ipairs53 = _VERSION >= 'Lua 5.3'
local has_load52 = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_loadfile52 = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_loadstring = _VERSION == 'Lua 5.1'
local has_alias_loadstring = profile.compat51
local has_newproxy = _VERSION == 'Lua 5.1'
local has_rawlen = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_unpack = _VERSION == 'Lua 5.1'
local has_alias_unpack = profile.compat51
local has_xpcall52 = _VERSION >= 'Lua 5.2' or jit
local has_xpcall53 = _VERSION >= 'Lua 5.3' or jit

plan'no_plan'

do -- assert
    local v, msg = assert('text', "assert string")
    is(v, 'text', "function assert")
    is(msg, "assert string")
    v, msg = assert({}, "assert table")
    is(msg, "assert table")

    error_like(function () assert(false, "ASSERTION TEST") end,
               "^[^:]+:%d+: ASSERTION TEST",
               "function assert(false, msg)")

    error_like(function () assert(false) end,
               "^[^:]+:%d+: assertion failed!",
               "function assert(false)")

    if has_error53 then
        v, msg = pcall(function() assert(false, 42) end)
        is(msg, 42, "function assert(false, 42)")
    else
        error_like(function () assert(false, 42) end,
                   "^[^:]+:%d+: 42",
                   "function assert(false, 42)")
    end

    if has_error53 then
        local obj = {}
        v, msg = pcall(function() assert(false, obj) end)
        is(msg, obj, "function assert(false, {})")
    else
        diag("no assert with table")
    end
end

-- collectgarbage
if jit then
    is(collectgarbage('stop'), 0, "function collectgarbage 'stop/restart/collect'")
    is(collectgarbage('step'), false)
    is(collectgarbage('restart'), 0)
    is(collectgarbage('step'), false)
    is(collectgarbage('collect'), 0)
    is(collectgarbage('setpause', 10), 200)
    is(collectgarbage('setstepmul', 200), 200)
    is(collectgarbage(), 0)
elseif _VERSION == 'Lua 5.1' then
    is(collectgarbage('stop'), 0, "function collectgarbage 'stop/restart/collect'")
    is(collectgarbage('restart'), 0)
    is(collectgarbage('step'), false)
    is(collectgarbage('collect'), 0)
    is(collectgarbage(), 0)
elseif _VERSION == 'Lua 5.2' then
    is(collectgarbage('stop'), 0, "function collectgarbage 'stop/restart/collect'")
    is(collectgarbage('isrunning'), false)
    is(collectgarbage('step'), false)
    is(collectgarbage('restart'), 0)
    is(collectgarbage('isrunning'), true)
    is(collectgarbage('step'), false)
    is(collectgarbage('collect'), 0)
    is(collectgarbage('setpause', 10), 200)
    is(collectgarbage('setstepmul', 200), 200)
    is(collectgarbage(), 0)
    is(collectgarbage('generational'), 0)
    is(collectgarbage('step'), false)
    is(collectgarbage('incremental'), 0)
    is(collectgarbage('setmajorinc'), 200)
elseif _VERSION == 'Lua 5.3' then
    is(collectgarbage('stop'), 0, "function collectgarbage 'stop/restart/collect'")
    is(collectgarbage('isrunning'), false)
    --is(collectgarbage('step'), false)
    type_ok(collectgarbage('step'), 'boolean')
    is(collectgarbage('restart'), 0)
    is(collectgarbage('isrunning'), true)
    is(collectgarbage('step'), false)
    is(collectgarbage('collect'), 0)
    is(collectgarbage('setpause', 10), 200)
    is(collectgarbage('setstepmul', 200), 200)
    is(collectgarbage(), 0)
    is(collectgarbage('step'), false)
elseif _VERSION == 'Lua 5.4' then
    is(collectgarbage('stop'), 0, "function collectgarbage 'stop/restart/collect'")
    is(collectgarbage('isrunning'), false)
    is(collectgarbage('generational'), 'generational')
    is(collectgarbage('incremental'), 'generational')
    is(collectgarbage('incremental'), 'incremental')
    is(collectgarbage('generational'), 'incremental')
    is(collectgarbage('step'), false)
    is(collectgarbage('restart'), 0)
    is(collectgarbage('isrunning'), true)
    is(collectgarbage('step'), false)
    is(collectgarbage('collect'), 0)
    is(collectgarbage('setpause', 10), 200)
    is(collectgarbage('setstepmul', 200), 100)
    is(collectgarbage(), 0)
    is(collectgarbage('step'), false)
end

type_ok(collectgarbage('count'), 'number', "function collectgarbage 'count'")

error_like(function () collectgarbage('unknown') end,
           "^[^:]+:%d+: bad argument #1 to 'collectgarbage' %(invalid option 'unknown'%)",
           "function collectgarbage (invalid)")

do -- dofile
    local f = io.open('lib-301.lua', 'w')
    f:write[[
function norm (x, y)
    return (x^2 + y^2)^0.5
end

function twice (x)
    return 2*x
end
]]
    f:close()
    dofile('lib-301.lua')
    local n = norm(3.4, 1.0)
    like(twice(n), '^7%.088', "function dofile")

    os.remove('lib-301.lua') -- clean up

    error_like(function () dofile('no_file-301.lua') end,
               "cannot open no_file%-301%.lua: No such file or directory",
               "function dofile (no file)")

    f = io.open('foo-301.lua', 'w')
    f:write[[?syntax error?]]
    f:close()
    error_like(function () dofile('foo-301.lua') end,
               "^foo%-301%.lua:%d+:",
               "function dofile (syntax error)")

    os.remove('foo-301.lua') -- clean up
end

do -- error
    error_like(function () error("ERROR TEST") end,
               "^[^:]+:%d+: ERROR TEST",
               "function error(msg)")

    error_is(function () error("ERROR TEST", 0) end,
             "ERROR TEST",
             "function error(msg, 0)")

    if has_error53 then
        local v, msg = pcall(function() error(42) end)
        is(msg, 42, "function error(42)")
    else
        error_like(function () error(42) end,
                   "^[^:]+:%d+: 42",
                   "function error(42)")
    end

    local obj = {}
    local v, msg = pcall(function() error(obj) end)
    is(msg, obj, "function error({})")

    v, msg = pcall(function() error() end)
    is(msg, nil, "function error()")
end

-- gcinfo
if has_gcinfo then
    type_ok(gcinfo(), 'number', "function gcinfo")
else
    is(gcinfo, nil, "no gcinfo (removed)")
end

-- getfenv
if has_getfenv then
    type_ok(getfenv(0), 'table', "function getfenv")
    is(getfenv(0), _G)
    is(getfenv(1), _G)
    is(getfenv(), _G)
    local function f () end
    type_ok(getfenv(f), 'table')
    is(getfenv(f), _G)
    type_ok(getfenv(print), 'table')
    is(getfenv(print), _G)

    error_like(function () getfenv(-3) end,
               "^[^:]+:%d+: bad argument #1 to 'getfenv' %(.-level.-%)",
              "function getfenv (negative)")

    error_like(function () getfenv(12) end,
               "^[^:]+:%d+: bad argument #1 to 'getfenv' %(invalid level%)",
               "function getfenv (too depth)")
else
    is(getfenv, nil, "no getfenv")
end

do -- ipairs
    local a = {'a','b','c'}
    if has_ipairs53 then
        a = setmetatable({
            [1] = 'a',
            [3] = 'c',
        }, {
            __index = {
               [2] = 'b',
            }
        })
    end
    local f, v, s = ipairs(a)
    type_ok(f, 'function', "function ipairs")
    type_ok(v, 'table')
    is(s, 0)
    s, v = f(a, s)
    is(s, 1)
    is(v, 'a')
    s, v = f(a, s)
    is(s, 2)
    is(v, 'b')
    s, v = f(a, s)
    is(s, 3)
    is(v, 'c')
    s, v = f(a, s)
    is(s, nil)
    is(v, nil)
end

do -- load
    local t = { [[
function bar (x)
    return x
end
]] }
    local i = 0
    local function reader ()
        i = i + 1
        return t[i]
    end
    local f, msg = load(reader)
    if msg then
        diag(msg)
    end
    type_ok(f, 'function', "function load(reader)")
    is(bar, nil)
    f()
    is(bar('ok'), 'ok')
    bar = nil

    t = { [[
function baz (x)
    return x
end
]] }
    i = -1
    function reader ()
        i = i + 1
        return t[i]
    end
    f, msg = load(reader)
    if msg then
        diag(msg)
    end
    type_ok(f, 'function', "function load(pathological reader)")
    f()
    if _VERSION == 'Lua 5.1' and not jit then
        todo("not with 5.1")
    end
    is(baz, nil)

    t = { [[?syntax error?]] }
    i = 0
    f, msg = load(reader, "errorchunk")
    is(f, nil, "function load(syntax error)")
    like(msg, "^%[string \"errorchunk\"%]:%d+:")

    f = load(function () return nil end)
    type_ok(f, 'function', "when reader returns nothing")

    f, msg = load(function () return {} end)
    is(f, nil, "reader function must return a string")
    like(msg, "reader function must return a string")

    if has_load52 then
        f = load([[
function bar (x)
    return x
end
]])
        is(bar, nil, "function load(str)")
        f()
        is(bar('ok'), 'ok')
        bar = nil

        local env = {}
        f = load([[
function bar (x)
    return x
end
]], "from string", 't', env)
        is(env.bar, nil, "function load(str)")
        f()
        is(env.bar('ok'), 'ok')

        f, msg = load([[?syntax error?]], "errorchunk")
        is(f, nil, "function load(syntax error)")
        like(msg, "^%[string \"errorchunk\"%]:%d+:")

        f, msg = load([[print 'ok']], "chunk txt", 'b')
        like(msg, "attempt to load")
        is(f, nil, "mode")

        f, msg = load("\x1bLua", "chunk bin", 't')
        like(msg, "attempt to load")
        is(f, nil, "mode")
    else
       diag("no load with string")
    end
end

do -- loadfile
    local f = io.open('foo-301.lua', 'w')
    if _VERSION ~= 'Lua 5.1' or jit then
        f:write'\xEF\xBB\xBF' -- BOM
    end
    f:write[[
function foo (x)
    return x
end
]]
    f:close()
    f = loadfile('foo-301.lua')
    is(foo, nil, "function loadfile")
    f()
    is(foo('ok'), 'ok')

    if has_loadfile52 then
        local msg
        f, msg = loadfile('foo-301.lua', 'b')
        like(msg, "attempt to load")
        is(f, nil, "mode")

        local env = {}
        f = loadfile('foo-301.lua', 't', env)
        is(env.foo, nil, "function loadfile")
        f()
        is(env.foo('ok'), 'ok')
    else
        diag("no loadfile with mode & env")
    end

    os.remove('foo-301.lua') -- clean up

    local msg
    f, msg = loadfile('no_file-301.lua')
    is(f, nil, "function loadfile (no file)")
    is(msg, "cannot open no_file-301.lua: No such file or directory")

    f = io.open('foo-301.lua', 'w')
    f:write[[?syntax error?]]
    f:close()
    f, msg = loadfile('foo-301.lua')
    is(f, nil, "function loadfile (syntax error)")
    like(msg, '^foo%-301%.lua:%d+:')
    os.remove('foo-301.lua') -- clean up
end

-- loadstring
if has_loadstring then
    local f = loadstring([[i = i + 1]])
    i = 0
    f()
    is(i, 1, "function loadstring")
    f()
    is(i, 2)

    i = 32
    local i = 0
    f = loadstring([[i = i + 1; return i]])
    local g = function () i = i + 1; return i end
    is(f(), 33, "function loadstring")
    is(g(), 1)

    local msg
    f, msg = loadstring([[?syntax error?]])
    is(f, nil, "function loadstring (syntax error)")
    like(msg, '^%[string "%?syntax error%?"%]:%d+:')
elseif has_alias_loadstring then
    is(loadstring, load, "alias loadstring")
else
    is(loadstring, nil, "no loadstring")
end

-- newproxy
if has_newproxy then
    local proxy = newproxy(false)
    type_ok(proxy, 'userdata', "function newproxy(false)")
    is(getmetatable(proxy), nil, "without metatable")
    proxy = newproxy(true)
    type_ok(proxy, 'userdata', "function newproxy(true)")
    type_ok(getmetatable(proxy), 'table', "with metatable")

    local proxy2 = newproxy(proxy)
    type_ok(proxy, 'userdata', "function newproxy(proxy)")
    is(getmetatable(proxy2), getmetatable(proxy))

    error_like(function () newproxy({}) end,
               "^[^:]+:%d+: bad argument #1 to 'newproxy' %(boolean or proxy expected%)",
               "function newproxy({})")
else
    is(newproxy, nil, "no newproxy")
end

do -- next
    local t = {'a','b','c'}
    local a = next(t, nil)
    is(a, 1, "function next (array)")
    a = next(t, 1)
    is(a, 2)
    a = next(t, 2)
    is(a, 3)
    a = next(t, 3)
    is(a, nil)

    error_like(function () a = next() end,
               "^[^:]+:%d+: bad argument #1 to 'next' %(table expected, got no value%)",
               "function next (no arg)")

    error_like(function () a = next(t, 6) end,
               "invalid key to 'next'",
               "function next (invalid key)")

    t = {'a','b','c'}
    a = next(t, 2)
    is(a, 3, "function next (unorderer)")
    a = next(t, 1)
    is(a, 2)
    a = next(t, 3)
    is(a, nil)

    t = {}
    a = next(t, nil)
    is(a, nil, "function next (empty table)")
end

do -- pairs
    local a = {'a','b','c'}
    local f, v, s = pairs(a)
    type_ok(f, 'function', "function pairs")
    type_ok(v, 'table')
    is(s, nil)
    s = f(v, s)
    is(s, 1)
    s = f(v, s)
    is(s, 2)
    s = f(v, s)
    is(s, 3)
    s = f(v, s)
    is(s, nil)
end

do -- pcall
    local status, result = pcall(assert, 1)
    is(status, true, "function pcall")
    is(result, 1)
    status, result = pcall(assert, false, 'catched')
    is(status, false)
    is(result, 'catched')
    status = pcall(assert)
    is(status, false)
end

do -- rawequal
    local t = {}
    local a = t
    is(rawequal(nil, nil), true, "function rawequal -> true")
    is(rawequal(false, false), true)
    is(rawequal(3, 3), true)
    is(rawequal('text', 'text'), true)
    is(rawequal(t, a), true)
    is(rawequal(print, print), true)

    is(rawequal(nil, 2), false, "function rawequal -> false")
    is(rawequal(false, true), false)
    is(rawequal(false, 2), false)
    is(rawequal(3, 2), false)
    is(rawequal(3, '2'), false)
    is(rawequal('text', '2'), false)
    is(rawequal('text', 2), false)
    is(rawequal(t, {}), false)
    is(rawequal(t, 2), false)
    is(rawequal(print, type), false)
    is(rawequal(print, 2), false)
end

-- rawlen
if has_rawlen then
    is(rawlen("text"), 4, "function rawlen (string)")
    is(rawlen({ 'a', 'b', 'c'}), 3, "function rawlen (table)")
    error_like(function () local a = rawlen(true) end,
               "^[^:]+:%d+: bad argument #1 to 'rawlen' %(table ",
               "function rawlen (bad arg)")
else
    is(rawlen, nil, "no rawlen")
end

do -- rawget
    local t = {a = 'letter a', b = 'letter b'}
    is(rawget(t, 'a'), 'letter a', "function rawget")
end

do -- rawset
    local t = {}
    is(rawset(t, 'a', 'letter a'), t, "function rawset")
    is(t.a, 'letter a')

    error_like(function () t = {}; rawset(t, nil, 42) end,
               "^table index is nil",
               "function rawset (table index is nil)")
end

do -- select
    is(select('#'), 0, "function select")
    is(select('#','a','b','c'), 3)
    eq_array({select(1,'a','b','c')}, {'a','b','c'})
    eq_array({select(3,'a','b','c')}, {'c'})
    eq_array({select(5,'a','b','c')}, {})
    eq_array({select(-1,'a','b','c')}, {'c'})
    eq_array({select(-2,'a','b','c')}, {'b', 'c'})
    eq_array({select(-3,'a','b','c')}, {'a', 'b', 'c'})

    error_like(function () select(0,'a','b','c') end,
               "^[^:]+:%d+: bad argument #1 to 'select' %(index out of range%)",
               "function select (out of range)")

    error_like(function () select(-4,'a','b','c') end,
               "^[^:]+:%d+: bad argument #1 to 'select' %(index out of range%)",
               "function select (out of range)")
end

-- setfenv
if has_getfenv then
    local t = {}
    local function f () end
    is(setfenv(f, t), f, "function setfenv")
    type_ok(getfenv(f), 'table')
    is(getfenv(f), t)

    save = getfenv(1)
    a = 1
    setfenv(1, {g = _G})
    g.is(a, nil, "function setfenv")
    g.is(g.a, 1)
    g.setfenv(1, g.save) -- restore

    save = getfenv(1)
    a = 1
    local newgt = {}        -- create new environment
    setmetatable(newgt, {__index = _G})
    setfenv(1, newgt)       -- set it
    is(a, 1, "function setfenv")
    a = 10
    is(a, 10)
    is(_G.a, 1)
    _G.a = 20
    is(_G.a, 20)
    setfenv(1, save) -- restore

    save = getfenv(1)
    local function factory ()
        return function ()
                   return a    -- "global" a
               end
    end
    a = 3
    local f1 = factory()
    local f2 = factory()
    is(f1(), 3, "function setfenv")
    is(f2(), 3)
    setfenv(f1, {a = 10})
    is(f1(), 10)
    is(f2(), 3)
    setfenv(1, save) -- restore

    is(setfenv(0, _G), nil, "function setfenv(0)")

    error_like(function () setfenv(-3, {}) end,
               "^[^:]+:%d+: bad argument #1 to 'setfenv' %(.-level.-%)",
               "function setfenv (negative)")

    error_like(function () setfenv(12, {}) end,
               "^[^:]+:%d+: bad argument #1 to 'setfenv' %(invalid level%)",
               "function setfenv (too depth)")

    t = {}
    error_like(function () setfenv(t, {}) end,
               "^[^:]+:%d+: bad argument #1 to 'setfenv' %(number expected, got table%)",
               "function setfenv (bad arg)")

    error_like(function () setfenv(print, {}) end,
               "^[^:]+:%d+: 'setfenv' cannot change environment of given object",
               "function setfenv (forbidden)")
else
    is(setfenv, nil, "no setfenv")
end

do -- type
    is(type("Hello world"), 'string', "function type")
    is(type(10.4*3), 'number')
    is(type(print), 'function')
    is(type(type), 'function')
    is(type(true), 'boolean')
    is(type(nil), 'nil')
    is(type(io.stdin), 'userdata')
    is(type(type(X)), 'string')

    local a = nil
    is(type(a), 'nil', "function type")
    a = 10
    is(type(a), 'number')
    a = "a string!!"
    is(type(a), 'string')
    a = print
    is(type(a), 'function')
    is(type(function () end), 'function')

    error_like(function () type() end,
               "^[^:]+:%d+: bad argument #1 to 'type' %(value expected%)",
               "function type (no arg)")
end

do -- tonumber
    is(tonumber('text12'), nil, "function tonumber")
    is(tonumber('12text'), nil)
    is(tonumber(3.14), 3.14)
    is(tonumber('3.14'), 3.14)
    is(tonumber('  3.14  '), 3.14)
    is(tonumber(tostring(111), 2), 7)
    is(tonumber('111', 2), 7)
    is(tonumber('  111  ', 2), 7)
    local a = {}
    is(tonumber(a), nil)

    error_like(function () tonumber() end,
               "^[^:]+:%d+: bad argument #1 to 'tonumber' %(value expected%)",
               "function tonumber (no arg)")

    error_like(function () tonumber('111', 200) end,
               "^[^:]+:%d+: bad argument #2 to 'tonumber' %(base out of range%)",
               "function tonumber (bad base)")
end

do -- tostring
    is(tostring('text'), 'text', "function tostring")
    is(tostring(3.14), '3.14')
    is(tostring(nil), 'nil')
    is(tostring(true), 'true')
    is(tostring(false), 'false')
    like(tostring({}), '^table: 0?[Xx]?%x+$')
    like(tostring(print), '^function: 0?[Xx]?[builtin]*#?%x+$')

    error_like(function () tostring() end,
               "^[^:]+:%d+: bad argument #1 to 'tostring' %(value expected%)",
               "function tostring (no arg)")
end

-- unpack
if has_unpack then
    eq_array({unpack({})}, {}, "function unpack")
    eq_array({unpack({'a'})}, {'a'})
    eq_array({unpack({'a','b','c'})}, {'a','b','c'})
    eq_array({(unpack({'a','b','c'}))}, {'a'})
    eq_array({unpack({'a','b','c','d','e'},2,4)}, {'b','c','d'})
    eq_array({unpack({'a','b','c'},2,4)}, {'b','c'})
elseif has_alias_unpack then
    is(unpack, table.unpack, "alias unpack")
else
    is(unpack, nil, "no unpack")
end

do -- xpcall
    local function err (obj)
        return obj
    end

    local function backtrace ()
        return 'not a back trace'
    end

    local status, result = xpcall(function () return assert(1) end, err)
    is(status, true, "function xpcall")
    is(result, 1)
    status, result = xpcall(function () return assert(false, 'catched') end, err)
    is(status, false)
    if jit then
        is(result, 'catched')
    else
        like(result, ':%d+: catched')
    end
    status, result = xpcall(function () return assert(false, 'catched') end, backtrace)
    is(status, false)
    is(result, 'not a back trace')

    if has_xpcall52 then
        status, result = xpcall(assert, err, 1)
        is(status, true, "function xpcall with args")
        is(result, 1)
        status, result = xpcall(assert, err, false, 'catched')
        is(status, false)
        is(result, 'catched')
        status, result = xpcall(assert, backtrace, false, 'catched')
        is(status, false)
        is(result, 'not a back trace')
    end

    error_like(function () xpcall(assert) end,
               "bad argument #2 to 'xpcall' %(.-",
               "function xpcall")

    if has_xpcall53 then
        error_like(function () xpcall(assert, 1) end,
                   "bad argument #2 to 'xpcall' %(function expected, got number%)",
                  "function xpcall")
    else
        is(xpcall(assert, nil), false, "function xpcall")
    end
end

if jit and pcall(require, 'ffi') then
    dofile'lexicojit/basic.t'
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
