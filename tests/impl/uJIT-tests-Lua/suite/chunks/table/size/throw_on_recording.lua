-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

-- Generate non-hoistable table for test below ({ {1}, {2}, ... })
local t = {}
for i = 1, 3 do
    table.insert(t, { i })
end

t[2] = 42 -- this will make uJIT throw error on recording (wrong type to ujit.table.size)

jit.on()
jit.opt.start(3, "hotloop=1")

local s = 0
for i = 1, #t do
    s = s + ujit.table.size(t[i])
end
