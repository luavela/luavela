-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- See eqv.lua - all the tests here are the same
-- with "==" replaced by "~=" and true/false swapped
-- where necessary.
local cinterpcall = ujit.debug.cinterpcall

do -- NEV (number, number): n1 == n2
	local nev_number_equal = function()
		local n1 = 42
		local n2 = 42
		local res = (n1 ~= n2)
		return res
	end

	assert(cinterpcall(nev_number_equal) == false)
end

do -- NEV (number, number): n1 != n2
	local nev_number_not_equal = function()
		local n1 = 42
		local n2 = 43
		local res = (n1 ~= n2)
		return res
	end

	assert(cinterpcall(nev_number_not_equal) == true)
end

do -- NEV (number, NaN): n1 != NaN - true
	local nev_number_not_equal_to_nan = function()
		local n1 = 42
		local nan = 0 / 0
		local res = (n1 ~= nan)
		return res
	end

	assert(cinterpcall(nev_number_not_equal_to_nan) == true)
end

do -- NEV (NaN, NaN): NaN != NaN - true
	local nev_nan_not_equal_to_nan = function()
		local nan = 0 / 0
		local res = (nan ~= nan)
		return res
	end

	assert(cinterpcall(nev_nan_not_equal_to_nan) == true)
end

do -- NEV (pri, pri): p1 != p2
	local nev_prim_equal = function()
		local p1 = false
		local p2 = true
		local res = (p1 ~= p2)
		return res
	end

	assert(cinterpcall(nev_prim_equal) == true)
end

do -- NEV (pri, pri): p1 == p2
	local nev_prim_not_equal = function()
		local p1 = false
		local p2 = false
		local res = (p1 ~= p2)
		return res
	end

	assert(cinterpcall(nev_prim_not_equal) == false)
end

do -- NEV (string, string): s1 == s2
	local nev_string_equal = function()
		local s1 = "hello"
		local s2 = "hello"
		local res = (s1 ~= s2)
		return res
	end

	assert(cinterpcall(nev_string_equal) == false)
end

do -- NEV (string, string): s1 != s2
	local nev_string_not_equal = function()
		local s1 = "hello"
		local s2 = "world"
		local res = (s1 ~= s2)
		return res
	end

	assert(cinterpcall(nev_string_not_equal) == true)
end

do -- NEV (table, table): same table
	local nev_table_same_equal = function()
		local t1 = {}
		local t2 = t1
		local res = (s1 ~= s2)
		return res
	end

	assert(cinterpcall(nev_table_same_equal) == false)
end

do -- NEV (table, table): not the same table
	local nev_table_different_not_equal = function()
		local t1 = {}
		local t2 = {}
		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_table_different_not_equal) == true)
end

do -- NEV (table, table): tables have __eq metamethod and are equal
	local nev_mt_equal = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_mt_equal) == false)
end

do -- NEV (table, table): tables have __eq metamethod and are not equal
	local nev_mt_not_equal = function()
		local t1 = {x = 42}
		local t2 = {x = 43}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_mt_not_equal) == true)
end

do -- NEV (table, table): first table doesn't have metatable
	local nev_first_no_mt = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t2, mt)

		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_first_no_mt) == true)
end

do -- NEV (table, table): second table doesn't have metatable
	local nev_second_no_mt = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)

		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_second_no_mt) == true)
end

do -- NEV (table, table): tables have __eq metamethod, but meta-tables differ
	local nev_different_mt_not_equal = function()
		local t1 = {x = 42}
		local t2 = {x = 42}
		local mt = { __eq = function(a, b) return a.x == b.x end }
		local mt2 = { __eq = function(a, b) return a.x == b.x end }
		setmetatable(t1, mt)
		setmetatable(t2, mt2)

		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_different_mt_not_equal) == true)
end

do -- NEV (table, string): metatables only work in table-table comparison
	local nev_metatables_only_work_on_tables = function()
		local t1 = {str = "hello"}
		local s = "hello"
		local mt = { __eq = function(a, b) return a.str == s end }
		setmetatable(t1, mt)

		local res = (t1 ~= s)
		return res
	end

	assert(cinterpcall(nev_metatables_only_work_on_tables) == true)
end

do -- EQV (table, table): metatables match, but no __eq method in it
	local nev_mt_no_eq = function()
		local t1 = {str = "hello"}
		local t2 = {str = "hello"}
		local mt = { __tostring = function(t) return t.str end}
		setmetatable(t1, mt)
		setmetatable(t2, mt)

		local res = (t1 ~= t2)
		return res
	end

	assert(cinterpcall(nev_mt_no_eq) == true)
end

do -- NEV (string, number): types differ - not equal
	local nev_mt_different_types = function()
		local n1 = 42
		local str2 = "42"

		local res = (n1 ~= str2)
		return res
	end

	assert(cinterpcall(nev_mt_different_types) == true)
end

do -- NEV (number, nil): types differ - not equal
	local nev_mt_num_nil = function()
		local n1 = 42
		local n2 = nil

		local res = (n1 ~= n2)
		return res
	end

	assert(cinterpcall(nev_mt_num_nil) == true)
end

do -- NEV (nil, nil): nil, nil - equal
	local nev_mt_nil_nil = function()
		local n1 = nil
		local n2 = nil

		local res = (n1 ~= n2)
		return res
	end

	assert(cinterpcall(nev_mt_nil_nil) == false)
end

