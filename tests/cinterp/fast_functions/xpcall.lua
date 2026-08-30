-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

local errf = function(msg)
	return "Caught an error: " .. msg
end

------------------------
-- without exceptions --
------------------------

do -- no args
	local bar = function() end

	local foo = function()
		local ok, ret = xpcall(bar, errf)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == nil)
end

do -- one arg
	local bar = function(arg1)
		return arg1 + 1
	end

	local foo = function()
		local ok, ret = xpcall(bar, errf, 41)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == 42)
end

do -- two args
	local bar = function(arg1, arg2)
		return arg1 + 1, arg2 + 1
	end

	local foo = function()
		local ok, ret1, ret2 = xpcall(bar, errf, 41, 99)
		return ok, ret1, ret2
	end

	local ok, ret1, ret2 = cinterpcall(foo)
	assert(ok == true)
	assert(ret1 == 42)
	assert(ret2 == 100)
end

do -- missing args
	local bar = function(arg1, arg2)
		return arg1, arg2
	end

	local foo = function()
		local ok, ret1, ret2 = xpcall(bar, errf)
		return ok, ret1, ret2
	end

	local ok, ret1, ret2 = cinterpcall(foo)
	assert(ok == true)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

---------------------
-- with exceptions --
---------------------

do -- try to index a number variable
	local bar = function()
		local num = 42
		num[123] = 456 -- exception
	end

	local foo = function()
		local ok, msg = xpcall(bar, errf)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "attempt to index local 'num' (a number value)")
	-- custom message from error handler
	assert(msg:sub(1, 16) == "Caught an error:")
end

do -- function is missing
	local baz = function() end

	local bar = function()
		xpcall(baz)
	end

	local foo = function()
		local ok, msg = pcall(bar)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #2 to 'xpcall' (function expected, got no value)")
end

do -- non-callable argument should be handled
	local foo = function()
		local ok, ret = xpcall("str", errf)
		return ok, ret
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert(msg:sub(1, 16) == "Caught an error:")
	assert_ends_with(msg, "attempt to call a string value")
end

do -- same as above but with tail call to C frame
	local foo = function()
		return xpcall("str", errf)
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert(msg:sub(1, 16) == "Caught an error:")
	assert_ends_with(msg, "attempt to call a string value")
end

-----------------------------------
---- exceptions with __call mm ----
-----------------------------------

do -- metamethod should be invoked and trigger an exception
	local foo = function()
		local tab = {}
		setmetatable(tab, {
			__call = function() assert(false) end
		})

		local ok, msg = xpcall(tab, errf)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert(msg:sub(1, 16) == "Caught an error:")
	assert_ends_with(msg, "assertion failed!")
end

do -- same as above but with tail call to C frame
	local foo = function()
		local tab = {}
		setmetatable(tab, {
			__call = function() assert(false) end
		})

		return xpcall(tab, errf)
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert(msg:sub(1, 16) == "Caught an error:")
	assert_ends_with(msg, "assertion failed!")
end

do -- metamethod should be invoked and return correct result
	local foo = function()
		local tab = {}
		setmetatable(tab, {
			__call = function() return 42 end
		})

		local ok, ret = xpcall(tab, errf)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == 42)
end

do -- same as above but with tail call to C frame
	local foo = function()
		local tab = {}
		setmetatable(tab, {
			__call = function() return 42 end
		})

		return xpcall(tab, errf)
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == 42)
end
