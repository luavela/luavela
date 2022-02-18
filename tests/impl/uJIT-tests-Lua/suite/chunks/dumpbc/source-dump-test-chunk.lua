-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local x = 0

-- tests that source of function prototype dump gets printed correctly
local function f(arg)
    local y = arg
    local f2 = function()
        local inc = 5
        y = y + inc
    end

    f2()
end

f(x)

local t = {}

-- TSETS below corresponds to line 22, but we printed line 24 ('end' of t:f) and we can't go back
function t:f(arg)
    print(self, arg)
end

print(t)
