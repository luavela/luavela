-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- asize=0, hbits=0
	-- 0001    HOTCNT
	-- 0002    TNEW     0   0
	-- 0003    RET1     0   2
	local foo = function()
		local tab = {}
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 0)
end

do -- asize=3, hbits=1
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KSTR     1   0      ; "test"
	-- 0004    KPRI     2   0
	-- 0005    KSHORT   3 2024
	-- 0006    TNEW     4 2051
	-- 0007    TSETB    0   4   1
	-- 0008    TSETB    1   4   2
	-- 0009    KPRI     5   2
	-- 0010    TSETV    2   4   5
	-- 0011    KPRI     5   1
	-- 0012    TSETV    3   4   5
	-- 0013    RET1     4   2
	local foo = function()
		local a = 42
		local b = 'test'
		local c = nil
		local d = 2024
		return {a, b, [true] = c, [false] = d}
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 3)
	assert(ret[1] == 42)
	assert(ret[2] == 'test')
	assert(ret[true] == nil)
	assert(ret[false] == 2024)
end
