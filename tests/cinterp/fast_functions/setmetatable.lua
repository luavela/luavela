-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

-- NOTE: setmetatable is extensively tested in other tests like in table
-- and comparison ops. Since error handling doesn't work properly yet, not
-- so much can be additionally tested here

do -- change metatable to nil
	local foo = function()
		local t = {}
		setmetatable(t, {__index = function(t, k) return k end})
		local ret1 = t[123456]
		setmetatable(t, nil)
		local ret2 = t[123456]
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 123456)
	assert(ret2 == nil)
end

do -- more results than returns (var1)
	local foo = function()
		local t = {}
		local ret1, ret2 = setmetatable(t, {__index = function(t, k) return 42 end})
		return t, ret1, ret2
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == ret2)
	assert(ret3 == nil)
end

do -- more results than returns (var2)
	local foo = function()
		local t = {}
		local ret1, ret2, ret3 = setmetatable(t, {__index = function(t, k) return 42 end})
		return t, ret1, ret2, ret3
	end

	local ret1, ret2, ret3, ret4 = cinterpcall(foo)
	assert(ret1 == ret2)
	assert(ret3 == nil)
	assert(ret4 == nil)
end

do -- no args for setmetatable
	local no_args = function()
		setmetatable()
	end

	local caller = function()
		local ok, msg = pcall(no_args)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'setmetatable' (table expected, got no value)")

end
