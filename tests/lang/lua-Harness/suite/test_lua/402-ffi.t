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

=head1 FFI Library

=head2 Synopsis

    % prove 402-ffi.t

=head2 Description

See L<http://luajit.org/ext_ffi.html>.

=cut

--]]

require 'tap'

if not jit then
    skip_all("only with LuaJIT")
end

if not pcall(require, 'ffi') then
    plan(2)
    is(_G.ffi, nil, "no FFI")
    is(package.loaded.ffi, nil)
    os.exit(0)
end

plan(33)

is(_G.ffi, nil, "ffi not loaded by default")
ffi = require'ffi'
is(package.loaded.ffi, ffi, "package.loaded")
is(require'ffi', ffi, "require")

do -- C
    type_ok(ffi.C, 'userdata', 'C')
end

do -- abi
    type_ok(ffi.abi('32bit'), 'boolean', "abi")
    type_ok(ffi.abi('64bit'), 'boolean')
    type_ok(ffi.abi('le'), 'boolean')
    type_ok(ffi.abi('be'), 'boolean')
    type_ok(ffi.abi('fpu'), 'boolean')
    type_ok(ffi.abi('softfp'), 'boolean')
    type_ok(ffi.abi('hardfp'), 'boolean')
    type_ok(ffi.abi('eabi'), 'boolean')
    type_ok(ffi.abi('win'), 'boolean')
    is(ffi.abi('bad'), false)
    is(ffi.abi(0), false)

    error_like(function () ffi.abi(true) end,
               "^[^:]+:%d+: bad argument #1 to 'abi' %(string expected, got boolean%)",
               "function unpack missing size")
end

do -- alignof
    type_ok(ffi.alignof, 'function', "alignof")
end

do -- arch
    is(ffi.arch, jit.arch, "alias arch")
end

do -- cast
    type_ok(ffi.cast, 'function', "cast")
end

do -- cdef
    type_ok(ffi.cdef, 'function', "cdef")
end

do -- copy
    type_ok(ffi.copy, 'function', "copy")
end

do -- errno
    type_ok(ffi.errno, 'function', "errno")
end

do -- fill
    type_ok(ffi.fill, 'function', "fill")
end

do -- gc
    type_ok(ffi.gc, 'function', "gc")
end

do -- istype
    type_ok(ffi.istype, 'function', "istype")
end

do -- load
    type_ok(ffi.load, 'function', "load")
end

do -- metatype
    type_ok(ffi.metatype, 'function', "metatype")
end

do -- new
    type_ok(ffi.new, 'function', "new")
end

do -- offsetof
    type_ok(ffi.offsetof, 'function', "offsetof")
end

do -- os
    is(ffi.os, jit.os, "alias os")
end

do -- sizeof
    type_ok(ffi.sizeof, 'function', "sizeof")
end

do -- string
    type_ok(ffi.string, 'function', "string")
end

do -- typeof
    type_ok(ffi.typeof, 'function', "typeof")
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
