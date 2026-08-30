-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

-- this test is similar to call.lua except tailcalls

-------------------
-- various calls --
-------------------
do -- no params/returns
	local bar = function() end

	do
		local foo = function()
			return bar()
		end

		assert(cinterpcall(foo) == nil)
	end

	do
		local foo = function()
			return bar(42, 'test')
		end

		assert(cinterpcall(foo) == nil)
	end
end

do -- one arg
	local bar = function(arg1)
		return arg1
	end

	do
		local foo = function()
			return bar()
		end

		assert(cinterpcall(foo) == nil)
	end

	do
		local foo = function()
			return bar(42)
		end

		assert(cinterpcall(foo) == 42)
	end

	do
		local foo = function()
			return bar(42, 'extra_arg')
		end

		assert(cinterpcall(foo) == 42)
	end
end

do -- two args
	local bar = function(arg1, arg2)
		return arg1, arg2
	end

	do
		local foo = function()
			return bar() -- missing args
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == nil)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			return bar(42) -- one arg missing
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			return bar(42, 'test')
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == 'test')
	end

	do
		local foo = function()
			return bar(42, 'test', 'extra_arg')
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == 'test')
	end
end

do -- return from fixarg to vararg
	local bar = function(arg1, arg2)
		return arg1, arg2
	end

	local foo = function(...)
		local arg1, arg2 = ...
		return bar(arg1, arg2)
	end

	local ret1, ret2 = cinterpcall(foo, 42, 43)
	assert(ret1 == 42)
	assert(ret2 == 43)
end

do -- return from tail-called ffunc to vararg
	local bar = function(arg)
		return tostring(arg)
	end

	local foo = function(...)
		return bar(42)
	end

	local ret = cinterpcall(foo, 42, 43)
	assert(ret == '42')
end

-----------------------
-- __call metamethod --
-----------------------
do
	local foo = function()
		local t = {}
		setmetatable(t, {__call = function() end})
		return t()
	end

	assert(cinterpcall(foo) == nil)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t) return t.key end})
		return t()
	end

	assert(cinterpcall(foo) == 42)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		return t() -- arg1 is missing
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		return t('test')
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == 'test')
end

do
	local foo = function()
		local t = {key = 'data'}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		return t(false, true, nil, {}) -- more args than needed
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 'data')
	assert(ret2 == false)
end

do -- global metatable
	local foo = function()
		local n = 42
		debug.setmetatable(n, {__call = function(n, arg1) return n + arg1 end})
		return n(10)
	end

	assert(cinterpcall(foo) == 52)
end

do -- in case of tail-called ffunc KBASE for foo() should be set during tailcall
	local bar = function()
		return tostring(42)
	end

	local foo = function()
		local ret = bar()
		-- KBASE should be setup correctly for foo() after tail-called return from bar()
		local str = 'some text'
		return ret, str
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == '42')
	assert(ret2 == 'some text')
end
