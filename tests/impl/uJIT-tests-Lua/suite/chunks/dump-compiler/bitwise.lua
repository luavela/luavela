-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NYI: BSAR

local function foo(x)
    local a = bit.bnot(x)
    local b = bit.bswap(x)
    local c = bit.band(a, b)
    local d = bit.bor(a, b)
    local e = bit.bxor(c, d)
    local f = bit.lshift(a, x)
    local g = bit.rshift(b, x)
    local h = bit.rol(x, f)
    local i = bit.ror(x, g)
    return a + b + c + d + e * f * g * h * i
end

local value = 0
for i = 1, 100 do
    value = value + foo(i) + foo(i + 1.75)
end;
print(value)
