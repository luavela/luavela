-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "hotloop=1")

local shallowcopy = ujit.table.shallowcopy
local t = {4, 5, 6}

-- Variable is defined in the outer scope of 'for' -> independent of the loop.
-- Thus it triggers sinking.
local b

for _ = 1, 3 do
	-- 'shallowcopy' triggers 'TDUP' emit. Inside a trace it's a candidate
	-- for sinking as it triggers allocation.
	b = shallowcopy(t)
end
