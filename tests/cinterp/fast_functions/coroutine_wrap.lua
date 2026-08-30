-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- no args
	local bar = function() end

	local foo = function()
		local wrap = coroutine.wrap(bar)
		return wrap()
	end

	assert(cinterpcall(foo) == nil)
end

do -- no args with yields
	local bar = function()
		coroutine.yield()
		coroutine.yield()
	end

	local foo = function()
		local wrap = coroutine.wrap(bar)
		local ret1 = wrap()
		local ret2 = wrap()
		local ret3 = wrap()
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == nil)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do -- one arg
	local bar = function(arg) return arg + 1 end

	local foo = function()
		local wrap = coroutine.wrap(bar)
		return wrap(42)
	end

	assert(cinterpcall(foo) == 43)
end

do -- one arg with yields
	local bar = function(arg)
		coroutine.yield(arg + 1)
		coroutine.yield(arg + 2)
		return arg + 3
	end

	local foo = function()
		local wrap = coroutine.wrap(bar)
		local ret1 = wrap(42)
		local ret2 = wrap()
		local ret3 = wrap()
		return ret1, ret2, ret3
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 43)
	assert(ret2 == 44)
	assert(ret3 == 45)
end

do -- error in wrapped coroutine will be propagated to caller
	local baz = function()
		assert(false)
	end

	local bar = function()
		local wrap = coroutine.wrap(baz)
		wrap()
	end

	local caller = function()
		local ok, msg = pcall(bar)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "assertion failed!")
end

do -- trigger stack grow when a lot of results returned
	local bar = function()
		local t = {}
		for i = 1, 1000 do
			t[i] = i
		end
		assert(#t == 1000)

		coroutine.yield(unpack(t))
		coroutine.yield(unpack(t))
	end

	local foo = function()
		local wrap = coroutine.wrap(bar)
		local ret1 = {wrap()}
		local ret2 = {wrap()}
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(#ret1 == 1000)
	assert(#ret2 == 1000)

	for i = 1, 1000 do
		assert(ret1[i] == i)
		assert(ret2[i] == i)
	end
end

do -- yield from another function
	local f2 = function(...)
		return coroutine.yield(...)
	end

	local f1 = function(...)
		return pcall(f2, ...)
	end

	local foo = function()
		local co = coroutine.wrap(f1)
		co("Hello")
		return co("World")
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == true)
	assert(ret2 == "World")
end

do -- coroutine_wrap should return correct number of results in case of pcall
	local coroutine_func = function()
		return 42, 43, 44, 45
	end

	local foo = function()
		local f = coroutine.wrap(coroutine_func)
		return pcall(f)
	end

	local ret1, ret2, ret3, ret4, ret5, ret6 = cinterpcall(foo)
	assert(ret1 == true)
	assert(ret2 == 42)
	assert(ret3 == 43)
	assert(ret4 == 44)
	assert(ret5 == 45)
	assert(ret6 == nil)
end
