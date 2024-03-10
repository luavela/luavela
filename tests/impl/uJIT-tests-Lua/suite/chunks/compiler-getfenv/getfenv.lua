-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function foo(bar)
    local e = getfenv(0);
    local x = (bar > 50 and e.print or e.math)
    return type(x) == 'table' and x.abs(bar) or 42
end

local value = 0
for i = 1, 100 do
    value = value + foo(i)
end
assert(value == 3375)
print(value)
