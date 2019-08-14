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

=head1 Lua expression

=head2 Synopsis

    % prove 202-expr.t

=head2 Description

See section "Expressions" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#2.5>,
L<https://www.lua.org/manual/5.2/manual.html#3.4>,
L<https://www.lua.org/manual/5.3/manual.html#3.4>

=cut

--]]

require'tap'
local profile = require'profile'
local nocvtn2s = profile.nocvtn2s
local nocvts2n = profile.nocvts2n

plan'no_plan'

local x = math.pi
is(tostring(x - x%0.0001), tostring(3.1415), "modulo")

local a = {}; a.x = 1; a.y = 0;
local b = {}; b.x = 1; b.y = 0;
local c = a
is(a == c, true, "relational op (by reference)")
is(a ~= b, true)

is('0' == 0, false, "relational op")
is(2 < 15, true)
is('2' < '15', false)

error_like(function () return 2 < '15' end,
           "compare",
           "relational op")

error_like(function () return '2' < 15 end,
           "compare",
           "relational op")

is(4 and 5, 5, "logical op")
is(nil and 13, nil)
is(false and 13, false)
is(4 or 5, 4)
is(false or 5, 5)
is(false or 'text', 'text')

is(10 or 20, 10, "logical op")
is(10 or error(), 10)
is(nil or 'a', 'a')
is(nil and 10, nil)
is(false and error(), false)
is(false and nil, false)
is(false or nil, nil)
is(10 and 20, 20)

is(not nil, true, "logical not")
is(not false, true)
is(not 0, false)
is(not not nil, false)
is(not 'text', false)
a = {}
is(not a, false)
is(not (a == a), false)
is(not (a ~= a), true)

is("Hello " .. "World", "Hello World", "concatenation")
if not nocvtn2s then
    is(0 .. 1, '01')
end
a = "Hello"
is(a .. " World", "Hello World")
is(a, "Hello")

if not nocvts2n then
    is('10' + 1, 11, "coercion")
    is('-5.3' * '2', -10.6)
end
is(tostring(10), '10')
if not nocvtn2s then
    is(10 .. 20, '1020')
    is(10 .. '', '10')
end

error_like(function () return 'hello' + 1 end,
           _VERSION >= 'Lua 5.4' and "attempt to add" or "perform arithmetic",
           "no coercion")

error_like(function ()
                local function first() return end
                local function limit() return 2 end
                local function step()  return 1 end
                for i = first(), limit(), step() do
                    print(i)
                end
           end,
           "^[^:]+:%d+:.- 'for' initial value",
           "for tonumber")

error_like(function ()
                local function first() return 1 end
                local function limit() return end
                local function step()  return 2 end
                for i = first(), limit(), step() do
                    print(i)
                end
           end,
           "^[^:]+:%d+:.- 'for' limit",
           "for tonumber")

error_like(function ()
                local function first() return 1 end
                local function limit() return 2 end
                local function step()  return end
                for i = first(), limit(), step() do
                    print(i)
                end
           end,
           "^[^:]+:%d+:.- 'for' step",
           "for tonumber")

if _VERSION >= 'Lua 5.4' then
    error_like(function ()
                    for i = 1, 10, 0 do
                        print(i)
                    end
               end,
               "^[^:]+:%d+: 'for' step is zero",
               "for step zero")
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
