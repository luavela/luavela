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

=head1 Lua Mathematic Library

=head2 Synopsis

    % prove 307-math.t

=head2 Description

Tests Lua Mathematic Library

See section "Mathematical Functions" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#5.6>,
L<https://www.lua.org/manual/5.2/manual.html#6.6>,
L<https://www.lua.org/manual/5.3/manual.html#6.7>

=cut

--]]

require'tap'
local profile = require'profile'
local has_integer = _VERSION >= 'Lua 5.3'
local has_mathx = _VERSION < 'Lua 5.3' or profile.compat52 or profile.compat53 or profile.has_mathx
local has_log10 = _VERSION < 'Lua 5.2' or profile.compat51 or profile.has_math_log10 or
                  profile.compat52 or profile.compat53 or profile.has_mathx
local has_log_with_base = _VERSION >= 'Lua 5.2' or profile.compat52
local has_mod = true -- UJIT: we still support math.mod (TODO: remove alias)
local nocvts2n = profile.nocvts2n

plan'no_plan'

do -- abs
    is(math.abs(-12.34), 12.34, "function abs (float)")
    is(math.abs(12.34), 12.34)
    if math.type then
        is(math.type(math.abs(-12.34)), 'float')
    end
    is(math.abs(-12), 12, "function abs (integer)")
    is(math.abs(12), 12)
    if math.type then
        is(math.type(math.abs(-12)), 'integer')
    end
end

do -- acos
    like(math.acos(0.5), '^1%.047', "function acos")
end

do -- asin
    like(math.asin(0.5), '^0%.523', "function asin")
end

do -- atan
    like(math.atan(0.5), '^0%.463', "function atan")
end

-- atan2
if has_mathx then
    like(math.atan2(1.0, 2.0), '^0%.463', "function atan2")
else
    is(math.atan2, nil, "function atan2 (removed)")
end

do -- ceil
    is(math.ceil(12.34), 13, "function ceil")
    is(math.ceil(-12.34), -12)
    is(math.ceil(-12), -12)
    if math.type then
        is(math.type(math.ceil(-12.34)), 'integer')
    end
end

do -- cos
    like(math.cos(1.0), '^0%.540', "function cos")
end

-- cosh
if has_mathx then
    like(math.cosh(1.0), '^1%.543', "function cosh")
else
    is(math.cosh, nil, "function cosh (removed)")
end

do -- deg
    is(math.deg(math.pi), 180, "function deg")
end

do -- exp
    like(math.exp(1.0), '^2%.718', "function exp")
end

do -- floor
    is(math.floor(12.34), 12, "function floor")
    is(math.floor(-12.34), -13)
    is(math.floor(-12), -12)
    if math.type then
        is(math.type(math.floor(-12.34)), 'integer')
    end
end

do -- fmod
    like(math.fmod(7.0001, 0.3), '^0%.100', "function fmod (float)")
    like(math.fmod(-7.0001, 0.3), '^-0%.100')
    like(math.fmod(-7.0001, -0.3), '^-0%.100')
    if math.type then
        is(math.type(math.fmod(7.0, 0.3)), 'float')
    end
    is(math.fmod(7, 3), 1, "function fmod (integer)")
    is(math.fmod(-7, 3), -1)
    is(math.fmod(-7, -1), 0)
    if math.type then
        is(math.type(math.fmod(7, 3)), 'integer')
    end
    if _VERSION >= 'Lua 5.3' then
        error_like(function () math.fmod(7, 0) end,
                   "^[^:]+:%d+: bad argument #2 to 'fmod' %(zero%)",
                   "function fmod 0")
    else
        diag"fmod by zero -> nan"
    end
end

-- frexp
if has_mathx then
    eq_array({math.frexp(1.5)}, {0.75, 1}, "function frexp")
else
    is(math.frexp, nil, "function frexp (removed)")
end

do -- huge
    type_ok(math.huge, 'number', "variable huge")
    if math.type then
        is(math.type(math.huge), 'float')
    end
end

-- ldexp
if has_mathx then
    is(math.ldexp(1.2, 3), 9.6, "function ldexp")
else
    is(math.ldexp, nil, "function ldexp (removed)")
end

do -- log
    like(math.log(47), '^3%.85', "function log")
    if has_log_with_base then
        like(math.log(47, math.exp(1)), '^3%.85', "function log (base e)")
        like(math.log(47, 2), '^5%.554', "function log (base 2)")
        like(math.log(47, 10), '^1%.672', "function log (base 10)")
    end
end

-- log10
if has_log10 then
    like(math.log10(47.0), '^1%.672', "function log10")
else
    is(math.log10, nil, "function log10 (removed)")
end

do --max
    is(math.max(1), 1, "function max")
    is(math.max(1, 2), 2)
    is(math.max(1, 2, 3, -4), 3)

    error_like(function () math.max() end,
               "^[^:]+:%d+: bad argument #1 to 'max' %(.- expected",
               "function max 0")
end

-- maxinteger
if has_integer then
    type_ok(math.maxinteger, 'number', "variable maxinteger")
    if math.type then
        is(math.type(math.maxinteger), 'integer')
    end
else
    is(math.maxinteger, nil, "no maxinteger")
end

do --min
    is(math.min(1), 1, "function min")
    is(math.min(1, 2), 1)
    is(math.min(1, 2, 3, -4), -4)

    error_like(function () math.min() end,
               "^[^:]+:%d+: bad argument #1 to 'min' %(.- expected",
               "function min 0")
end

-- mininteger
if has_integer then
    type_ok(math.mininteger, 'number', "variable mininteger")
    if math.type then
        is(math.type(math.mininteger), 'integer')
    end
else
    is(math.mininteger, nil, "no mininteger")
end

-- mod (compat50)
if has_mod then
    is(math.mod, math.fmod, "function mod (alias fmod)")
else
    is(math.mod, nil, "function mod (alias removed)")
end

do -- modf
    eq_array({math.modf(2.25)}, {2, 0.25}, "function modf")
    eq_array({math.modf(2)}, {2, 0.0})
end

do -- pi
    like(tostring(math.pi), '^3%.14', "variable pi")
end

-- pow
if has_mathx then
    is(math.pow(-2, 3), -8, "function pow")
else
    is(math.pow, nil, "function pow (removed)")
end

do -- rad
    like(math.rad(180), '^3%.14', "function rad")
end

do -- random
    like(math.random(), '^0%.%d+', "function random no arg")
    if math.type then
        is(math.type(math.random()), 'float')
    end
    like(math.random(9), '^%d$', "function random 1 arg")
    if math.type then
        is(math.type(math.random(9)), 'integer')
    end
    like(math.random(10, 19), '^1%d$', "function random 2 arg")
    if math.type then
        is(math.type(math.random(10, 19)), 'integer')
    end
    like(math.random(-19, -10), '^-1%d$', "function random 2 arg")

    if _VERSION >= 'Lua 5.4' then
        like(math.random(0), '^%-?%d+$', "function random 0")
    else
        if jit then
            todo("LuaJIT intentional. Don't check empty interval.")
        end
        -- UJIT: random(0) works
        -- error_like(function () math.random(0) end,
        --           "^[^:]+:%d+: bad argument #1 to 'random' %(interval is empty%)",
        --          "function random empty interval")
    end

    if jit then
        todo("LuaJIT intentional. Don't check empty interval.", 2)
    end
    error_like(function () math.random(-9) end,
               "^[^:]+:%d+: bad argument #%d to 'random' %(interval is empty%)",
               "function random empty interval")

    error_like(function () math.random(19, 10) end,
               "^[^:]+:%d+: bad argument #%d to 'random' %(interval is empty%)",
               "function random empty interval")

    if jit then
        todo("LuaJIT intentional. Don't care about extra arguments.")
    end
    error_like(function () math.random(1, 2, 3) end,
               "^[^:]+:%d+: wrong number of arguments",
               "function random too many arg")
end

do -- randomseed
    math.randomseed(42)
    local a = math.random()
    math.randomseed(42)
    local b = math.random()
    is(a, b, "function randomseed")
end

do -- sin
    like(math.sin(1.0), '^0%.841', "function sin")
end

-- sinh
if has_mathx then
    like(math.sinh(1), '^1%.175', "function sinh")
else
    is(math.sinh, nil, "function sinh (removed)")
end

do -- sqrt
    like(math.sqrt(2), '^1%.414', "function sqrt")
end

do -- tan
    like(math.tan(1.0), '^1%.557', "function tan")
end

-- tanh
if has_mathx then
    like(math.tanh(1), '^0%.761', "function tanh")
else
    is(math.tanh, nil, "function tanh (removed)")
end

-- tointeger
if has_integer then
    is(math.tointeger(-12), -12, "function tointeger (number)")
    is(math.tointeger(-12.0), -12)
    is(math.tointeger(-12.34), nil)
    if nocvts2n then
        is(math.tointeger('-12'), nil, "function tointeger (string)")
        is(math.tointeger('-12.0'), nil)
    else
        is(math.tointeger('-12'), -12, "function tointeger (string)")
        is(math.tointeger('-12.0'), -12)
    end
    is(math.tointeger('-12.34'), nil)
    is(math.tointeger('bad'), nil)
    is(math.tointeger(true), nil, "function tointeger (boolean)")
    is(math.tointeger({}), nil, "function tointeger (table)")
else
    is(math.tointeger, nil, "no math.tointeger")
end

-- type
if has_integer then
    is(math.type(3), 'integer', "function type")
    is(math.type(3.14), 'float')
    is(math.type('3.14'), nil)
else
    is(math.type, nil, "no math.type")
end

-- ult
if has_integer then
    is(math.ult(2, 3), true, "function ult")
    is(math.ult(2, 2), false)
    is(math.ult(2, 1), false)
else
    is(math.ult, nil, "no math.ult")
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
