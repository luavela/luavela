-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- immutable table
	local foo = function()
		local t = ujit.immutable({})
		t.k = 42
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
		str.key = 123
		return upval
	end

	assert(cinterpcall(foo) == 123)
end

do -- no key, no __newindex
	local foo = function()
		local tab = {k1 = 'test'}
		tab.k2 = 'new'
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 2)
	assert(ret.k1 == 'test')
	assert(ret.k2 == 'new')
end

do -- no key, has __newindex
	local foo = function()
		local tab = {k1 = 'test'}
		setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 'overridden_value') end})
		tab.k2 = 'new'
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 2)
	assert(ret.k1 == 'test')
	assert(ret.k2 == 'overridden_value')
end

do -- key is nil, no __newindex
	local foo = function()
		local tab = {k1 = nil}
		tab.k1 = 'new'
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 1)
	assert(ret.k1 == 'new')
end

do -- key is nil, has __newindex
	local foo = function()
		local tab = {k1 = nil}
		setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 'overridden_value') end})
		tab.k1 = 'new'
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 1)
	assert(ret.k1 == 'overridden_value')
end

do -- key is present, no __newindex
	local foo = function()
		local tab = {k1 = 'test'}
		tab.k1 = 'new'
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 1)
	assert(ret.k1 == 'new')
end

do -- key is present, has __newindex (ignored)
	local foo = function()
		local tab = {k1 = 'test'}
		setmetatable(tab, {__newindex = function(t, k, v) rawset(t, k, 'overridden_value') end})
		tab.k1 = 'new'
		return tab
	end

	local ret = cinterpcall(foo)
	assert(ujit.table.size(ret) == 1)
	assert(ret.k1 == 'new')
end
