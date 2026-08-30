-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

local foo = function()
	local tab = {11, nil, 'test'}
	return tab
end

-- 0001    HOTCNT
-- 0002    TDUP     0   0
-- 0003    RET1     0   2
local ret = cinterpcall(foo)
assert(ret[1] == 11)
assert(ret[2] == nil)
assert(ret[3] == 'test')
