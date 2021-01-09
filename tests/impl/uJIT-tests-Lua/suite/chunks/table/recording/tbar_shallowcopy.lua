-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "hotloop=1")

local shallowcopy = ujit.table.shallowcopy
local t = {1, 2, 3}

for _ = 1, 3 do
	local b = shallowcopy(t)
	-- New table store triggers allocation. Destination table is GC-collectable
	-- thus this allocation triggers 'IR_TDUP' emit.
	b["key"] = {}
end
