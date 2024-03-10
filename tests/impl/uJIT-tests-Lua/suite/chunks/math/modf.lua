-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local i, f
local f_eps -- fraction component, to eliminate problems with epsilon in assertions

--
-- A couple of trivial cases, positive numbers:
--

i, f = math.modf(65536)
assert(i == 65536 and f == 0)

f_eps = 42.4242 - 42
i, f  = math.modf(42.4242)
assert(i == 42 and f == f_eps)

--
-- A couple of trivial cases, negative numbers:
--

i, f = math.modf(-65536)
assert(i == -65536 and f == 0)

f_eps = -42.4242 + 42
i, f  = math.modf(-42.4242)
assert(i == -42 and f == f_eps)

-- Subnormal positive number
i, f = math.modf(0.4E-323)
assert(i == 0 and f == 0.4E-323)

-- Subnormal negative number
i, f = math.modf(-0.4E-323)
assert(i == 0 and f == -0.4E-323)

-- +infinity
local plus_inf = 1 / 0
i, f = math.modf(plus_inf)
assert(i == plus_inf and f == 0)

-- -infinity
local minus_inf = -1 / 0
i, f = math.modf(minus_inf)
assert(i == minus_inf and f == 0)

-- Not a number
i, f = math.modf(0 / 0)
assert(i ~= i and f ~= f) -- NB! NaN is not self-equal

-- Small positive number, less than epsilon
i, f = math.modf(0.4E-500)
assert(i == 0 and f == 0)

--
-- Huge numbers, remember that representation precision
-- is variable in IEEE754
--

--
-- Numbers from the range representable with precision 0.5
--

local fp_stepping_05 = 2 ^ 51 + 2 ^ 50

i, f = math.modf(fp_stepping_05 + 0.1)
assert(i == fp_stepping_05 and f == 0)

i, f = math.modf(fp_stepping_05 + 0.5)
assert(i == fp_stepping_05 and f == 0.5)

i, f = math.modf(fp_stepping_05 + 0.6)
assert(i == fp_stepping_05 and f == 0.5)

i, f = math.modf(fp_stepping_05 + 0.9)
assert(i == fp_stepping_05 + 1 and f == 0)

--
-- Numbers from the range representable with precision 1.0
--

local fp_stepping_1  = 2 ^ 52 + 2 ^ 51

i, f = math.modf(fp_stepping_1 + 0.1)
assert(i == fp_stepping_1 and f == 0)

i, f = math.modf(fp_stepping_1 + 0.5)
assert(i == fp_stepping_1 and f == 0)

i, f = math.modf(fp_stepping_1 + 0.6)
assert(i == fp_stepping_1 + 1 and f == 0)

i, f = math.modf(fp_stepping_1 + 0.9)
assert(i == fp_stepping_1 + 1 and f == 0)
