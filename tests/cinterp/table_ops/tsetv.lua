-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- immutable table
	local foo = function()
		local key = 'k'
		local t = ujit.immutable({})
		t[key] = 42
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "attempt to modify an immutable object")
end

do -- argument is not a table
	local foo = function()
		local str = 'not a table'
		local upval
		debug.setmetatable(str, {__newindex = function(t, k, v) upval = v end})
		local k = 'key'
		str[k] = 123
		return upval
	end

	assert(cinterpcall(foo) == 123)
end

do -- cases when key is string
	do -- key is present, no __newindex
		local foo = function()
			local tab = {k1 = 'test'}
			local k = 'k1'
			tab[k] = 123
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 1)
		assert(ret.k1 == 123)
	end

	do -- key is present, has __newindex (ignored)
		local foo = function()
			local tab = {k1 = 'test'}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 2024) end})
			local k = 'k1'
			tab[k] = 123
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 1)
		assert(ret.k1 == 123)
	end

	do -- key is not present, no __newindex
		local foo = function()
			local tab = {k1 = 'test'}
			local k = 'new_key'
			tab[k] = 123
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 2)
		assert(ret.k1 == 'test')
		assert(ret.new_key == 123)
	end

	do -- key is not present, has __newindex
		local foo = function()
			local tab = {k1 = 'test'}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 2024) end})
			local k = 'new_key'
			tab[k] = 123
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 2)
		assert(ret.k1 == 'test')
		assert(ret.new_key == 2024)
	end
end

do -- cases when key is number
	do -- index in array part, no __newindex
		local foo = function()
			local tab = {11, 22, 33}
			local k = 3
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 3)
		assert(ret[1] == 11)
		assert(ret[2] == 22)
		assert(ret[3] == 42)
	end

	do -- index in array part, has __newindex (ignored)
		local foo = function()
			local tab = {false, 22, 'test'}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 2024) end})
			local k = 3
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 3)
		assert(ret[1] == false)
		assert(ret[2] == 22)
		assert(ret[3] == 42)
	end

	do -- index in array part and is nil, no __newindex
		local foo = function()
			local tab = {nil, 22, 'test'}
			local k = 1
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 3)
		assert(ret[1] == 42)
		assert(ret[2] == 22)
		assert(ret[3] == 'test')
	end

	do -- index in array part and is nil, has __newindex
		local foo = function()
			local tab = {nil, 22, 'test'}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, v + 1) end})
			local k = 1
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 3)
		assert(ret[1] == 43)
		assert(ret[2] == 22)
		assert(ret[3] == 'test')
	end

	do -- index outside array part, no __newindex
		local foo = function()
			local tab = {false, 22, 'test'}
			local k = 100
			tab[k] = 12345
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[1] == false)
		assert(ret[2] == 22)
		assert(ret[3] == 'test')
		assert(ret[100] == 12345)
	end

	do -- index outside array part, has __newindex
		local foo = function()
			local tab = {true, 123, 'test'}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 'test2') end})
			local k = 100
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[1] == true)
		assert(ret[2] == 123)
		assert(ret[3] == 'test')
		assert(ret[100] == 'test2')
	end

	do -- 0.0 index, no metatable
		local foo = function()
			local tab = {11, 22, 33}
			local k = 0.0
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[0] == 42)
		assert(ret[1] == 11)
		assert(ret[2] == 22)
		assert(ret[3] == 33)
	end

	do -- 0.0 index, with metatable
		local foo = function()
			local tab = {11, 22, 33}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 2024) end})
			local k = 0.0
			tab[k] = 'val'
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[0] == 2024)
		assert(ret[1] == 11)
		assert(ret[2] == 22)
		assert(ret[3] == 33)
	end

	do -- -0.0 index, no metatable (should be treated as 0.0)
		local foo = function()
			local tab = {11, 22, 33}
			local k = -0.0
			tab[k] = 42
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[0] == 42)
		assert(ret[1] == 11)
		assert(ret[2] == 22)
		assert(ret[3] == 33)
	end

	do -- -0.0 index, with metatable (should be treated as 0.0)
		local foo = function()
			local tab = {11, 22, 33}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 2024) end})
			local k = -0.0
			tab[k] = 'val'
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[0] == 2024)
		assert(ret[1] == 11)
		assert(ret[2] == 22)
		assert(ret[3] == 33)
	end
end

do -- key is non-integer, no __newindex
		local foo = function()
			local tab = {1, 2, 3}
			local k = 4.5
			tab[k] = 12345
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[1] == 1)
		assert(ret[2] == 2)
		assert(ret[3] == 3)
		assert(ret[4.5] == 12345)
end

do -- key is non-integer, has __newindex
		local foo = function()
			local tab = {1, 2, 3}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k + 0.5, v + 10) end})
			local k = 4.5
			tab[k] = 12345
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[1] == 1)
		assert(ret[2] == 2)
		assert(ret[3] == 3)
		assert(ret[5] == 12355)
end

do -- key is not a number or string, no __newindex
		local key = {}

		local foo = function()
			local tab = {1, 2, 3}
			tab[key] = 'test'
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[1] == 1)
		assert(ret[2] == 2)
		assert(ret[3] == 3)
		assert(ret[key] == 'test')
end

do -- key is not a number or string, has __newindex
		local key = {}

		local foo = function()
			local tab = {1, 2, 3}
			setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 'overridden_value') end})
			tab[key] = 'test'
			return tab
		end

		local ret = cinterpcall(foo)
		assert(ujit.table.size(ret) == 4)
		assert(ret[1] == 1)
		assert(ret[2] == 2)
		assert(ret[3] == 3)
		assert(ret[key] == 'overridden_value')
end
