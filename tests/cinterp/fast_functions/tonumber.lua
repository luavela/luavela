-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

do -- tonumber(nil)
	local tonumber_nil = function()
		local res = tonumber(nil)
		return res
	end

	local ret = cinterpcall(tonumber_nil)
	assert(ret == nil)
end

do -- tonumber()
	local tonumber_no_args = function()
		local res = tonumber()
		return res
	end

	local caller = function()
		local ok, msg = pcall(tonumber_no_args)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'tonumber' (value expected)")
end

do -- tonumber(number)
	local tonumber_number = function()
		local res = tonumber(42)
		return res
	end

	local ret = cinterpcall(tonumber_number)
	assert(ret == 42)
end

do -- tonumber(string)
	local tonumber_string = function()
		local res = tonumber("42")
		return res
	end

	local ret = cinterpcall(tonumber_string)
	assert(ret == 42)
end

do -- tonumber(string), float
	local tonumber_string_float = function()
		local res = tonumber("5.5")
		return res
	end

	local ret = cinterpcall(tonumber_string_float)
	assert(ret == 5.5)
end

do -- tonumber(string, base)
	local tonumber_base = function()
		local res = tonumber("100100", 2) -- 36
		return res
	end

	local ret = cinterpcall(tonumber_base)
	assert(ret == 36)
end

do -- tonumber(string, base, <extra args>)
	local tonumber_base = function()
		local res = tonumber("100100", 2,
			45, "what", {} -- extra args are ignored
		) -- 36
		return res
	end

	local ret = cinterpcall(tonumber_base)
	assert(ret == 36)
end

do -- tonumber(table)
	local tonumber_base = function()
		local res = tonumber({test = "ok"})
		return res
	end

	local ret = cinterpcall(tonumber_base)
	assert(ret == nil)
end
