-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISLT (number, number): n1 < n2
	local islt_n1_less = function()
		local n1 = 41
		local n2 = 42
		local res = (n1 < n2)
		return res
	end

	assert(cinterpcall(islt_n1_less) == true)
end

do -- ISLT (number, number): n1 == n2
	local islt_nums_equal = function()
		local n1 = 42
		local n2 = 42
		local res = (n1 < n2)
		return res
	end

	assert(cinterpcall(islt_nums_equal) == false)
end

do -- ISLT (number, number): n1 > n2
	local islt_n1_greater = function()
		local n1 = 43
		local n2 = 42
		local res = (n1 < n2)
		return res
	end

	assert(cinterpcall(islt_n1_greater) == false)
end

do -- ISLT (number, NaN): n1 < NaN - false
	local islt_n1_less_than_nan = function()
		local n1 = 41
		local nan = 0 / 0
		local res = (n1 < nan)
		return res
	end

	assert(cinterpcall(islt_n1_less_than_nan) == false)
end

do -- ISLT (NaN, NaN): NaN < NaN - false
	local islt_nan_less_than_nan = function()
		local nan = 0 / 0
		local res = (nan < nan)
		return res
	end

	assert(cinterpcall(islt_nan_less_than_nan) == false)
end

do -- ISLT (NaN, number): NaN < n1 - false
	local islt_nan_less_than_n1 = function()
		local n1 = 41
		local nan = 0 / 0
		local res = (nan < n1)
		return res
	end

	assert(cinterpcall(islt_nan_less_than_n1) == false)
end

do -- ISLT (string, string): str1 < string
	local islt_str1_less = function()
		local n1 = "A"
		local n2 = "B"
		local res = (n1 < n2)
		return res
	end

	assert(cinterpcall(islt_str1_less) == true)
end

do -- ISLT (string, string): str1 == str2
	local islt_strings_equal = function()
		local str1 = "A"
		local str2 = "A"
		local res = (str1 < str2)
		return res
	end

	assert(cinterpcall(islt_strings_equal) == false)
end

do -- ISLT (string, string): str1 > str2
	local islt_str1_greater = function()
		local str1 = "B"
		local str2 = "A"
		local res = (str1 < str2)
		return res
	end

	assert(cinterpcall(islt_str1_greater) == false)
end

do -- ISLT + __lt (table, table): t1 < t2
	local islt_t1_less = function()
		local t1 = {k = 123}
		local t2 = {k = 456}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 < t2)
		return res
	end

	assert(cinterpcall(islt_t1_less) == true)
end

do -- ISLT + __lt (table, table): t1 == t2
	local islt_tables_equal = function()
		local t1 = {k = 123}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 < t2)
		return res
	end

	assert(cinterpcall(islt_tables_equal) == false)
end

do -- ISLT + __lt (table, table): t1 > t2
	local islt_t1_greater = function()
		local t1 = {k = 456}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 < t2)
		return res
	end

	assert(cinterpcall(islt_t1_greater) == false)
end

