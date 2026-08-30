-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

-- this test is similar to callm.lua except tailcalls

local baz = function() end -- RET0

local baz1 = function()
	return 42 -- RET1
end

local bazm = function()
	return 42, 'test', false, true -- RET
end

local bazv = function(...)
	return ... -- RETM
end

-------------------
-- various calls --
-------------------
do -- no params/returns
	local bar = function() end

	local foo = function()
		return bar(baz1())
	end

	assert(cinterpcall(foo) == nil)
end

do -- one arg
	local bar = function(arg1)
		return arg1
	end

	do
		local foo = function()
			return bar(baz())
		end

		assert(cinterpcall(foo) == nil)
	end

	do
		local foo = function()
			return bar(baz1())
		end

		assert(cinterpcall(foo) == 42)
	end

	do
		local foo = function()
			return bar(bazm())
		end

		assert(cinterpcall(foo) == 42)
	end

	do
		local foo = function()
			return bar(bazv(42, 'test', false))
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
			return bar(baz())
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == nil)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			return bar(baz1()) -- one arg missing
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			return bar(bazm()) -- 2 extra args
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == 'test')
	end

	do
		local foo = function()
			return bar(bazv(123, false, 'extra_arg'))
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 123)
		assert(ret2 == false)
	end
end

do -- return from vararg to vararg (v1)
	local baz = function(arg)
		return arg
	end

	local bar = function(...)
		return baz(...)
	end

	local foo = function(...)
		return bar(...)
	end

	local ret = cinterpcall(foo, 42)
	assert(ret == 42)
end

do -- return from vararg to vararg (v2)
	local bar = function(...)
		return ...
	end

	local foo = function(...)
		return bar(...)
	end

	local ret1, ret2 = cinterpcall(foo, 42, 'test')
	assert(ret1 == 42)
	assert(ret2 == 'test')
end

do -- return from tail-called ffunc to vararg
	local bar = function(...)
		return tostring(...)
	end

	local foo = function(...)
		return bar(...)
	end

	local ret = cinterpcall(foo, 42)
	assert(ret == '42')
end

-----------------------
-- __call metamethod --
-----------------------
do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		return t(baz())
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key + arg1 end})
		return t(baz1())
	end

	assert(cinterpcall(foo) == 84)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1, arg2, arg3) return t.key + arg1, arg2, arg3 end})
		return t(bazm())
	end

	local ret1, ret2, ret3 = cinterpcall(foo)
	assert(ret1 == 84)
	assert(ret2 == 'test')
	assert(ret3 == false)
end

do
	local foo = function()
		local t = {key = 'data'}
		setmetatable(t, {__call = function(t, arg1, arg2, arg3) return t.key, arg1, arg2, arg3 end})
		return t(bazv(42, 'test', false, true)) -- 1 extra arg
	end

	local ret1, ret2, ret3, ret4, ret5 = cinterpcall(foo)
	assert(ret1 == 'data')
	assert(ret2 == 42)
	assert(ret3 == 'test')
	assert(ret4 == false)
	assert(ret5 == nil)
end

do -- global metatable
	local foo = function()
		local n = 42
		debug.setmetatable(n, {__call = function(n, arg1, arg2, arg3) return n, arg1, arg2, arg3 end})
		return n(bazv('test', false, true))
	end

	local ret1, ret2, ret3, ret4, ret5 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == 'test')
	assert(ret3 == false)
	assert(ret4 == true)
	assert(ret5 == nil)
end

do -- in case of tail-called ffunc KBASE for foo() should be set during tailcall
	local bar = function()
		return tostring(bazv(42, 'text'))
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
