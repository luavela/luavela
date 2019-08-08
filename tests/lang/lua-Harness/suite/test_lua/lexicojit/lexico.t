--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

type_ok(42LL, 'cdata', "42LL")
type_ok(42ULL, 'cdata', "42ULL")
type_ok(42uLL, 'cdata', "42uLL")
type_ok(42ull, 'cdata', "42ull")

type_ok(0x2aLL, 'cdata', "0x2aLL")
type_ok(0x2aULL, 'cdata', "0x2aULL")
type_ok(0x2auLL, 'cdata', "0x2auLL")
type_ok(0x2aull, 'cdata', "0x2aull")

type_ok(12.5i, 'cdata', '12.5i')
type_ok(12.5I, 'cdata', '12.5I')
type_ok(1i, 'cdata', '1i')
type_ok(1I, 'cdata', '1I')
type_ok(0i, 'cdata', '0i')
type_ok(0I, 'cdata', '0I')

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
