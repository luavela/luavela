-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- set global var, no __newindex
	local foo = function()
		setmetatable(_G, nil)
		set_global1 = 'test'
		return set_global1
	end

	assert(cinterpcall(foo) == 'test')
end

do -- set global var, with __newindex
	local foo = function()
		setmetatable(_G, {__newindex = function(t, k, v) rawset(t, k, 'override1') end})
		set_global2 = 'test'
		return set_global2
	end

	assert(cinterpcall(foo) == 'override1')
end

do -- overwrite nil global variable, no __newindex
	local foo = function()
		setmetatable(_G, nil)
		set_global3 = nil
		set_global3 = 'test'
		return set_global3
	end

	assert(cinterpcall(foo) == 'test')
end

do -- overwrite nil global variable, has __newindex
	local foo = function()
		setmetatable(_G, nil)
		set_global4 = nil
		setmetatable(_G, {__newindex = function(t, k, v) rawset(t, k, 'override2') end})
		set_global4 = 'test'
		return set_global4
	end

	assert(cinterpcall(foo) == 'override2')
end

do -- overwrite non-nil global variable, no __newindex
	local foo = function()
		setmetatable(_G, nil)
		set_global5 = 'data'
		set_global5 = 'test'
		return set_global5
	end

	assert(cinterpcall(foo) == 'test')
end

do -- overwrite non-nil global variable, __newindex should be ignored
	local foo = function()
		setmetatable(_G, nil)
		set_global6 = 'my_val'
		setmetatable(_G, {__newindex = function(t, k, v) rawset(t, k, 'override3') end})
		set_global6 = 'test'
		return set_global6
	end

	assert(cinterpcall(foo) == 'test')
end

do -- immutable global table (placed at the end since it will make _G immutable)
	local foo = function()
		some_global_var = 42
	end

	local caller = function()
		ujit.immutable(_G)
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "attempt to modify an immutable object")
end
