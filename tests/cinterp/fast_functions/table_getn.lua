-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

do -- missing arg
	local foo = function()
		table.getn()
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'getn' (table expected, got no value)")
end

do -- arg is not a table
	local foo = function()
		table.getn(42)
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'getn' (table expected, got number)")
end

do -- lj_tab_len will be called
	local foo = function()
		return table.getn({1, 2, 3})
	end


	local ret = cinterpcall(foo)
	assert(ret == 3)
end

do -- other args are ignored
	local foo = function()
		return table.getn({1, 2, 3}, false, 42)
	end

	local ret = cinterpcall(foo)
	assert(ret == 3)
end
