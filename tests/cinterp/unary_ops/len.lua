-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- boolean without metamethod (should trigger exception)
	local foo = function()
		local val = false
		return #val
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "attempt to get length of local 'val' (a boolean value)")
end

do -- boolean with __len metamethod
	local foo = function()
		local val = false
		local mt = {__len = function() return 42 end}
		debug.setmetatable(val, mt)
		return #val
	end

	assert(cinterpcall(foo) == 42)
end

do -- string
	local foo = function()
		local s = "teststring"
		return #s
	end

	assert(cinterpcall(foo) == 10)
end

do -- string (__len metamethod ignored)
	local foo = function()
		local s = "teststring"
		local mt = {__len = function() return 42 end}
		debug.setmetatable(s, mt)
		return #s
	end

	assert(cinterpcall(foo) == 10)
end

do -- table
	local foo = function()
		local t = {10, 20, 30}
		return #t
	end

	assert(cinterpcall(foo) == 3)
end

do -- table with __len metamethod
	local foo = function()
		local t = {10, 20, 30}
		local mt = {__len = function() return 42 end}
		setmetatable(t, mt)
		return #t
	end

	-- __len metamethod on tables is only honored under Lua 5.2 semantics
	local lua52compat = (rawlen ~= nil)
	assert(cinterpcall(foo) == (lua52compat and 42 or 3))
end
