-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()
local mt = {
    __mul = function(x, y)
        return { x[1] * y[1] }
    end
}

local function bar(baz)
    local var01 = {} -- <-- GC is triggered here
    local var02 = "str02"
    local var03 = "str03"
    local var04 = "str04"
    local var05 = "str05"
    local var06 = "str06"
    local var07 = "str07"
    local var08 = "str08"
    local var09 = "str09"
    local var10 = "str10"
    local var11 = "str11"
    local var12 = "str12"
    local var13 = "str13"
    local var14 = "str14"
    local var15 = baz[1]
end
--- NB! DO NOT EDIT/MOVE THE LINES BELOW IN ANY MANNER -------------------------
local function foo()
    local t1 = setmetatable({10}, mt)
    local t2 = setmetatable({20}, mt)
    return bar(t1 * t2)
end
--- NB! DO NOT EDIT/MOVE THE LINES ABOVE IN ANY MANNER -------------------------
collectgarbage("setpause", 50)
collectgarbage("setstepmul", 1e4)
for _ = 1, 370 do
    local _ = {}
end

foo()
