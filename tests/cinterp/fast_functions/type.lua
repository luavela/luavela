-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

local foo = function(val)
	local ret = type(val)
	return ret
end

local co = coroutine.create(function() end)

assert(cinterpcall(foo, nil)    == 'nil')
assert(cinterpcall(foo, false)  == 'boolean')
assert(cinterpcall(foo, true)   == 'boolean')
assert(cinterpcall(foo, 42)     == 'number')
assert(cinterpcall(foo, 42.5)   == 'number')
assert(cinterpcall(foo, 'test') == 'string')
assert(cinterpcall(foo, foo)    == 'function')
assert(cinterpcall(foo, co)     == 'thread')
assert(cinterpcall(foo, {})     == 'table')
-- TODO: userdata?

do -- more results than returns (var1)
	local foo = function()
		local ret1, ret2 = type('str')
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 'string')
	assert(ret2 == nil)
end

do -- more results than returns (var2)
	local foo = function()
		local ret1, ret2, ret3 = type(false)
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 'boolean')
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- empty argument test
	local foo = function()
		type()
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'type' (value expected)")
end
