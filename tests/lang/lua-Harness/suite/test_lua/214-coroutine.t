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

=head1 Lua coroutines

=head2 Synopsis

    % prove 214-coroutine.t

=head2 Description

See section "Coroutines" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#2.11>,
L<https://www.lua.org/manual/5.2/manual.html#2.6>,
L<https://www.lua.org/manual/5.3/manual.html#2.6>

See section "Coroutines" in "Programming in Lua".

=cut

--]]

require'tap'
local profile = require'profile'
local has_coroutine52 = _VERSION >= 'Lua 5.2' or jit
local has_running52 = _VERSION >= 'Lua 5.2' or (profile.luajit_compat52 and not ujit)
local has_isyieldable = _VERSION >= 'Lua 5.3' or (jit and jit.version_num >= 20100)
local has_close = _VERSION >= 'Lua 5.4'

plan'no_plan'

do
    local output = {}

    local function foo1 (a)
        output[#output+1] = "foo " .. tostring(a)
        return coroutine.yield(2*a)
    end

    local co = coroutine.create(function (a,b)
        local r, s
        output[#output+1] = "co-body " .. tostring(a) .." " .. tostring(b)
        r = foo1(a+1)
        output[#output+1] = "co-body " .. r
        r, s = coroutine.yield(a+b, a-b)
        output[#output+1] = "co-body " .. r .. " " .. s
        return b, 'end'
    end)

    eq_array({coroutine.resume(co, 1, 10)}, {true, 4}, "foo1")
    eq_array({coroutine.resume(co, 'r')}, {true, 11, -9})
    eq_array({coroutine.resume(co, "x", "y")}, {true, 10, 'end'})
    eq_array({coroutine.resume(co, "x", "y")}, {false, "cannot resume dead coroutine"})
    eq_array(output, {
        'co-body 1 10',
        'foo 2',
        'co-body r',
        'co-body x y',
    })
end

do
    local output = ''
    local co = coroutine.create(function ()
        output = 'hi'
    end)
    like(co, '^thread: 0?[Xx]?%x+$', "basics")

    is(coroutine.status(co), 'suspended')
    coroutine.resume(co)
    is(output, 'hi')
    is(coroutine.status(co), 'dead')

    error_like(function () coroutine.create(true) end,
               "^[^:]+:%d+: bad argument #1 to 'create' %(.- expected")

    error_like(function () coroutine.resume(true) end,
               "^[^:]+:%d+: bad argument #1 to 'resume' %(.- expected")

    error_like(function () coroutine.status(true) end,
               "^[^:]+:%d+: bad argument #1 to 'status' %(.- expected")
end

do
    local output = {}
    local co = coroutine.create(function ()
        for i=1,10 do
            output[#output+1] = i
            coroutine.yield()
        end
    end)

    coroutine.resume(co)
    if has_running52 then
        local thr, ismain = coroutine.running()
        type_ok(thr, 'thread', "running")
        is(ismain, true, "running")
    else
        local thr = coroutine.running()
        is(thr, nil, "main thread")
    end
    is(coroutine.status(co), 'suspended', "basics")
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    eq_array({coroutine.resume(co)}, {false, 'cannot resume dead coroutine'})
    eq_array(output, {1,2,3,4,5,6,7,8,9,10})
end

do
    local co = coroutine.create(function (a,b)
        coroutine.yield(a + b, a - b)
    end)

    eq_array({coroutine.resume(co, 20, 10)}, {true, 30, 10}, "basics")
end

do
    local co = coroutine.create(function ()
        return 6, 7
    end)

    eq_array({coroutine.resume(co)}, {true, 6, 7}, "basics")
end

if has_coroutine52 then
    local co = coroutine.wrap(function(...)
        return pcall(function(...)
            return coroutine.yield(...)
        end, ...)
    end)
    eq_array({co("Hello")}, {"Hello"})
    eq_array({co("World")}, {true, "World"})
end

if has_coroutine52 then
    local co = coroutine.wrap(function(...)
        local function backtrace ()
            return 'not a back trace'
        end
        return xpcall(function(...)
            return coroutine.yield(...)
        end, backtrace, ...)
    end)
    eq_array({co("Hello")}, {"Hello"})
    eq_array({co("World")}, {true, "World"})
end

if has_coroutine52 then
    local output = {}
    local co = coroutine.wrap(function()
        while true do
            local t = setmetatable({}, {
                __eq = function(...)
                    return coroutine.yield(...)
                end}
            )
            local t2 = setmetatable({}, getmetatable(t))
            output[#output+1] = t == t2
        end
    end)
    co()
    co(true)
    co(false)
    eq_array(output, {true, false})
end

if has_coroutine52 then
    local co = coroutine.wrap(print)
    type_ok(co, 'function')

    error_like(function () coroutine.wrap(true) end,
               "^[^:]+:%d+: bad argument #1 to 'wrap' %(function expected, got boolean%)")

    co = coroutine.wrap(function () error"in coro" end)
    error_like(function () co() end,
               "^[^:]+:%d+: [^:]+:%d+: in coro$")
end

do
    local co = coroutine.create(function ()
        error "in coro"
    end)
    local r, msg = coroutine.resume(co)
    is(r, false)
    like(msg, "^[^:]+:%d+: in coro$")
end

do
    error_like(function () coroutine.yield() end,
               "attempt to yield")

    if has_isyieldable then
        is(coroutine.isyieldable(), false, "isyieldable")
    else
        is(coroutine.isyieldable, nil, "no coroutine.isyieldable")
    end
end

-- close
if has_close then
    local output = ''
    local co = coroutine.create(function ()
        output = 'hi'
    end)
    is(coroutine.close(co), true, "close")
    is(coroutine.status(co), 'dead')
    is(coroutine.close(co), true, "close again")

    error_like(function () coroutine.close(coroutine.running()) end,
               "^[^:]+:%d+: cannot close a running coroutine")

    error_like(function () coroutine.close(42) end,
               "^[^:]+:%d+: bad argument #1 to 'close' %(thread expected, got number%)")
else
    is(coroutine.close, nil, "no coroutine.close")
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
