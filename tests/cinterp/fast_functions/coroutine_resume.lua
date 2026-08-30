-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- no args, no yield
	local bar = function() end

	local foo = function()
		local co = coroutine.create(bar)
		local ok, ret = coroutine.resume(co)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == nil)
end

do -- no args, no yield (v2)
	local bar = function() end

	local foo = function()
		local co = coroutine.create(bar)
		coroutine.resume(co)
		local ok, ret = coroutine.resume(co)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(not ok)
	assert(ret == "cannot resume dead coroutine")
end

do -- returns arg, no yield
	local bar = function(arg) return arg end

	local foo = function()
		local co = coroutine.create(bar)
		local ok, ret = coroutine.resume(co, 42)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == true)
	assert(ret == 42)
end

do -- some yields
	local bar = function(arg)
		arg = arg + 1
		coroutine.yield(arg)
		arg = arg + 1
		coroutine.yield(arg)
		return arg + 1
	end

	local foo = function()
		local co = coroutine.create(bar)
		local ok1, ret1 = coroutine.resume(co, 42)
		local ok2, ret2 = coroutine.resume(co)
		local ok3, ret3 = coroutine.resume(co)
		local ok4, ret4 = coroutine.resume(co)
		return ok1, ret1, ok2, ret2, ok3, ret3, ok4, ret4
	end

	local ok1, ret1, ok2, ret2, ok3, ret3, ok4, ret4 = cinterpcall(foo)
	assert(ok1 == true)
	assert(ret1 == 43)
	assert(ok2 == true)
	assert(ret2 == 44)
	assert(ok3 == true)
	assert(ret3 == 45)
	assert(not ok4)
	assert(ret4 == "cannot resume dead coroutine")
end

do -- exception in coroutine
	local bar = function(arg)
		coroutine.yield(arg + 1)
		assert(false)
	end

	local foo = function()
		local co = coroutine.create(bar)
		local ok1, ret1 = coroutine.resume(co, 42)
		local ok2, ret2 = coroutine.resume(co)
		local ok3, ret3 = coroutine.resume(co)
		return ok1, ret1, ok2, ret2, ok3, ret3
	end

	local ok1, ret1, ok2, ret2, ok3, ret3 = cinterpcall(foo)
	assert(ok1 == true)
	assert(ret1 == 43)
	assert(not ok2)
	assert_ends_with(ret2, "assertion failed!")
	assert(not ok3)
	assert(ret3 == "cannot resume dead coroutine")
end

do -- trigger stack grow when a lot of results returned
	local bar = function()
		local t = {}
		for i = 1, 1000 do
			t[i] = i
		end
		assert(#t == 1000)

		-- return a lot of results and trigger stack
		-- reallocation of caller coroutine
		coroutine.yield(unpack(t))
		coroutine.yield(unpack(t))
	end

	local foo = function()
		local co = coroutine.create(bar)
		local ret1 = {coroutine.resume(co)}
		-- just obtain a lot of results one more time
		local ret2 = {coroutine.resume(co)}
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(#ret1 == 1001)
	assert(#ret2 == 1001)
	assert(ret1[1] == true)
	assert(ret2[1] == true)

	for i = 1, 1000 do
		assert(ret1[i + 1] == i)
		assert(ret2[i + 1] == i)
	end
end

do -- coroutine_resume should return correct number of results in case of pcall
	local coroutine_func = function()
		return 42, 43, 44, 45
	end

	local foo = function()
		local co = coroutine.create(coroutine_func)
		return pcall(coroutine.resume, co)
	end

	local ret1, ret2, ret3, ret4, ret5, ret6, ret7 = cinterpcall(foo)
	assert(ret1 == true)
	assert(ret2 == true)
	assert(ret3 == 42)
	assert(ret4 == 43)
	assert(ret5 == 44)
	assert(ret6 == 45)
	assert(ret7 == nil)
end
