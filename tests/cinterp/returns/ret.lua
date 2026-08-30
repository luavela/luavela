-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

-- NB: return from bar() to foo() is used to get entire bytecode executed
-- (i.e. return to fixarg Lua function). Returning directly from cinterpcall
-- will be considered as non-standard return and will be handled by vm_return()
-- instead of the bytecode itself.

do -- no expected return values, 2 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    KPRI     4   1
	-- 0007    RET      3   3
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, false
	end

	local foo = function()
		bar()
	end

	cinterpcall(foo)
end

do -- no expected return values, 3 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    KPRI     4   1
	-- 0007    MOV      5   2
	-- 0008    RET      3   4
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, false, var3
	end

	local foo = function()
		bar()
	end

	cinterpcall(foo)
end

do -- 1 expected return, 2 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0 123
	-- 0003    KPRI     1   1
	-- 0004    RET      0   3
	local bar = function()
		return 123, false
	end

	local foo = function()
		local ret = bar()
		return ret
	end

	local ret = cinterpcall(foo)
	assert(ret == 123)
end

do -- 1 expected return, 3 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KSTR     1   0      ; "test"
	-- 0004    KPRI     2   1
	-- 0005    RET      0   4
	local bar = function()
		return 42, 'test', false
	end

	local foo = function()
		local ret = bar()
		return ret
	end

	local ret = cinterpcall(foo)
	assert(ret == 42)
end

do -- 2 expected returns, 2 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    MOV      4   1
	-- 0007    RET      3   3
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, var2
	end

	local foo = function()
		local ret1, ret2 = bar()
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == false)
end

do -- 2 expected returns, 3 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    MOV      4   1
	-- 0007    MOV      5   2
	-- 0008    RET      3   4
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, var2, var3
	end

	local foo = function()
		local ret1, ret2 = bar()
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == false)
end

do -- 2 expected returns, 4 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    KSHORT   3 123
	-- 0006    MOV      4   0
	-- 0007    TNEW     5   0
	-- 0008    KSTR     6   0      ; "test"
	-- 0009    RET      3   5
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return 123, var1, {}, 'test'
	end

	local foo = function()
		local ret1, ret2 = bar()
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 123)
	assert(ret2 ==  42)
end

do -- 3 expected returns, 2 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    MOV      4   1
	-- 0007    RET      3   3
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, var2
	end

	local foo = function()
		local ret1, ret2, ret3 = bar()
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == false)
	assert(ret3 == nil)
end

do -- 3 expected returns, 3 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    KSHORT   4 123
	-- 0007    MOV      5   1
	-- 0008    RET      3   4
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, 123, var2
	end

	local foo = function()
		local ret1, ret2, ret3 = bar()
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == 123)
	assert(ret3 == false)
end

do -- 3 expected returns, 4 actual returns
	-- 0001    HOTCNT
	-- 0002    KSHORT   0  42
	-- 0003    KPRI     1   1
	-- 0004    KPRI     2   2
	-- 0005    MOV      3   0
	-- 0006    KSTR     4   0      ; "test"
	-- 0007    MOV      5   1
	-- 0008    KSHORT   6 123
	-- 0009    RET      3   5
	local bar = function()
		local var1 = 42
		local var2 = false
		local var3 = true
		return var1, 'test', var2, 123
	end

	local foo = function()
		local ret1, ret2, ret3 = bar()
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == 'test')
	assert(ret3 == false)
end

do -- return from vararg function (fix base and RA)
	-- 0001    HOTCNT
	-- 0002    KSTR     0   0      ; "test1"
	-- 0003    KSTR     1   1      ; "test2"
	-- 0004    KSHORT   2  10
	-- 0005    KSHORT   3  11
	-- 0006    RET      2   3
	local bar = function(...)
		local var1 = 'test1'
		local var1 = 'test2'
		return 10, 11
	end

	local foo = function()
		local ret1, ret2 = bar(20, 21, 22)
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 10)
	assert(ret2 == 11)
end
