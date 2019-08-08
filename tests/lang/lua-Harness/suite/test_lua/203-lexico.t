#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2010-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 Lua Lexicography

=head2 Synopsis

    % prove 203-lexico.t

=head2 Description

See "Lua 5.3 Reference Manual", section 3.1 "Lexical Conventions",
L<http://www.lua.org/manual/5.3/manual.html#3.1>.

See section "Lexical Conventions"
L<https://www.lua.org/manual/5.1/manual.html#2.1>,
L<https://www.lua.org/manual/5.2/manual.html#3.1>,
L<https://www.lua.org/manual/5.3/manual.html#3.1>

=cut

--]]

require'tap'
local loadstring = loadstring or load

plan'no_plan'

is("\65", "A")
is("\065", "A")

is(string.byte("\a"), 7)
is(string.byte("\b"), 8)
is(string.byte("\f"), 12)
is(string.byte("\n"), 10)
is(string.byte("\r"), 13)
is(string.byte("\t"), 9)
is(string.byte("\v"), 11)
is(string.byte("\\"), 92)

is(string.len("A\0B"), 3)

do
    local f, msg = loadstring [[a = "A\300"]]
    if _VERSION == 'Lua 5.1' then
        like(msg, "^[^:]+:%d+: .- near")
    else
        like(msg, "^[^:]+:%d+: .- escape .- near")
    end

    f, msg = loadstring [[a = " unfinished string ]]
    like(msg, "^[^:]+:%d+: unfinished string near")

    f, msg = loadstring [[a = " unfinished string
]]
    like(msg, "^[^:]+:%d+: unfinished string near")

    f, msg = loadstring [[a = " unfinished string \
]]
    like(msg, "^[^:]+:%d+: unfinished string near")

    f, msg = loadstring [[a = " unfinished string \]]
    like(msg, "^[^:]+:%d+: unfinished string near")

    f, msg = loadstring "a = [[ unfinished long string "
    like(msg, "^[^:]+:%d+: unfinished long string .-near")

    f, msg = loadstring "a = [== invalid long string delimiter "
    like(msg, "^[^:]+:%d+: invalid long string delimiter near")
end

do
    local a = 'alo\n123"'
    is('alo\n123"', a)
    is("alo\n123\"", a)
    is('\97lo\10\04923"', a)
    is([[alo
123"]], a)
    is([==[
alo
123"]==], a)
end

is(3.0, 3)
is(314.16e-2, 3.1416)
is(0.31416E1, 3.1416)
is(.3, 0.3)
is(0xff, 255)
is(0x56, 86)

do
    local f, msg = loadstring [[a = 12e34e56]]
    like(msg, "^[^:]+:%d+: malformed number near")
end

--[===[
--[[
--[=[
    nested long comments
--]=]
--]]
--]===]

do
    local f, msg = loadstring "  --[[ unfinished long comment "
    like(msg, "^[^:]+:%d+: unfinished long comment .-near")
end

if _VERSION >= 'Lua 5.2' or jit then
    dofile'lexico52/lexico.t'
end

if _VERSION >= 'Lua 5.3' or (jit and jit.version_num >= 20100) then
    dofile'lexico53/lexico.t'
end

if jit and pcall(require, 'ffi') then
    dofile'lexicojit/lexico.t'
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
