-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

do -- getmetatable(nil)
	local getmetatable_nil = function()
		local res = getmetatable(nil)
		return res
	end

	local res = cinterpcall(getmetatable_nil)
	assert(res == nil)
end

do -- getmetatable()
	local getmetatable_no_args = function()
		local res = getmetatable()
		return res
	end

	local caller = function()
		local ok, msg = pcall(getmetatable_no_args)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	local lua52compat = (rawlen ~= nil)
	local expected = lua52compat
		and "bad argument #1 to 'getmetatable' (value expected)"
		or "bad argument #1 to 'getmetatable' (table expected, got no value)"
	assert_ends_with(msg, expected)
end

do -- getmetatable(string)
	local getmetatable_string = function()
		local res = getmetatable("hello")
		return res
	end

	local res = cinterpcall(getmetatable_string)
	local mt = getmetatable("test")
	assert(res == mt)
end

do -- getmetatable(number) - nil if not set via debug.setmetatable
	local getmetatable_num_mt_not_set = function()
		local res = getmetatable(5)
		return res
	end

	local res = cinterpcall(getmetatable_num_mt_not_set)
	assert(res == nil)
end

do -- getmetatable(number) - metatable set via debug.setmetatable
	local mt = {}

	local getmetatable_num_mt_set = function()
		-- note that we can set metatable on any number - it's assigned to a type
		debug.setmetatable(42, mt)
		local res = getmetatable(5)
		return res
	end

	local res = cinterpcall(getmetatable_num_mt_set)
	assert(res == mt)
end

do -- getmetatable(number) - metatable set via debug.setmetatable and has __metatable
	local inner = {}
	local mt = { __metatable = inner }

	local getmetatable_num_metamethod_set = function()
		debug.setmetatable(42, mt)
		local res = getmetatable(5)
		return res
	end

	local res = cinterpcall(getmetatable_num_metamethod_set)
	assert(res ~= mt)
	assert(res == inner)
end

do -- getmetatable(table) - metatable not set
	local getmetatable_table_mt_not_set = function()
		local t = {}
		local res = getmetatable(t)
		return res
	end

	local res = cinterpcall(getmetatable_table_mt_not_set)
	assert(res == nil)
end

do -- getmetatable(table) - metatable set
	local mt = {}

	local getmetatable_table_mt_set = function()
		local t = {}
		setmetatable(t, mt)
		local res = getmetatable(t)
		return res
	end

	local res = cinterpcall(getmetatable_table_mt_set)
	assert(res == mt)
end

do -- getmetatable(table) - metatable set and __metatable metamethod set
	local inner = {}
	local mt = {
		__metatable = inner
	}

	local getmetatable_table_metamethod_set = function()
		local t = {}
		setmetatable(t, mt)
		local res = getmetatable(t)
		return res
	end

	local res = cinterpcall(getmetatable_table_metamethod_set)
	assert(res ~= mt)
	assert(res == inner)
end

-- TODO: userdata tests
