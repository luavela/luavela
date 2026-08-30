-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

-------------------
-- various calls --
-------------------
do -- no params/returns
	local bar = function() end

	do
		local foo = function()
			local ret = bar()
			return ret
		end

		assert(cinterpcall(foo) == nil)
	end

	do
		local foo = function()
			local ret = bar(42, 'test')
			return ret
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
			local ret = bar()
			return ret
		end

		assert(cinterpcall(foo) == nil)
	end

	do
		local foo = function()
			local ret = bar(42)
			return ret
		end

		assert(cinterpcall(foo) == 42)
	end

	do
		local foo = function()
			local ret = bar(42, 'extra_arg')
			return ret
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
			local ret1, ret2 = bar() -- missing args
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == nil)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			local ret1, ret2 = bar(42) -- one arg missing
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			local ret1, ret2 = bar(42, 'test')
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == 'test')
	end

	do
		local foo = function()
			local ret1, ret2 = bar(42, 'test', 'extra_arg')
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == 'test')
	end
end

-----------------------
-- __call metamethod --
-----------------------
do
	local foo = function()
		local t = {}
		setmetatable(t, {__call = function() end})
		local ret = t()
		return ret
	end

	assert(cinterpcall(foo) == nil)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t) return t.key end})
		local ret = t()
		return ret
	end

	assert(cinterpcall(foo) == 42)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		local ret1, ret2 = t() -- arg1 is missing
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		local ret1, ret2 = t('test')
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == 'test')
end

do
	local foo = function()
		local t = {key = 'data'}
		setmetatable(t, {__call = function(t, arg1) return t.key, arg1 end})
		local ret1, ret2 = t(false, true, nil, {}) -- more args than needed
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 'data')
	assert(ret2 == false)
end

do -- global metatable
	local foo = function()
		local n = 42
		debug.setmetatable(n, {__call = function(n, arg1) return n + arg1 end})
		local ret = n(10)
		return ret
	end

	assert(cinterpcall(foo) == 52)
end

----------------
-- exceptions --
----------------

do
	local bar = function()
		local t = {}
		t()
	end

	local foo = function()
		local ok, ret = pcall(bar)
		return ok, ret
	end

	local ok, ret = cinterpcall(foo)
	assert(ok == false)
	assert_ends_with(ret, "attempt to call local 't' (a table value)")
end
