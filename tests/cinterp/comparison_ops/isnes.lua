-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISNES (string, const str): str1 == str2
	local isnes_equal_strings = function()
		local str = "hello"
		return (str ~= "hello")
	end

	assert(cinterpcall(isnes_equal_strings) == false)
end

do -- ISNES (string, const str): str1 != str2
	local isnes_not_equal_strings = function()
		local str = "hello"
		return (str ~= "world")
	end

	assert(cinterpcall(isnes_not_equal_strings) == true)
end

do -- ISNES (number, const str): always true
	local isnes_num = function()
		local num = 42
		return (num ~= "world")
	end

	assert(cinterpcall(isnes_num) == true)
end
