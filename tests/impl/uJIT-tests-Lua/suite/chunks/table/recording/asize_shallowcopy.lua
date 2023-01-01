-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("hotloop=1")

local shallowcopy = ujit.table.shallowcopy
local t = {4, 5, 6}

for i = 1, 3 do
	local b = shallowcopy(t)
	-- Load from array part triggers bounds check ir with asize.
	local _ = b[i]
end
