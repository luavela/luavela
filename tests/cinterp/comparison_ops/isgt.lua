-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NOTE:
--		`local res = not (a >= b)` - produces ISGT
--		`local res = a > b`        - produces ISLT

local cinterpcall = ujit.debug.cinterpcall

do -- ISGT (number, number): n1 < n2
	local isgt_n1_less = function()
		local n1 = 41
		local n2 = 42
		res = not (n1 >= n2)
		return res
	end

	assert(cinterpcall(isgt_n1_less) == true)
end

do -- ISGT (number, number): n1 == n2
	local isgt_nums_equal = function()
		local n1 = 42
		local n2 = 42
		local res = not (n1 >= n2)
		return res
	end

	assert(cinterpcall(isgt_nums_equal) == false)
end

do -- ISGT (number, number): n1 > n2
	local isgt_n1_greater = function()
		local n1 = 43
		local n2 = 42
		local res = not (n1 >= n2)
		return res
	end

	assert(cinterpcall(isgt_n1_greater) == false)
end

do -- ISGT (number, NaN): not(n1 >= NaN) - true
	local isgt_n1_not_ge_nan = function()
		local n1 = 41
		local nan = 0 / 0
		local res = not (n1 >= nan)
		return res
	end

	assert(cinterpcall(isgt_n1_not_ge_nan) == true)
end

do -- ISGT (NaN, NaN): not(NaN >= NaN) - true
	local isgt_nan_not_ge_nan = function()
		local nan = 0 / 0
		local res = not (nan >= nan)
		return res
	end

	assert(cinterpcall(isgt_nan_not_ge_nan) == true)
end

do -- ISGT (NaN, number): not(NaN >= n1) - true
	local isgt_nan_not_ge_n1 = function()
		local n1 = 41
		local nan = 0 / 0
		local res = not (nan >= nan)
		return res
	end

	assert(cinterpcall(isgt_nan_not_ge_n1) == true)
end

do -- ISGT (string, string): str1 < string
	local isgt_str1_less = function()
		local n1 = "A"
		local n2 = "B"
		local res = not (n1 >= n2)
		return res
	end

	assert(cinterpcall(isgt_str1_less) == true)
end

do -- ISGT (string, string): str1 == str2
	local isgt_strings_equal = function()
		local str1 = "A"
		local str2 = "A"
		local res = not (str1 >= str2)
		return res
	end

	assert(cinterpcall(isgt_strings_equal) == false)
end

do -- ISGT (string, string): str1 > str2
	local isgt_str1_greater = function()
		local str1 = "B"
		local str2 = "A"
		local res = not (str1 >= str2)
		return res
	end

	assert(cinterpcall(isgt_str1_greater) == false)
end

do -- ISGT + __lt (table, table): t1 < t2
	local isgt_t1_less = function()
		local t1 = {k = 123}
		local t2 = {k = 456}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = not (t1 >= t2)
		return res
	end

	assert(cinterpcall(isgt_t1_less) == true)
end

do -- ISGT + __lt (table, table): t1 == t2
	local isgt_tables_equal = function()
		local t1 = {k = 123}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = not (t1 >= t2)
		return res
	end

	assert(cinterpcall(isgt_tables_equal) == false)
end

do -- ISGT + __lt (table, table): t1 > t2
	local isgt_t1_greater = function()
		local t1 = {k = 456}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = not (t1 >= t2)
		return res
	end

	assert(cinterpcall(isgt_t1_greater) == false)
end
