--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2009-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

local profile = require'profile'

if profile.nocvts2n or _VERSION >= 'Lua 5.4' then
    error_like(function () return ~'4' end,
               "^[^:]+:%d+: attempt to",
               "~'4'")
else
    is(~'4', -5, "~'4'")
end

error_like(function () return ~'text' end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a string value",
           "~'text'")

if profile.nocvts2n and _VERSION == 'Lua 5.3' then
    error_like(function () return '25.5' // 3.5 end,
               "^[^:]+:%d+: attempt to",
               "'25.5' // 3.5")

    error_like(function () return '25' // 3 end,
               "^[^:]+:%d+: attempt to",
               "'25' // 3")
else
    is('25.5' // 3.5, 7.0, "'25.5' // 3.5")

    is('25' // 3, 8, "'25' // 3")
end

if profile.nocvts2n or _VERSION >= 'Lua 5.4' then
    error_like(function () return '3' & 7 end,
               "^[^:]+:%d+: attempt to",
               "'3' & 7")

    error_like(function () return '4' | 1 end,
               "^[^:]+:%d+: attempt to",
               "'4' | 1")

    error_like(function () return '7' ~ 1 end,
               "^[^:]+:%d+: attempt to",
               "'7' ~ 1")

    error_like(function () return '100' >> 5 end,
               "^[^:]+:%d+: attempt to",
               "'100' >> 5")

    error_like(function () return '3' << 2 end,
               "^[^:]+:%d+: attempt to",
               "'3' << 2")
else
    is('3' & 7, 3, "'3' & 7")

    is('4' | 1, 5, "'4' | 1")

    is('7' ~ 1, 6, "'7' ~ 1")

    is('100' >> 5, 3, "'100' >> 5")

    is('3' << 2, 12, "'3' << 2")
end

error_like(function () return '25' // {} end,
           "^[^:]+:%d+: attempt to",
           "'25' // {}")

error_like(function () return '3' & true end,
           "^[^:]+:%d+: attempt to",
           "'3' & true")

error_like(function () return '4' | true end,
           "^[^:]+:%d+: attempt to",
           "'4' | true")

error_like(function () return '7' ~ true end,
           "^[^:]+:%d+: attempt to",
           "'7' ~ true")

error_like(function () return '100' >> true end,
           "^[^:]+:%d+: attempt to",
           "'100' >> true")

error_like(function () return '3' << true end,
           "^[^:]+:%d+: attempt to",
           "'3' << true")

error_like(function () return '25' // 'text' end,
           "^[^:]+:%d+: attempt to",
           "'25' // 'text'")

error_like(function () return '3' & 'text' end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a string value",
           "'3' & 'text'")

error_like(function () return '4' | 'text' end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a string value",
           "'4' | 'text'")

error_like(function () return '7' ~ 'text' end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a string value",
           "'7' ~ 'text'")

error_like(function () return '100' >> 'text' end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a string value",
           "'100' >> 'text'")

error_like(function () return '3' << 'text' end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a string value",
           "'3' << 'text'")

if profile.nocvts2n and _VERSION == 'Lua 5.3' then
    error_like(function () return '25.5' // '3.5' end,
               "^[^:]+:%d+: attempt to",
               "'25.5' // '3.5'")

    error_like(function () return '25' // '3' end,
               "^[^:]+:%d+: attempt to",
               "'25' // '3'")
else
    is('25.5' // '3.5', 7.0, "'25.5' // '3.5'")

     is('25' // '3', 8, "'25' // '3'")
end

if profile.nocvts2n or _VERSION >= 'Lua 5.4' then
    error_like(function () return '3' & '7' end,
               "^[^:]+:%d+: attempt to",
               "'3' & '7'")

    error_like(function () return '4' | '1' end,
               "^[^:]+:%d+: attempt to",
               "'4' | '1'")

    error_like(function () return '7' ~ '1' end,
               "^[^:]+:%d+: attempt to",
               "'7' ~ '1'")

    error_like(function () return '100' >> '5' end,
               "^[^:]+:%d+: attempt to",
               "'100' >> '5'")

    error_like(function () return '3' << '2' end,
               "^[^:]+:%d+: attempt to",
               "'3' << '2'")
else
    is('3' & '7', 3, "'3' & '7'")

    is('4' | '1', 5, "'4' | '1'")

    is('7' ~ '1', 6, "'7' ~ '1'")

    is('100' >> '5', 3, "'100' >> '5'")

    is('3' << '2', 12, "'3' << 2")
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
