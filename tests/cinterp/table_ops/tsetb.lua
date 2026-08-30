-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- immutable table
	local foo = function()
		local t = ujit.immutable({})
		t[1] = 42
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
		str[1] = 1234
		return upval
	end

	assert(cinterpcall(foo) == 1234)
end

do -- index in array part, no __newindex
	local foo = function()
		local tab = {11, 22, 33}
		tab[3] = 42
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
		tab[3] = 42
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
		tab[1] = 42
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
		setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, v) end})
		tab[1] = 42
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 3)
	assert(ret[1] == 42)
	assert(ret[2] == 22)
	assert(ret[3] == 'test')
end

do -- index outside array part, no __newindex
	local foo = function()
		local tab = {false, 22, 'test'}
		tab[100] = 12345
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
		tab[100] = 42
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 4)
	assert(ret[1] == true)
	assert(ret[2] == 123)
	assert(ret[3] == 'test')
	assert(ret[100] == 'test2')
end
