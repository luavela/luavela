-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NOTE:
--		`local res = not (a > b)` - produces ISGE
--		`local res = a >= b`      - produces ISLE
local cinterpcall = ujit.debug.cinterpcall

do -- ISGE (number, number): n1 < n2
	local isge_n1_less = function()
		local n1 = 41
		local n2 = 42
		local res = not (n1 > n2)
		return res
	end

	assert(cinterpcall(isge_n1_less) == true)
end

do -- ISGE (number, number): n1 == n2
	local isge_nums_equal = function()
		local n1 = 42
		local n2 = 42
		local res = not (n1 > n2)
		return res
	end

	assert(cinterpcall(isge_nums_equal) == true)
end

do -- ISGE (number, number): n1 > n2
	local isge_n1_greater = function()
		local n1 = 43
		local n2 = 42
		local res = not (n1 > n2)
		return res
	end

	assert(cinterpcall(isge_n1_greater) == false)
end

do -- ISGE (number, NaN): not(n1 > NaN) - true
	local isge_n1_not_greater_than_nan = function()
		local n1 = 41
		local nan = 0 / 0
		local res = not (n1 > nan)
		return res
	end

	assert(cinterpcall(isge_n1_not_greater_than_nan) == true)
end

do -- ISGE (NaN, NaN): not(NaN > NaN) - true
	local isge_nan_not_greater_than_nan = function()
		local nan = 0 / 0
		local res = not (nan > nan)
		return res
	end

	assert(cinterpcall(isge_nan_not_greater_than_nan) == true)
end

do -- ISGE (NaN, number): not(NaN > n1) - true
	local isge_nan_not_greater_than_n1 = function()
		local n1 = 41
		local nan = 0 / 0
		local res = not (nan > nan)
		return res
	end

	assert(cinterpcall(isge_nan_not_greater_than_n1) == true)
end

do -- ISGE (string, string): str1 < string
	local isge_str1_less = function()
		local n1 = "A"
		local n2 = "B"
		local res = not (n1 > n2)
		return res
	end

	assert(cinterpcall(isge_str1_less) == true)
end

do -- ISGE (string, string): str1 == str2
	local isge_strings_equal = function()
		local str1 = "A"
		local str2 = "A"
		local res = not (str1 > str2)
		return res
	end

	assert(cinterpcall(isge_strings_equal) == true)
end

do -- ISGE (string, string): str1 > str2
	local isge_str1_greater = function()
		local str1 = "B"
		local str2 = "A"
		local res = not (str1 > str2)
		return res
	end

	assert(cinterpcall(isge_str1_greater) == false)
end

do -- ISGE + __lt (table, table): t1 < t2
	local isge_t1_less = function()
		local t1 = {k = 123}
		local t2 = {k = 456}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = not (t1 > t2)
		return res
	end

	assert(cinterpcall(isge_t1_less) == true)
end

do -- ISGE + __lt (table, table): t1 == t2
	local isge_tables_equal = function()
		local t1 = {k = 123}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = not (t1 > t2)
		return res
	end

	assert(cinterpcall(isge_tables_equal) == true)
end

do -- ISGE + __lt (table, table): t1 > t2
	local isge_t1_greater = function()
		local t1 = {k = 456}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = not (t1 > t2)
		return res
	end

	assert(cinterpcall(isge_t1_greater) == false)
end

