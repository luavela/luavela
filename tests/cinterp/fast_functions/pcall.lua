-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

------------------------
-- without exceptions --
------------------------

do -- no args
	local bar = function() end

	local foo = function()
		local ok, ret = pcall(bar)
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
		local ok, ret = pcall(bar, 41)
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
		local ok, ret1, ret2 = pcall(bar, 41, 99)
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
		local ok, ret1, ret2 = pcall(bar)
		return ok, ret1, ret2
	end

	local ok, ret1, ret2 = cinterpcall(foo)
	assert(ok == true)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do -- several pcalls
	local function f(x) return x*x end

	local foo = function()
	    return pcall(pcall, f, 42)
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == true)
	assert(ret2 == true)
	assert(ret3 == 42 * 42)
end

-------------------------------------------
-- without exceptions and with __call mm --
-------------------------------------------

do -- no args
	local foo = function()
		local tab = {}
		setmetatable(tab, {__call = function() end})

		local ok, ret = pcall(tab)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == nil)
end

do -- 1 arg
	local foo = function()
		local tab = {k1 = 10}
		setmetatable(tab, {__call = function(t, arg1) return arg1 + tab.k1 end})

		local ok, ret = pcall(tab, 32)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == 42)
end

do -- 2 args
	local foo = function()
		local tab = {k1 = 10}
		setmetatable(tab, {__call = function(t, arg1, arg2) return arg1 + tab.k1, arg2 end})

		local ok, ret1, ret2 = pcall(tab, 32, 'data')
		return ok, ret1, ret2
	end

	local ok, ret1, ret2 = cinterpcall(foo)
	assert(ok == true)
	assert(ret1 == 42)
	assert(ret2 == 'data')
end

do -- 2 args with tail call to C frame
	local foo = function()
		local tab = {k1 = 10}
		setmetatable(tab, {__call = function(t, arg1, arg2) return arg1 + tab.k1, arg2 end})

		return pcall(tab, 32, 'data')
	end

	local ok, ret1, ret2 = cinterpcall(foo)
	assert(ok == true)
	assert(ret1 == 42)
	assert(ret2 == 'data')
end

do -- missing args
	local foo = function()
		local tab = {}
		setmetatable(tab, {__call = function(t, arg1, arg2) return t, arg1, arg2 end})

		local ok, ret1, ret2, ret3 = pcall(tab)
		return ok, ret1, ret2, ret3
	end

	local ok, ret1, ret2, ret3 = cinterpcall(foo)
	assert(ok == true)
	assert(type(ret1) == 'table')
	assert(ret2 == nil)
	assert(ret3 == nil)
end

---------------------
-- with exceptions --
---------------------

do -- try to index a number variable
	local bar = function()
		local num = 42
		num[123] = 456
	end

	local foo = function()
		local ok, msg = pcall(bar)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "attempt to index local 'num' (a number value)")
end

do -- try to index a boolean variable
	local bar = function()
		local bool = false
		bool[123] = 456
	end

	local foo = function()
		local ok, msg = pcall(bar)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "attempt to index local 'bool' (a boolean value)")
end

do -- function is missing
	local bar = function()
		pcall()
	end

	local foo = function()
		local ok, msg = pcall(bar)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'pcall' (value expected)")
end

do -- non-callable argument should be handled
	local foo = function()
		local ok, ret = pcall("str")
		return ok, ret
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert(msg == "attempt to call a string value")
end

do -- same as above but with tail call
	local foo = function()
		return pcall("str")
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert(msg == "attempt to call a string value")
end

-- exception should be correctly handled if there is a
-- chain of interpreters on host stack
do
	local f4 = function() assert(false) end
	local f3 = function() return cinterpcall(f4) end
	local f2 = function() return cinterpcall(f3) end
	local f1 = function() return pcall(f2) end

	local ok, msg = cinterpcall(f1)
	assert(ok == false)
	assert_ends_with(msg, "assertion failed!")
end

-- interpreter should be in valid state after
-- exception occurred several times
do
	local err = function() assert(false) end
	local foo = function()
		local ok1, msg1 = pcall(err)
		local ok2, msg2 = pcall(err)
		local ok3, msg3 = pcall(err)

		return ok1, msg1, ok2, msg2, ok3, msg3
	end

	local ok1, msg1, ok2, msg2, ok3, msg3 = cinterpcall(foo)
	assert(ok1 == false)
	assert(ok2 == false)
	assert(ok3 == false)
	assert_ends_with(msg1, "assertion failed!")
	assert_ends_with(msg2, "assertion failed!")
	assert_ends_with(msg3, "assertion failed!")
end

-----------------------------------
-- exceptions and with __call mm --
-----------------------------------

do -- metamethod should be invoked and trigger an exception
	local foo = function()
		local tab = {}
		setmetatable(tab, {
			__call = function() return nonexistent_variable[42] end
		})

		local ok, msg = pcall(tab, 32)
		return ok, msg
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "attempt to index global 'nonexistent_variable' (a nil value)")
end

do -- same as above but with tail call to C frame
	local foo = function()
		local tab = {}
		setmetatable(tab, {
			__call = function() return nonexistent_variable[42] end
		})

		return pcall(tab, 32)
	end

	local ok, msg = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(msg, "attempt to index global 'nonexistent_variable' (a nil value)")
end
