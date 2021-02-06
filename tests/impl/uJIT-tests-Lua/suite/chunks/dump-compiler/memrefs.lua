-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local meta = {
    __len = function (op)
        if type(op) == "string" then
            return string.len(op)
        elseif type(op) == "table" then
            return op[0] or 0
        else
            return 0
        end
    end
}

local t = {}
local x = {}
setmetatable(t, meta)

local bar = function (y)
    local Y = y > 50
    return Y
end

local function foo(n)
    return bar(n) and n - 10 or n + 20
end

t[0] = 0
local value = 0
for i = 1, 1E6 do
    table.insert(t, i % 2 == 1 and i * 2 or math.ceil(i / 10))
    t[0] = t[0] + 1
    table.insert(x, #t)
    value = value + foo(t[0])
end;
print(#t)
