-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NYI: GE, GT, UGE, UGT

local function bar(x)
    if x == 20 then
        print('LUCK')
    else
        for i = 1, 100 do
            x = x + ((x % 2 == 0) and 30 or (-20 * math.random(i)))
        end
        return x - 30
    end
end

local function foo(x, v)
    local r = math.random(x)
    local s = 0

    if r > r / 2 then
        s = s + 100
    end

    if r >= r / 3 then
        s = s - 100
    end

    if r < r / 4 then
        s = s + 100
    end

    if r <= r / 5 then
        s = s - 100
    end

    if r % 2 == 1 then
        s = s + 100
    end

    if r % 3 == 2 then
        s = s - 100
    end

    v[r] = s
    return s + (bar(s) or 7)
end

local value  = 0
local values = {}
for i = 1, 100 do
    value = value + foo(i, values)
end;
print(value)
