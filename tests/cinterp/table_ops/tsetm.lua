-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- no array reallocation
	-- 0001    HOTCNT
	-- 0002    KSTR     0   0      ; "test"
	-- 0003    RET1     0   2
	local bar = function()
		return 'test'
	end

	-- 0001    HOTCNT
	-- 0002    KPRI     0   0
	-- 0003    TNEW     1   4
	-- 0004    TSETB    0   1   1
	-- 0005    UGET     2   0      ; bar
	-- 0006    CALL     2   2   1
	-- 0007    TSETB    2   1   2
	-- 0008    UGET     2   0      ; bar
	-- 0009    CALL     2   0   1
	-- 0010    TSETM    2   0      ; 3
	-- 0011    TGETB    2   1   1
	-- 0012    RET1     2   2
	local foo = function()
		local a = 42
		local tab = {a, bar(), bar()}
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 3)
	assert(ret[1] == 42)
	assert(ret[2] == 'test')
	assert(ret[3] == 'test')
end

do -- array reallocation
	local barm = function(start)
		return start + 1, start + 2, start + 3
	end

	local foo = function()
		local t = {barm(10), barm(20)}
		return t
	end

	local ret = cinterpcall(foo)
	assert(#ret == 4)
	assert(ret[1] == 11)
	assert(ret[2] == 21)
	assert(ret[3] == 22)
	assert(ret[4] == 23)
end

-- TSETM should not resize table if there is enough
-- space in array part for new keys
do
	local bar = function() return {} end

	local foo = function()
		local t = {
			key1 = 42,
			key2 = 'some_string',
			bar(),
		}

		local ret = {}
		local idx = 1

		for k, _ in pairs(t) do
			ret[idx] = k
			idx = idx + 1
		end

		return ret
	end

	local ret = cinterpcall(foo)
	assert(ret[1] == 1)
	assert(ret[2] == 'key1')
	assert(ret[3] == 'key2')
end
