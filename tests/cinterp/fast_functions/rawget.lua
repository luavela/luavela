-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

local caller = function(f)
	local ok, msg = pcall(f)
	return ok, msg
end

do -- rawget()
	local rawget_no_args = function()
		local res = rawget()
		return res
	end

	local ok, msg = cinterpcall(caller, rawget_no_args)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'rawget' (table expected, got no value)")
end

do -- rawget(t)
	local rawget_no_second_arg = function()
		local res = rawget({})
		return res
	end

	local ok, msg = cinterpcall(caller, rawget_no_second_arg)
	assert(not ok)
	assert_ends_with(msg, "bad argument #2 to 'rawget' (value expected)")
end


do -- rawget(nil)
	local rawget_nil = function()
		local res = rawget(nil)
		return res
	end

	local ok, msg = cinterpcall(caller, rawget_nil)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'rawget' (table expected, got nil)")
end

do -- rawget(string)
	local rawget_str = function()
		local res = rawget("test")
		return res
	end

	local ok, msg = cinterpcall(caller, rawget_str)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'rawget' (table expected, got string)")
end

do -- rawget(table, nil)
	local rawget_table_nil = function()
		local res = rawget({}, nil)
		return res
	end

	local res = cinterpcall(rawget_table_nil)
	assert(res == nil)
end

do -- rawget(table, key) - with no __index metamethod set
	local rawget_table_mt = function()
		local t = { test = "hello" }
		local res = rawget(t, "missing")
		return res
	end

	local res = cinterpcall(rawget_table_mt)
	assert(res == nil)
end

do -- rawget(table, key) - with __index metamethod set
	local rawget_table_mt = function()
		local t = { test = "hello" }
		local mt = { __index = function(self, t, k) return "__index" end }
		setmetatable(t, mt)

		local res = t["missing"] -- calls __index
		local res2 = rawget(t, "missing") -- doesn't call __index
		return {res, res2}
	end

	local res = cinterpcall(rawget_table_mt)
	assert(res[1] == "__index")
	assert(res[2] == nil)
end
