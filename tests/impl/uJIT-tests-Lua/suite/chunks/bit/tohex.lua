-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Tests to check some bit.tohex implementation-defined corner cases
--

assert(bit.tohex(1.5) == "00000002")
assert(bit.tohex(0.5) == "00000000")
assert(bit.tohex(-0.5) == "00000000")
assert(bit.tohex(-1.5) == "fffffffe")
assert(bit.tohex(6.5, -1.5) == "06")
assert(bit.tohex(2500000000.0) == "9502f900")
assert(bit.tohex(25500000000.0, -7.5) == "EFEB1F00")
