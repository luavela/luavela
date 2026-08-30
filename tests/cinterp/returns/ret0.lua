-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

-- NB: return from bar() to foo() is used to get entire bytecode executed
-- (i.e. return to fixarg Lua function). Returning directly from cinterpcall
-- will be considered as non-standard return and will be handled by vm_return()
-- instead of the bytecode itself.

do -- no expected return values
	-- 0001    HOTCNT
	-- 0002    RET0     0   1
	local bar = function() end
	local foo = function() bar() end

	cinterpcall(foo)
end

do -- 1 expected return
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    RET0     0   1
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
	end

	local foo = function()
		local ret = bar()
		return ret
	end

	local ret = cinterpcall(foo)
	assert(ret == nil)
end

do -- 2 expected returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    RET0     0   1
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
	end

	local foo = function()
		local ret1, ret2 = bar()
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do -- 3 expected returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    RET0     0   1
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
	end

	local foo = function()
		local ret1, ret2, ret3 = bar()
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == nil)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- return from vararg function (fix base and RA)
	-- 0001    HOTCNT
	-- 0002    KSTR     0   0      ; "test"
	-- 0003    RET0     0   1
	local bar = function(...)
		local var1 = 'test'
	end

	local foo = function()
		local ret = bar(20, 21, 22)
		return ret
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == nil)
	assert(ret2 == nil)
end
