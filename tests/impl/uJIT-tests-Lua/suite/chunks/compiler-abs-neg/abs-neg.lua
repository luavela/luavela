-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function foo(x)
    local a = math.abs(x)
    local b = -x
    -- NB! The line below force IR_NEG to be emitted
    local c = x % 2 == 0 and bit.rshift(b, 1) or bit.lshift(b, 1)
    return a + 5 * c
end

local value = 0
for i = -100, 50 do
    value = value + foo(i)
end

assert(value == 268435485825)
