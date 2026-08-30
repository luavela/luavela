-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- argument is not a table
	local foo = function()
		local str = 'string'
		debug.setmetatable(str, {__index = function(t, k) return 42 end})
		return str[1]
	end

	assert(cinterpcall(foo) == 42)
end

do -- index not in array part, no __index
	local foo = function()
		local tab = {11, 22, 33}
		return tab[100]
	end

	assert(cinterpcall(foo) == nil)
end

do -- index not in array part, has __index
	local foo = function()
		local tab = {11, 22, 33}
		setmetatable(tab, {__index = function(t, k) return 42 end})
		return tab[100]
	end

	assert(foo() == 42)
end

do -- index fits array part and is nil, no __index (var 1)
	local foo = function()
		local tab = {11, nil, 33}
		return tab[2]
	end

	assert(foo() == nil)
end

do -- index fits array part and is nil, has __index (var 1)
	local foo = function()
		local tab = {11, nil, 33}
		setmetatable(tab, {__index = function(t, k) return 42 end})
		return tab[2]
	end

	assert(foo() == 42)
end

do -- index fits array part and is nil, no __index (var 2)
	local foo = function()
		local tab = {11, 22, 33} -- array size = 5
		return tab[4]
	end

	assert(foo() == nil)
end

do -- index fits array part and is nil, has __index (var 2)
	local foo = function()
		local tab = {11, 22, 33} -- array size = 5
		setmetatable(tab, {__index = function(t, k) return 42 end})
		return tab[4]
	end

	assert(foo() == 42)
end
