#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 JIT Library

=head2 Synopsis

    % prove 403-jit.t

=head2 Description

See L<http://luajit.org/ext_jit.html>.

=cut

--]]

require 'tap'

if not jit then
    skip_all("only with LuaJIT")
end

local compiled_with_jit = jit.status()
local has_jit_opt = compiled_with_jit
local has_jit_util = false -- UJIT: we don't have jit.util.* methods

plan'no_plan'

is(package.loaded.jit, _G.jit, "package.loaded")
is(require'jit', jit, "require")

do -- arch
    type_ok(jit.arch, 'string', "arch")
end

do -- flush
    type_ok(jit.flush, 'function', "flush")
end

do -- off
    jit.off()
    is(jit.status(), false, "off")
end

-- on
if compiled_with_jit then
    jit.on()
    is(jit.status(), true, "on")
else
    error_like(function () jit.on() end,
               "^[^:]+:%d+: JIT compiler permanently disabled by build option",
               "no jit.on")
end

-- opt
if has_jit_opt then
    type_ok(jit.opt, 'table', "opt.*")
    type_ok(jit.opt.start, 'function', "opt.start")
else
    is(jit.opt, nil, "no jit.opt")
end

do -- os
    type_ok(jit.os, 'string', "os")
end

do -- status
    local status = { jit.status() }
    type_ok(status[1], 'boolean', "status")
    if compiled_with_jit then
        for i = 2, #status do
            type_ok(status[i], 'string', status[i])
        end
    else
        is(#status, 1)
    end
end

-- util
if has_jit_util then
    type_ok(jit.util, 'table', "util.*")
else
    is(jit.util, nil, "no jit.util")
end

do -- version
    type_ok(jit.version, 'string', "version")
    like(jit.version, '^LuaJIT 2%.%d%.%d')
end

do -- version_num
    type_ok(jit.version_num, 'number', "version_num")
    like(string.format("%06d", jit.version_num), '^020[01]%d%d$')
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
