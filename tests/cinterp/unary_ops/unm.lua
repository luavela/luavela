-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- no metamethod
	local foo = function()
		local n = 42
		return -n
	end

	assert(cinterpcall(foo) == -42)
end

do -- with metamethod
	local foo = function()
		local t = {n = 42}
		local mt = {__unm = function(tab) return -tab.n end}
		setmetatable(t, mt)
		return -t
	end

	assert(cinterpcall(foo) == -42)
end
