-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local hits = 0

local function foo(s, l)
    hits = hits + 1
    local x = s:sub(l)
    local y = hits * 2

    local z = {
        x:len() > 10 and x:len() + y or 3 - y,
        x:len() > 10 and 3 - y       or x:len() + y,
    }
    z[0] = hits

    local v = {
        ['hint'] = y > 5 and 'more' or 'less',
    }
    return z, v
end

local t = {}
for i = 1, 1000 do
    table.insert(t, string.rep("abc", i))
end

local value  = 0
local buffer = {}
for i = 1, 1000 do
    local s    = t[i]
    local z, v = foo(s, math.ceil(s:len() / 2))
    value  = value + z[1] * z[0]
    table.insert(buffer, v.hint)
end
print(value)
print(#buffer)
