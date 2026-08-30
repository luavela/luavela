-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- argument is not a table
	local foo = function()
		local str = 'not a table'
		debug.setmetatable(str, {__index = function(t, k) return 42 end})
		return str.key
	end

	assert(cinterpcall(foo) == 42)
end

do -- key is present, no __index
	local foo = function()
		local tab = {k1 = 'test'}
		return tab.k1
	end

	assert(cinterpcall(foo) == 'test')
end

do -- key is present, has __index (ignored)
	local foo = function()
		local tab = {k1 = 'test'}
		setmetatable(tab, {__index = function(t, k) return 42 end})
		return tab.k1
	end

	assert(cinterpcall(foo) == 'test')
end

do -- key is present and nil, no __index
	local foo = function()
		local tab = {k1 = 'test', k2 = nil}
		return tab.k2
	end

	assert(cinterpcall(foo) == nil)
end

do -- key is present and nil, has __index
	local foo = function()
		local tab = {k1 = 'test', k2 = nil}
		setmetatable(tab, {__index = function(t, k) return 42 end})
		return tab.k2
	end

	assert(cinterpcall(foo) == 42)
end

do -- no key, no __index
	local foo = function()
		local tab = {k1 = 'test', k2 = 123}
		return tab.bad_key
	end

	assert(cinterpcall(foo) == nil)
end

do -- no key, has __index
	local foo = function()
		local tab = {k1 = 'test', k2 = 123}
		setmetatable(tab, {__index = function(t, k) return 42 end})
		return tab.bad_key
	end

	assert(cinterpcall(foo) == 42)
end
