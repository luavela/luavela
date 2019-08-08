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

=head1 Lua Grammar

=head2 Synopsis

    % prove 204-grammar.t

=head2 Description

See section "The Complete Syntax of Lua" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#8>,
L<https://www.lua.org/manual/5.2/manual.html#9>,
L<https://www.lua.org/manual/5.3/manual.html#9>

=cut

--]]

require'tap'
local profile = require'profile'
local has_goto = _VERSION >= 'Lua 5.2' or jit
local loadstring = loadstring or load

plan'no_plan'

do --[[ empty statement ]]
    local f, msg = loadstring [[; a = 1]]
    if _VERSION == 'Lua 5.1' and not profile.luajit_compat52 then
        like(msg, "^[^:]+:%d+: unexpected symbol near ';'", "empty statement")
    else
        type_ok(f, 'function', "empty statement")
    end

    f = loadstring [[a = 1; a = 2]]
    type_ok(f, 'function')

    f, msg = loadstring [[a = 1;;; a = 2]]
    if _VERSION == 'Lua 5.1' and not profile.luajit_compat52 then
        like(msg, "^[^:]+:%d+: unexpected symbol near ';'")
    else
        type_ok(f, 'function')
    end
end

do --[[ orphan break ]]
    local f, msg = loadstring [[
function f()
    print "before"
    do
        print "inner"
        break
    end
    print "after"
end
]]
    if _VERSION == 'Lua 5.1' or _VERSION >= 'Lua 5.4' then
        like(msg, "^[^:]+:%d+: no loop to break", "orphan break")
    else
        like(msg, "^[^:]+:%d+: <break> at line 5 not inside a loop", "orphan break")
    end
end

do --[[ break anywhere ]]
    local f, msg = loadstring [[
function f()
    print "before"
    while true do
        print "inner"
        break
        print "break"
    end
    print "after"
end
]]
    if _VERSION == 'Lua 5.1' and not profile.luajit_compat52 then
        like(msg, "^[^:]+:%d+: 'end' expected %(to close 'while' at line 3%) near 'print'", "break anywhere")
    else
        type_ok(f, 'function', "break anywhere")
    end

    f, msg = loadstring [[
function f()
    print "before"
    while true do
        print "inner"
        if cond then
            break
            print "break"
        end
    end
    print "after"
end
]]
    if _VERSION == 'Lua 5.1' and not profile.luajit_compat52 then
        like(msg, "^[^:]+:%d+: 'end' expected %(to close 'if' at line 5%) near 'print'", "break anywhere")
    else
        type_ok(f, 'function', "break anywhere")
    end
end

--[[ goto ]]
if has_goto then
    local f, msg = loadstring [[
::label::
    goto unknown
]]
    if jit then
        like(msg, ":%d+: undefined label 'unknown'", "unknown goto")
    else
        like(msg, ":%d+: no visible label 'unknown' for <goto> at line %d+", "unknown goto")
    end

    f, msg = loadstring [[
::label::
    goto label
::label::
]]
    if jit then
        like(msg, ":%d+: duplicate label 'label'", "duplicate label")
    else
        like(msg, ":%d+: label 'label' already defined on line %d+", "duplicate label")
    end

    f, msg = loadstring [[
::e::
    goto f
    local x
::f::
    goto e
]]
    if jit then
        like(msg, ":%d+: <goto f> jumps into the scope of local 'x'", "bad goto")
    else
        like(msg, ":%d+: <goto f> at line %d+ jumps into the scope of local 'x'", "bad goto")
    end

    f= loadstring [[
do
::s1:: ;
    goto s2

::s2::
    goto s3

::s3::
end
]]
    type_ok(f, 'function', "goto")
else
    diag("no goto")
end

do --[[ syntax error ]]
    local f, msg = loadstring [[a = { 1, 2, 3)]]
    like(msg, ":%d+: '}' expected near '%)'", "constructor { }")

    f, msg = loadstring [[a = (1 + 2}]]
    like(msg, ":%d+: '%)' expected near '}'", "expr ( )")

    f, msg = loadstring [[a = f(1, 2}]]
    like(msg, ":%d+: '%)' expected near '}'", "expr ( )")

    f, msg = loadstring [[function f () return 1]]
    like(msg, ":%d+: 'end' expected near '?<eof>'?", "function end")

    f, msg = loadstring [[do local a = f()]]
    like(msg, ":%d+: 'end' expected near '?<eof>'?", "do end")

    f, msg = loadstring [[for i = 1, 2 do print(i)]]
    like(msg, ":%d+: 'end' expected near '?<eof>'?", "for end")

    f, msg = loadstring [[if true then f()]]
    like(msg, ":%d+: 'end' expected near '?<eof>'?", "if end")

    f, msg = loadstring [[while true do f()]]
    like(msg, ":%d+: 'end' expected near '?<eof>'?", "while end")

    f, msg = loadstring [[repeat f()]]
    like(msg, ":%d+: 'until' expected near '?<eof>'?", "repeat until")

    f, msg = loadstring [[function f (a, 2) return a * 2 end]]
    like(msg, ":%d+: <name> or '...' expected near '2'", "function parameter list")

    f, msg = loadstring [[a = o:m[1, 2)]]
    like(msg, ":%d+: function arguments expected near '%['", "function argument list")

    f, msg = loadstring [[for i do print(i) end]]
    like(msg, ":%d+: '=' or 'in' expected near 'do'", "for init")

    f, msg = loadstring [[for i = 1, 2 print(i) end]]
    like(msg, ":%d+: 'do' expected near 'print'", "for do")

    f, msg = loadstring [[if true f() end]]
    like(msg, ":%d+: 'then' expected near 'f'", "if then")

    f, msg = loadstring [[while true f() end]]
    like(msg, ":%d+: 'do' expected near 'f", "while do")
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
