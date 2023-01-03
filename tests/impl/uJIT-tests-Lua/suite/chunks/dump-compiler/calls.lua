-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function foo(x)
    if x > 42000 then
        return x * 2
    else
        return math.floor(x / 10)
    end
end

local x = 0
local y = 0
for i = 1, 1e6 do
    x = x + i
    y = math.ceil(y + foo(i) - math.sin(x))
end;
print(x)
print(y)
