-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

-- NB: return from bar() to foo() is used to get entire bytecode executed
-- (i.e. return to fixarg Lua function). Returning directly from cinterpcall
-- will be considered as non-standard return and will be handled by vm_return()
-- instead of the bytecode itself.

do -- return empty vararg
	-- 0001    HOTCNT
	-- 0002    KSTR     0   0      ; "test1"
	-- 0003    KSTR     1   1      ; "test2"
	-- 0004    VARG     2   0   0
	-- 0005    RETM     2   0
	local bar = function(...)
		local a = 'test1'
		local b = 'test2'
		return ...
	end

	local foo = function()
		local ret1, ret2, ret3 = bar() -- no args
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == nil)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- return from vararg function with VARG bytecode (var1)
	-- 0001    HOTCNT
	-- 0002    VARG     0   0   0
	-- 0003    RETM     0   0
	local bar = function(...)
		return ...
	end

	local foo = function()
		local ret1, ret2, ret3, ret4, ret5, ret6 = bar(20, 21, 22)
		return ret1, ret2, ret3, ret4, ret5, ret6
	end

	local ret1, ret2, ret3, ret4, ret5, ret6 = cinterpcall(foo)
	assert(ret1 == 20)
	assert(ret2 == 21)
	assert(ret3 == 22)
	assert(ret4 == nil)
	assert(ret5 == nil)
	assert(ret6 == nil)
end

do -- return from vararg function with VARG bytecode (var2)
	-- 0001    HOTCNT
	-- 0002    KSTR     0   0      ; "test1"
	-- 0003    KSTR     1   1      ; "test2"
	-- 0004    KSHORT   2  10
	-- 0005    KSHORT   3  11
	-- 0006    VARG     4   0   0
	-- 0007    RETM     2   2
	local bar = function(...)
		local a = 'test1'
		local b = 'test2'
		return 10, 11, ...
	end

	local foo = function()
		local ret1, ret2, ret3, ret4, ret5, ret6 = bar(20, 21, 22)
		return ret1, ret2, ret3, ret4, ret5, ret6
	end

	local ret1, ret2, ret3, ret4, ret5, ret6 = cinterpcall(foo)
	assert(ret1 == 10)
	assert(ret2 == 11)
	assert(ret3 == 20)
	assert(ret4 == 21)
	assert(ret5 == 22)
	assert(ret6 == nil)
end

do -- non-standard return
	local foo = function(...)
		return ...
	end

	cinterpcall(foo)
end

-- RETM should set proper number of returned varargs
-- otherwise they will be lost in case of return from
-- protected frame (variant 1)
do
	local foo = function(...)
		return ...
	end

	local caller = function()
		return pcall(foo, 42, 43)
	end

	local ok, ret1, ret2, ret3 = cinterpcall(caller)
	assert(ok == true)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == nil)
end

do -- same as before (v2)
	local bar = function(...)
		return ...
	end

	local foo = function(...)
		return bar(...)
	end

	local caller = function()
		return pcall(foo, 42, 43)
	end

	local ok, ret1, ret2, ret3 = cinterpcall(caller)
	assert(ok == true)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == nil)
end

do -- same as before (v3)
	local bar = function(...)
		return ...
	end

	local foo = function(...)
		return pcall(bar, ...)
	end

	local caller = function()
		return pcall(foo, 42, 43)
	end

	local ok1, ok2, ret1, ret2, ret3 = cinterpcall(caller)
	assert(ok1 == true)
	assert(ok2 == true)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == nil)
end
