-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- EQV (number, number): n1 == n2
	local eqv_number_equal = function()
		local n1 = 42
		local n2 = 42
		local res = (n1 == n2)
		return res
	end

	assert(cinterpcall(eqv_number_equal) == true)
end

do -- EQV (number, number): n1 != n2
	local eqv_number_not_equal = function()
		local n1 = 42
		local n2 = 43
		local res = (n1 == n2)
		return res
	end

	assert(cinterpcall(eqv_number_not_equal) == false)
end

do -- EQV (number, NaN): n1 == NaN - false
	local eqv_number_not_equal_to_nan = function()
		local n1 = 42
		local nan = 0 / 0
		local res = (n1 == n2)
		return res
	end

	assert(cinterpcall(eqv_number_not_equal_to_nan) == false)
end

do -- EQV (NaN, NaN): NaN == NaN - false
	local eqv_nan_equal_to_nan = function()
		local nan = 0 / 0
		local res = (nan == nan)
		return res
	end

	assert(cinterpcall(eqv_nan_equal_to_nan) == false)
end

do -- EQV (pri, pri): p1 == p2
	local eqv_prim_equal = function()
		local p1 = true
		local p2 = true
		local res = (p1 == p2)
		return res
	end

	assert(cinterpcall(eqv_prim_equal) == true)
end

do -- EQV (pri, pri): p1 != p2
	local eqv_prim_not_equal = function()
		local p1 = true
		local p2 = false
		local res = (p1 == p2)
		return res
	end

	assert(cinterpcall(eqv_prim_not_equal) == false)
end

do -- EQV (string, string): s1 == s2
	local eqv_string_equal = function()
		local s1 = "hello"
		local s2 = "hello"
		local res = (s1 == s2)
		return res
	end

	assert(cinterpcall(eqv_string_equal) == true)
end

do -- EQV (string, string): s1 != s2
	local eqv_string_not_equal = function()
		local s1 = "hello"
		local s2 = "world"
		local res = (s1 == s2)
		return res
	end

	assert(cinterpcall(eqv_string_not_equal) == false)
end

do -- EQV (table, table): same table
	local eqv_table_same_equal = function()
		local t1 = {}
		local t2 = t1
		local res = (s1 == s2)
		return res
	end

	assert(cinterpcall(eqv_table_same_equal) == true)
end

do -- EQV (table, table): not the same table
	local eqv_table_different_not_equal = function()
		local t1 = {}
		local t2 = {}
		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_table_different_not_equal) == false)
end

do -- EQV (table, table): tables have __eq metamethod and are equal
	local eqv_mt_equal = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_mt_equal) == true)
end

do -- EQV (table, table): tables have __eq metamethod and are not equal
	local eqv_mt_not_equal = function()
		local t1 = {x = 42}
		local t2 = {x = 43}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_mt_not_equal) == false)
end

do -- EQV (table, table): first table doesn't have metatable
	local eqv_first_no_mt = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t2, mt)

		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_first_no_mt) == false)
end

do -- EQV (table, table): second table doesn't have metatable
	local eqv_second_no_mt = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)

		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_second_no_mt) == false)
end

do -- EQV (table, table): tables have __eq metamethod, but meta-tables differ
	local eqv_different_mt_not_equal = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		local mt2 = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)
		setmetatable(t2, mt2)

		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_different_mt_not_equal) == false)
end

do -- EQV (table, string): metatables only work in table-table comparison
	local eqv_metatables_only_work_on_tables = function()
		local t1 = {str = "hello"}
		local s = "hello"
		local mt = { __eq = function(a, b) return a.str == s end }
		setmetatable(t1, mt)

		local res = (t1 == s)
		return res
	end

	assert(cinterpcall(eqv_metatables_only_work_on_tables) == false)
end

do -- EQV (table, table): metatables match, but no __eq method in it
	local eqv_mt_no_eq = function()
		local t1 = {str = "hello"}
		local t2 = {str = "hello"}
		local mt = { __tostring = function(t) return t.str end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 == t2)
		return res
	end

	assert(cinterpcall(eqv_mt_no_eq) == false)
end

do -- EQV (string, number): types differ - not equal
	local eqv_mt_different_types = function()
		local n1 = 42
		local str2 = "42"

		local res = (n1 == str2)
		return res
	end

	assert(cinterpcall(eqv_mt_different_types) == false)
end

do -- EQV (number, nil): types differ - not equal
	local eqv_mt_num_nil = function()
		local n1 = 42
		local n2 = nil

		local res = (n1 == n2)
		return res
	end

	assert(eqv_mt_num_nil() == false)
end

do -- EQV (nil, nil): nil, nil - equal
	local eqv_mt_nil_nil = function()
		local n1 = nil
		local n2 = nil

		local res = (n1 == n2)
		return res
	end

	assert(cinterpcall(eqv_mt_nil_nil) == true)
end
