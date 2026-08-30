-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- non-nil global value
	get_global1 = 'global_var'

	local foo = function()
		local var = get_global1
		return var
	end

	assert(cinterpcall(foo) == 'global_var')
end

do -- global value is missing, no metatable
	local foo = function()
		return bad_global_var1
	end

	assert(cinterpcall(foo) == nil)
end

do -- global value is missing, redefined by __index
	local foo = function()
		setmetatable(_G, {__index = function(t, k) return 'override1' end})
		return bad_global_var2
	end

	assert(cinterpcall(foo) == 'override1')
end

do -- global value is nil, no metatable
	local foo = function()
		setmetatable(_G, nil)
		get_global2 = nil
		return get_global2
	end

	assert(cinterpcall(foo) == nil)
end

do -- global value is nil, has __index metamethod
	local foo = function()
		get_global3 = nil
		setmetatable(_G, {__index = function(t, k) return 'override2' end})
		return get_global3
	end

	assert(cinterpcall(foo) == 'override2')
end

do -- resolve metamethod in function env table
	global_func = function(arg) return arg + 1 end

	local foo = function()
		local newgt = {}
		setmetatable(newgt, {__index = _G})
		setfenv(1, newgt)
		return global_func(42)
	end

	assert(cinterpcall(foo) == 43)
end
