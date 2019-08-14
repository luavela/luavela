#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2018-2019, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 LuaJIT Stand-alone

=head2 Synopsis

    % prove 404-luajit.t

=head2 Description

See L<http://luajit.org/running.html>

=cut

--]]

require'tap'

if not jit or ujit then
    skip_all("only with LuaJIT")
end

local lua = arg[-3] or arg[-1]

if not pcall(io.popen, lua .. [[ -e "a=1"]]) then
    skip_all("io.popen not supported")
end

local compiled_with_jit = jit.status()

plan'no_plan'
diag(lua)

local f = io.open('hello-404.lua', 'w')
f:write([[
print 'Hello World'
]])
f:close()

os.execute(lua .. " -b hello-404.lua hello-404.out")
local cmd = lua .. " hello-404.out"
f = io.popen(cmd)
is(f:read'*l', 'Hello World', "-b")
f:close()

os.execute(lua .. " -bg hello-404.lua hello-404.out")
cmd = lua .. " hello-404.out"
f = io.popen(cmd)
is(f:read'*l', 'Hello World', "-bg")
f:close()

os.execute(lua .. " -be 'print[[Hello World]]' hello-404.out")
cmd = lua .. " hello-404.out"
f = io.popen(cmd)
is(f:read'*l', 'Hello World', "-be")
f:close()

os.remove('hello-404.out') -- clean up

cmd = lua .. " -bl hello-404.lua"
f = io.popen(cmd)
like(f:read'*l', '^%-%- BYTECODE %-%- hello%-404%.lua', "-bl hello.lua")
like(f:read'*l', '^0001    %u[%u%d]+%s+')
like(f:read'*l', '^0002    %u[%u%d]+%s+')
like(f:read'*l', '^0003    %u[%u%d]+%s+')
f:close()

os.execute(lua .. " -bl hello-404.lua hello-404.txt")
f = io.open('hello-404.txt', 'r')
like(f:read'*l', '^%-%- BYTECODE %-%- hello%-404%.lua', "-bl hello.lua hello.txt")
like(f:read'*l', '^0001    %u[%u%d]+%s+')
like(f:read'*l', '^0002    %u[%u%d]+%s+')
like(f:read'*l', '^0003    %u[%u%d]+%s+')
f:close()

os.remove('hello-404.txt') -- clean up

os.execute(lua .. " -b hello-404.lua hello-404.c")
f = io.open('hello-404.c', 'r')
like(f:read'*l', '^#ifdef _cplusplus$', "-b hello.lua hello.c")
like(f:read'*l', '^extern "C"$')
like(f:read'*l', '^#endif$')
like(f:read'*l', '^#ifdef _WIN32$')
like(f:read'*l', '^__declspec%(dllexport%)$')
like(f:read'*l', '^#endif$')
like(f:read'*l', '^const.- char luaJIT_BC_hello_404%[%] = {$')
like(f:read'*l', '^%d+,%d+,%d+,')
f:close()

os.remove('hello-404.c') -- clean up

os.execute(lua .. " -b hello-404.lua hello-404.h")
f = io.open('hello-404.h', 'r')
like(f:read'*l', '^#define luaJIT_BC_hello_404_SIZE %d+$', "-b hello.lua hello.h")
like(f:read'*l', '^static const.- char luaJIT_BC_hello_404%[%] = {$')
like(f:read'*l', '^%d+,%d+,%d+,')
f:close()

os.remove('hello-404.h') -- clean up

cmd = lua .. " -j flush hello-404.lua"
f = io.popen(cmd)
is(f:read'*l', 'Hello World', "-j flush")
f:close()

cmd = lua .. " -joff hello-404.lua"
f = io.popen(cmd)
is(f:read'*l', 'Hello World', "-joff")
f:close()

cmd = lua .. " -jon hello-404.lua 2>&1"
f = io.popen(cmd)
if compiled_with_jit then
    is(f:read'*l', 'Hello World', "-jon")
else
    like(f:read'*l', "^[^:]+: JIT compiler permanently disabled by build option", "no jit")
end
f:close()

cmd = lua .. " -j bad hello-404.lua 2>&1"
f = io.popen(cmd)
like(f:read'*l', "^[^:]+: unknown luaJIT command or jit%.%* modules not installed", "-j bad")
f:close()

if compiled_with_jit then
    cmd = lua .. " -O hello-404.lua"
    f = io.popen(cmd)
    is(f:read'*l', 'Hello World', "-O")
    f:close()

    cmd = lua .. " -O3 hello-404.lua"
    f = io.popen(cmd)
    is(f:read'*l', 'Hello World', "-O3")
    f:close()

    cmd = lua .. " -Ocse -O-dce -Ohotloop=10 hello-404.lua"
    f = io.popen(cmd)
    is(f:read'*l', 'Hello World', "-Ocse -O-dce -Ohotloop=10")
    f:close()

    cmd = lua .. " -O+cse,-dce,hotloop=10 hello-404.lua"
    f = io.popen(cmd)
    is(f:read'*l', 'Hello World', "-O+cse,-dce,hotloop=10")
    f:close()

    cmd = lua .. " -O+bad hello-404.lua 2>&1"
    f = io.popen(cmd)
    like(f:read'*l', "^[^:]+: unknown or malformed optimization flag '%+bad'", "-O+bad")
    f:close()
else
    cmd = lua .. " -O0 hello-404.lua 2>&1"
    f = io.popen(cmd)
    like(f:read'*l', "^[^:]+: attempt to index a nil value")
    f:close()
end

os.remove('hello-404.lua') -- clean up

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
