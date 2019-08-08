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

=head1 BitOp Library

=head2 Synopsis

    % prove 401-bitop.t

=head2 Description

See L<http://bitop.luajit.org/>.

=cut

--]]

require 'tap'

if not jit then
    skip_all("only with LuaJIT")
end

plan(29)

is(package.loaded.bit, _G.bit, "package.loaded")
is(require'bit', bit, "require")

do -- arshift
    is(bit.arshift(256, 8), 1, "function arshift")
    is(bit.arshift(-256, 8), -1)
end

do -- band
    is(bit.band(0x12345678, 0xff), 0x00000078, "function band")
end

do -- bnot
    is(bit.bnot(0), -1, "function bnot")
    is(bit.bnot(-1), 0)
    is(bit.bnot(0xffffffff), 0)
end

do -- bor
    is(bit.bor(1, 2, 4, 8), 15, "function bor")
end

do -- bswap
    is(bit.bswap(0x12345678), 0x78563412, "function bswap")
    is(bit.bswap(0x78563412), 0x12345678)
end

do -- bxor
    is(bit.bxor(0xa5a5f0f0, 0xaa55ff00), 0x0ff00ff0, "function bxor")
end

do -- lshift
    is(bit.lshift(1, 0), 1, "function lshift")
    is(bit.lshift(1, 8), 256)
    is(bit.lshift(1, 40), 256)
    is(bit.lshift(0x87654321, 12), 0x54321000)
end

do -- rol
    is(bit.rol(0x12345678, 12), 0x45678123, "function rol")
end

do -- ror
    is(bit.ror(0x12345678, 12), 0x67812345, "function ror")
end

do -- rshift
    is(bit.rshift(256, 8), 1, "function rshift")
    is(bit.rshift(-256, 8), 16777215)
    is(bit.rshift(0x87654321, 12), 0x00087654)
end

do -- tobit
    is(bit.tobit(0xffffffff + 1), 0, "function tobit")
    is(bit.tobit(2^40 + 1234), 1234)
end

do -- tohex
    is(bit.tohex(1), '00000001', "function tohex")
    is(bit.tohex(-1), 'ffffffff')
    is(bit.tohex(0xffffffff), 'ffffffff')
    is(bit.tohex(-1, -8), 'FFFFFFFF')
    is(bit.tohex(0x21, 4), '0021')
    is(bit.tohex(0x87654321, 4), '4321')
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
