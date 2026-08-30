-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

do -- tostring(nil)
	local tostring_nil = function()
		local str = tostring(nil)
		return str
	end

	local ret = cinterpcall(tostring_nil)
	assert(ret == "nil")
end

do -- tostring()
	local tostring_no_args = function()
		local str = tostring()
		return str
	end

	local caller = function()
		local ok, msg = pcall(tostring_no_args)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'tostring' (value expected)")
end

do -- tostring(false)
	local tostring_false = function()
		local str = tostring(false)
		return str
	end

	local ret = cinterpcall(tostring_false)
	assert(ret == "false")
end

do -- tostring(true)
	local tostring_true = function()
		local str = tostring(true)
		return str
	end

	local ret = cinterpcall(tostring_true)
	assert(ret == "true")
end

do -- tostring(num, int)
	local tostring_num_integer = function()
		local str = tostring(12345)
		return str
	end

	local ret = cinterpcall(tostring_num_integer)
	assert(ret == "12345")
end

-- NOTE: there are a lot more cases for tostring (nan, inf etc.)
-- but they're covered in other test suites.

do -- tostring(num, double)
	local tostring_num_double = function()
		local str = tostring(42.5)
		return str
	end

	local ret = cinterpcall(tostring_num_double)
	assert(ret == "42.5")
end

do -- tostring("")
	local tostring_str_empty = function()
		local str = tostring("")
		return str
	end

	local ret = cinterpcall(tostring_str_empty)
	assert(ret == "")
end

do -- tostring(str)
	local tostring_str = function()
		local str = tostring("hello")
		return str
	end

	local ret = cinterpcall(tostring_str)
	assert(ret == "hello")
end

local starts_with =	function(self, substring)
		return (self:sub(1,#substring) == substring)
end

do -- tostring(table)
	local tostring_table = function()
		local str = tostring({})
		return str
	end

	local ret = cinterpcall(tostring_table)
	assert(starts_with(ret, "table: 0x"))
end

do -- tostring(function)
	local tostring_table = function()
		local f = function() end
		local str = tostring(f)
		return str
	end

	local ret = cinterpcall(tostring_table)
	assert(starts_with(ret, "function: 0x"))
end

do -- tostring(coroutine)
	local tostring_table = function()
		local coro = coroutine.create(function() end)
		local str = tostring(coro)
		return str
	end

	local ret = cinterpcall(tostring_table)
	assert(starts_with(ret, "thread: 0x"))
end

do -- tostring() throws error when called without arguments
	local tostring_err = function()
		local str = tostring()
		return str
	end

	local caller = function()
		local ok, msg = pcall(tostring_err)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'tostring' (value expected)")
end

do -- metatable ignored for strings
	local tostring_str_mt = function()
		local mt = {
			__tostring = function() return "metatable" end
		}

		local str = "original"
		debug.setmetatable(str, mt)

		local str = tostring(str)
		return str
	end

	local ret = cinterpcall(tostring_str_mt)
	assert(ret == "original")
end

do -- numbers with metatable
	local tostring_str_num = function()
		local mt = {
			__tostring = function(v) return "metatable" end
		}

		local num = 42
		debug.setmetatable(num, mt)

		local str = tostring(num)
		debug.setmetatable(num, nil)

		return str
	end

	local ret = cinterpcall(tostring_str_num)
	assert(ret == "metatable")
end

do -- table with metatable
	local tostring_str_tab = function()
		local mt = {
			__tostring = function(v) return "metatable" end
		}

		local t = {}
		setmetatable(t, mt)

		local str = tostring(t)
		return str
	end

	local ret = cinterpcall(tostring_str_tab)
	assert(ret == "metatable")
end

do -- more results than returns (var1)
	local foo = function()
		local ret1, ret2 = tostring(42)
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == "42")
	assert(ret2 == nil)
end

do -- more results than returns (var2)
	local foo = function()
		local ret1, ret2, ret3 = tostring(42)
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == "42")
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- more results than returns with metamethod (var1)
	local tostring_str_tab = function()
		local t = {}
		setmetatable(t, {__tostring = function(v) return "metatable" end})

		local ret1, ret2 = tostring(t)
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(tostring_str_tab)
	assert(ret1 == "metatable")
	assert(ret2 == nil)
end

do -- more results than returns with metamethod (var2)
	local tostring_str_tab = function()
		local t = {}
		setmetatable(t, {__tostring = function(v) return "metatable" end})

		local ret1, ret2, ret3 = tostring(t)
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(tostring_str_tab)
	assert(ret1 == "metatable")
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- metamethod's missing arguments must be set to nil
	local foo = function()
		local t = {}
		local mm_tostring = function(val, arg1, arg2, arg3, arg4, arg5, arg6)
			return "test", arg1, arg2, arg3, arg4, arg5, arg6
		end

		setmetatable(t, {__tostring = mm_tostring})
		local ret1, ret2, ret3, ret4, ret5, ret6, ret7 = tostring(t)
		return t, ret1, ret2, ret3, ret4, ret5, ret6, ret7
	end

	local t, ret1, ret2, ret3, ret4, ret5, ret6, ret7 = cinterpcall(foo)
	assert(type(t) == "table")
	assert(ret1 == "test")
	assert(ret2 == nil)
	assert(ret3 == nil)
	assert(ret4 == nil)
	assert(ret5 == nil)
	assert(ret6 == nil)
	assert(ret7 == nil)
end

do -- tailcall back to C frame
	local foo = function()
		local t = {}
		local mt = {__tostring = function() return "custom tostring" end}
		setmetatable(t, mt)
		return tostring(t)
	end

	local ret = cinterpcall(foo)
	assert(ret == "custom tostring")
end

do -- tailcall back to C frame w/ __call metamethod
	local foo = function()
		local t1 = {}
		local t1_mt = {__call = function() return "custom tostring" end}
		setmetatable(t1, t1_mt)

		local t2 = {}
		local t2_mt = {__tostring = t1}
		setmetatable(t2, t2_mt)
		return tostring(t2)
	end

	local ret = cinterpcall(foo)
	assert(ret == "custom tostring")
end

do -- tailcall to fixarg function
	local bar = function()
		local t = {}
		local mt = {__tostring = function() return "custom tostring" end}
		setmetatable(t, mt)
		return tostring(t)
	end

	local foo = function()
		return bar()
	end

	local ret = cinterpcall(bar)
	assert(ret == "custom tostring")
end

do -- tailcall to fixarg function w/ __call metamethod
	local bar = function()
		local t1 = {}
		local t1_mt = {__call = function() return "custom tostring" end}
		setmetatable(t1, t1_mt)

		local t2 = {}
		local t2_mt = {__tostring = t1}
		setmetatable(t2, t2_mt)
		return tostring(t2)
	end

	local foo = function()
		return bar()
	end

	local ret = cinterpcall(foo)
	assert(ret == "custom tostring")
end

-- TODO: userdata, cdata
-- TODO: test more errors
