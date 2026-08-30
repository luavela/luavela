-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISEQS (string, const str): str1 == str2
	local iseqs_equal_strings = function()
		local str = "hello"
		return (str == "hello")
	end

	assert(cinterpcall(iseqs_equal_strings) == true)
end

do -- ISEQS (string, const str): str1 != str2
	local iseqs_not_equal_strings = function()
		local str = "hello"
		return (str == "world")
	end

	assert(cinterpcall(iseqs_not_equal_strings) == false)
end

do -- ISEQS (number, const str): always false
	local iseqs_not_equal_num = function()
		local num = 42
		return (num == "world")
	end

	assert(cinterpcall(iseqs_not_equal_num) == false)
end
