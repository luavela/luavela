-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local m, e

--
-- Trivial cases
--

m, e = math.frexp(65536)
assert(m == 0.5 and e == 17)

m, e = math.frexp(0.5)
assert(m == 0.5 and e == 0)

m, e = math.frexp(1)
assert(m == 0.5 and e == 1)

m, e = math.frexp(0.0078125) -- 1 / (2 ^ 7) == (2 ^ (-1)) * (2 ^ (-6))
assert(m == 0.5 and e == -6)

--
-- Trivial cases, negative numbers
--

m, e = math.frexp(-65536)
assert(m == -0.5 and e == 17)

m, e = math.frexp(-0.5)
assert(m == -0.5 and e == 0)

m, e = math.frexp(-1)
assert(m == -0.5 and e == 1)

m, e = math.frexp(-0.0078125) -- -1 / (2 ^ 7) == -1 * (2 ^ (-1)) * (2 ^ (-6))
assert(m == -0.5 and e == -6)

-- Subnormal positive number
m, e = math.frexp(0.4E-323)
assert(m == 0.5 and e == -1073)

-- Subnormal negative number
m, e = math.frexp(-0.4E-323)
assert(m == -0.5 and e == -1073)

-- +infinity
local plus_inf = 1 / 0
m, e = math.frexp(plus_inf)
assert(m == plus_inf and e == 0)

-- -infinity
local minus_inf = -1 / 0
m, e = math.frexp(minus_inf)
assert(m == minus_inf and e == 0)

-- Not a number
m, e = math.frexp(0 / 0)
assert(m ~= m and e == 0) -- NB! NaN is not self-equal

-- Small positive number, less than epsilon
m, e = math.frexp(0.4E-500)
assert(m == 0 and e == 0)
