-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISLE (number, number): n1 < n2
	local isle_n1_less = function()
		local n1 = 41
		local n2 = 42
		local res = (n1 <= n2)
		return res
	end

	assert(cinterpcall(isle_n1_less) == true)
end

do -- ISLE (number, number): n1 == n2
	local isle_nums_equal = function()
		local n1 = 42
		local n2 = 42
		local res = (n1 <= n2)
		return res
	end

	assert(cinterpcall(isle_nums_equal) == true)
end

do -- ISLE (number, number): n1 > n2
	local isle_n1_greater = function()
		local n1 = 43
		local n2 = 42
		local res = (n1 <= n2)
		return res
	end

	assert(cinterpcall(isle_n1_greater) == false)
end

do -- ISLE (number, NaN): n1 <= NaN - false
	local isle_n1_le_nan = function()
		local n1 = 41
		local nan = 0 / 0
		local res = (n1 <= nan)
		return res
	end

	assert(cinterpcall(isle_n1_le_nan) == false)
end

do -- ISLE (NaN, NaN): NaN <= NaN - false
	local isle_nan_le_nan = function()
		local nan = 0 / 0
		local res = (nan <= nan)
		return res
	end

	assert(cinterpcall(isle_nan_le_nan) == false)
end

do -- ISLE (NaN, number): NaN <= n1 - false
	local isle_nan_le_n1 = function()
		local n1 = 41
		local nan = 0 / 0
		local res = (nan <= n1)
		return res
	end

	assert(cinterpcall(isle_nan_le_n1) == false)
end

do -- ISLE (string, string): str1 < string
	local isle_str1_less = function()
		local n1 = "A"
		local n2 = "B"
		local res = (n1 <= n2)
		return res
	end

	assert(cinterpcall(isle_str1_less) == true)
end

do -- ISLE (string, string): str1 == str2
	local isle_strings_equal = function()
		local str1 = "A"
		local str2 = "A"
		local res = (str1 <= str2)
		return res
	end

	assert(cinterpcall(isle_strings_equal) == true)
end

do -- ISLE (string, string): str1 > str2
	local isle_str1_greater = function()
		local str1 = "B"
		local str2 = "A"
		local res = (str1 < str2)
		return res
	end

	assert(cinterpcall(isle_str1_greater) == false)
end

do -- ISLE + __lt (table, table): t1 < t2
	local isle_t1_less = function()
		local t1 = {k = 123}
		local t2 = {k = 456}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 <= t2)
		return res
	end

	assert(cinterpcall(isle_t1_less) == true)
end

do -- ISLE + __lt (table, table): t1 == t2
	local isle_tables_equal = function()
		local t1 = {k = 123}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 <= t2)
		return res
	end

	assert(cinterpcall(isle_tables_equal) == true)
end

do -- ISLE + __lt (table, table): t1 > t2
	local isle_t1_greater = function()
		local t1 = {k = 456}
		local t2 = {k = 123}

		local mt = {__lt = function(op1, op2) return op1.k < op2.k end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 < t2)
		return res
	end

	assert(cinterpcall(isle_t1_greater) == false)
end

do -- test from lua-Harness which fails if continuation slots aren't implemented properly
	local isle_complex = function()
		local t1 = {re = 2, im = 0}
		local t2 = {re = 2, im = 0}

		local mt = {
			__le = function(a, b)
				local ra = a.re * a.re + a.im * a.im
				local rb = b.re * b.re + b.im * b.im
				return ra <= rb
			end
		}

		setmetatable(t1, mt)
		setmetatable(t2, mt)

		return t1 <= t2
	end

	local res = cinterpcall(isle_complex)
	assert(res == true)
end
