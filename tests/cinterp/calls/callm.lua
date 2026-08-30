-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

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
		local ret = bar(baz1())
		return ret
	end

	assert(cinterpcall(foo) == nil)
end

do -- one arg
	local bar = function(arg1)
		return arg1
	end

	do
		local foo = function()
			local ret = bar(baz())
			return ret
		end

		assert(cinterpcall(foo) == nil)
	end

	do
		local foo = function()
			local ret = bar(baz1())
			return ret
		end

		assert(cinterpcall(foo) == 42)
	end

	do
		local foo = function()
			local ret = bar(bazm())
			return ret
		end

		assert(cinterpcall(foo) == 42)
	end

	
	do
		local foo = function()
			local ret = bar(bazv(42, 'test', false))
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
			local ret1, ret2 = bar(baz())
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == nil)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			local ret1, ret2 = bar(baz1()) -- one arg missing
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == nil)
	end

	do
		local foo = function()
			local ret1, ret2 = bar(bazm()) -- 2 extra args
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 42)
		assert(ret2 == 'test')
	end

	do
		local foo = function()
			local ret1, ret2 = bar(bazv(123, false, 'extra_arg'))
			return ret1, ret2
		end

		local ret1, ret2 = cinterpcall(foo)
		assert(ret1 == 123)
		assert(ret2 == false)
	end
end

-----------------------
-- __call metamethod --
-----------------------
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
		local ret1, ret2 = t(baz())
		return ret1, ret2
	end

	local ret1, ret2 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1) return t.key + arg1 end})
		local ret1, ret2 = t(baz1())
		return ret1, ret2
	end

	assert(cinterpcall(foo) == 84)
end

do
	local foo = function()
		local t = {key = 42}
		setmetatable(t, {__call = function(t, arg1, arg2, arg3) return t.key + arg1, arg2, arg3 end})
		local ret1, ret2, ret3 = t(bazm())
		return ret1, ret2, ret3
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
		local ret1, ret2, ret3, ret4, ret5 = t(bazv(42, 'test', false, true)) -- 1 extra arg
		return ret1, ret2, ret3, ret4, ret5
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
		local ret1, ret2, ret3, ret4, ret5 = n(bazv('test', false, true))
		return ret1, ret2, ret3, ret4, ret5
	end

	local ret1, ret2, ret3, ret4, ret5 = cinterpcall(foo)
	assert(ret1 == 42)
	assert(ret2 == 'test')
	assert(ret3 == false)
	assert(ret4 == true)
	assert(ret5 == nil)
end
