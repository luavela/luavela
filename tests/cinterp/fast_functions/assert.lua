-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- assert should pass
	local foo = function()
		local a = 1
		local b = 2
		assert(a ~= b)
	end

	cinterpcall(foo)
end

do -- assert should pass
	local foo = function()
		local a = 2
		local b = 2
		assert(a == b)
	end

	cinterpcall(foo)
end

do -- should throw
	local foo = function()
		local a = 1
		local b = 2
		assert(a == b)
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "assertion failed!")
end

do -- empty argument list - should throw also
	local foo = function()
		assert()
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'assert' (value expected)")
end

-- all testcases below check that assert returns all passed arguments correctly

do -- returns 1 arg
	local foo = function()
		local a = 1
		return assert(a)
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 1)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- returns 2 args
	local foo = function()
		local a = 1
		local b = 2
		return assert(a, b)
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 1)
	assert(ret2 == 2)
	assert(ret3 == nil)
end

do -- returns 3 args
	local foo = function()
		local a = 1
		local b = 2
		local c = 3
		return assert(a, b, c)
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 1)
	assert(ret2 == 2)
	assert(ret3 == 3)
end

do -- returns comparison result and other arguments
	local foo = function()
		local a = 1
		local b = 2
		local c = 3
		local d = 4
		return assert(a ~= 2, a, b, c, d)
	end

	local ret1, ret2, ret3, ret4, ret5 = cinterpcall(foo)
	assert(ret1 == true)
	assert(ret2 == 1)
	assert(ret3 == 2)
	assert(ret4 == 3)
	assert(ret5 == 4)
end
