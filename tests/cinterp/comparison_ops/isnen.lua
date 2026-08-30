-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISNEN (num, const num): n1 == n2
	local isnen_equal_nums = function()
		local n = 42
		return (n ~= 42)
	end

	assert(cinterpcall(isnen_equal_nums) == false)
end

do -- ISNEN (num, const num): n1 != n2
	local isnen_not_equal_nums = function()
		local n = 42
		return (n ~= 43)
	end

	assert(cinterpcall(isnen_not_equal_nums) == true)
end

do -- ISNEN (string, const num): always true
	local isnen_string = function()
		local str = "42"
		return (str ~= 42)
	end

	assert(cinterpcall(isnen_string) == true)
end
