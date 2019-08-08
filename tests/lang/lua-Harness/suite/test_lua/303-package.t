#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2009-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 Lua Package Library

=head2 Synopsis

    % prove 303-package.t

=head2 Description

Tests Lua Package Library

See section "Modules" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#5.3>,
L<https://www.lua.org/manual/5.2/manual.html#6.3>,
L<https://www.lua.org/manual/5.3/manual.html#6.3>

=cut

--]]

require'tap'
local profile = require'profile'
local has_loaders = _VERSION == 'Lua 5.1'
local has_alias_loaders = profile.compat51
local has_loadlib52 = _VERSION >= 'Lua 5.2' or jit
local has_module = _VERSION == 'Lua 5.1' or profile.compat51
local has_searchers = _VERSION >= 'Lua 5.2'
local has_alias_searchers = jit and jit.version_num >= 20100 and profile.luajit_compat52
local has_searcherpath = _VERSION >= 'Lua 5.2' or jit

plan'no_plan'


type_ok(package.config, 'string')

type_ok(package.cpath, 'string')

type_ok(package.path, 'string')

if has_loaders then
    type_ok(package.loaders, 'table', "table package.loaders")
elseif has_alias_loaders then
    is(package.loaders, package.searchers, "alias package.loaders")
else
    is(package.loaders, nil, "no package.loaders")
end

if has_searchers then
    type_ok(package.searchers, 'table', "table package.searchers")
elseif has_alias_searchers then
    is(package.searchers, package.loaders, "alias package.searchers")
else
    is(package.searchers, nil, "no package.searchers")
end

do -- loaded
    ok(package.loaded._G, "table package.loaded")
    ok(package.loaded.coroutine)
    ok(package.loaded.io)
    ok(package.loaded.math)
    ok(package.loaded.os)
    ok(package.loaded.package)
    ok(package.loaded.string)
    ok(package.loaded.table)

    local m = require'os'
    is(m, package.loaded['os'])
end

do -- preload
    type_ok(package.preload, 'table', "table package.preload")
    is(# package.preload, 0)

    local foo = {}
    foo.bar = 1234
    local function foo_loader ()
       return foo
    end
    package.preload.foo = foo_loader
    local m = require 'foo'
    assert(m == foo)
    is(m.bar, 1234, "function require & package.preload")
end

do -- loadlib
    local path_lpeg = package.searchpath and package.searchpath('lpeg', package.cpath)

    local f, msg = package.loadlib('libbar', 'baz')
    is(f, nil, "loadlib")
    type_ok(msg, 'string')

    if path_lpeg then
        f, msg = package.loadlib(path_lpeg, 'baz')
        is(f, nil, "loadlib")
        like(msg, 'undefined symbol')

        f = package.loadlib(path_lpeg, 'luaopen_lpeg')
        type_ok(f, 'function', "loadlib ok")
    else
        skip("no lpeg path")
    end

    if has_loadlib52 then
        f, msg = package.loadlib('libbar', '*')
        is(f, nil, "loadlib '*'")
        type_ok(msg, 'string')

        if path_lpeg then
            f = package.loadlib(path_lpeg, '*')
            is(f, true, "loadlib '*'")
        else
            skip("no lpeg path")
        end
    end
end

-- searchpath
if has_searcherpath then
    local p = package.searchpath('tap', package.path)
    type_ok(p, 'string', "searchpath")
    p = package.searchpath('tap', 'bad path')
    is(p, nil)
else
    is(package.searchpath, nil, "no package.searchpath")
end

do -- require
    local f = io.open('complex.lua', 'w')
    f:write [[
complex = {}

function complex.new (r, i) return {r=r, i=i} end

--defines a constant 'i'
complex.i = complex.new(0, 1)

function complex.add (c1, c2)
    return complex.new(c1.r + c2.r, c1.i + c2.i)
end

function complex.sub (c1, c2)
    return complex.new(c1.r - c2.r, c1.i - c2.i)
end

function complex.mul (c1, c2)
    return complex.new(c1.r*c2.r - c1.i*c2.i,
                       c1.r*c2.i + c1.i*c2.r)
end

local function inv (c)
    local n = c.r^2 + c.i^2
    return complex.new(c.r/n, -c.i/n)
end

function complex.div (c1, c2)
    return complex.mul(c1, inv(c2))
end

return complex
]]
    f:close()
    local m = require 'complex'
    is(m, complex, "function require")
    is(complex.i.r, 0)
    is(complex.i.i, 1)
    os.remove('complex.lua') -- clean up

    error_like(function () require('no_module') end,
               "^[^:]+:%d+: module 'no_module' not found:",
               "function require (no module)")

    f = io.open('syntax.lua', 'w')
    f:write [[?syntax error?]]
    f:close()
    error_like(function () require('syntax') end,
               "^error loading module 'syntax' from file '%.[/\\]syntax%.lua':",
               "function require (syntax error)")
    os.remove('syntax.lua') -- clean up

    f = io.open('bar.lua', 'w')
    f:write [[
    print("    in bar.lua", ...)
    a = ...
]]
    f:close()
    a = nil
    require 'bar'
    is(a, 'bar', "function require (arg)")
    os.remove('bar.lua') -- clean up

    f = io.open('cplx.lua', 'w')
    f:write [[
-- print('cplx.lua', ...)
local _G = _G
_ENV = nil
local cplx = {}

local function new (r, i) return {r=r, i=i} end
cplx.new = new

--defines a constant 'i'
cplx.i = new(0, 1)

function cplx.add (c1, c2)
    return new(c1.r + c2.r, c1.i + c2.i)
end

function cplx.sub (c1, c2)
    return new(c1.r - c2.r, c1.i - c2.i)
end

function cplx.mul (c1, c2)
    return new(c1.r*c2.r - c1.i*c2.i,
               c1.r*c2.i + c1.i*c2.r)
end

local function inv (c)
    local n = c.r^2 + c.i^2
    return new(c.r/n, -c.i/n)
end

function cplx.div (c1, c2)
    return mul(c1, inv(c2))
end

_G.cplx = cplx
return cplx
]]
    f:close()
    require 'cplx'
    is(cplx.i.r, 0, "function require & module")
    is(cplx.i.i, 1)
    os.remove('cplx.lua') -- clean up
end

-- module & seeall
local done_testing = done_testing
if has_module then
    m = {}
    package.seeall(m)
    m.pass("function package.seeall")

    is(mod, nil, "function module & seeall")
    module('mod', package.seeall)
    type_ok(mod, 'table')
    is(mod, package.loaded.mod)

    is(modz, nil, "function module")
    local _G = _G
    module('modz')
    _G.type_ok(_G.modz, 'table')
    _G.is(_G.modz, _G.package.loaded.modz)
else
    is(package.seeall, nil, "package.seeall (removed)")
    is(module, nil, "module (removed)")
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
