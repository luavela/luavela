-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local nan = ujit.math.nan
local lua_nan = 0 / 0

assert(type(nan) == 'number')
assert(type(lua_nan) == 'number')

-- NaN is not equal to itself
assert(lua_nan ~= lua_nan)
assert(nan ~= nan)

assert(nan ~= lua_nan)

assert(ujit.math.isnan(nan))
assert(ujit.math.isnan(lua_nan))

-- Coercion
assert("nan" ~= lua_nan)
assert("nan" ~= nan)

-- tostring
assert(tostring(lua_nan) == "nan")
assert(tostring(nan) == "nan")
