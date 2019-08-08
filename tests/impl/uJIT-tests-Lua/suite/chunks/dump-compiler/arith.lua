-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NYI: MULOV

local function foo(x, y, z)
    local a = x + y
    local b = y - z
    local c = z * x
    local _ = x / y
    local _ = y % z
    local f = math.pow(z, x)
    local g = -y
    local h = math.abs(z)
    local j = math.ldexp(y, z)
    local k = math.min(a, b)
    local l = math.max(a, b)
    local m = math.sin(c)

    return (k + l) * m + j - g * h * f
end

local function bar(x)
    local a = -x
    local b = x + 50
    return a * 3 + a % b + a * b * (-1000)
end

local value = 0
for i = 1, 100 do
    value = value + foo(i, math.ceil(i / 10), i + 5)
end;
print(value)

value = 0
for i = 1, 100 do
    value = value + bar(i)
end;
print(value)
