-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path

local tu = require 'testutil'
local assert_iteration_results = tu.assert_iteration_results
local assert_iteration_results_ordered = tu.assert_iteration_results_ordered
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

do -- arrays
	local itern_arrays = function()
		local t = { 10, 40, 50 }
		local res = {}
		for k, v in next, t do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_arrays)
	local expected = {
		{ 1, 10 },
		{ 2, 40 },
		{ 3, 50 },
	}
	assert_iteration_results_ordered(res, expected)
end

do -- hash maps
	local itern_hash_maps = function()
		local t = {
			key1 = "hello",
			a = "world",
			[4.5] = "test", -- this would go into the hash part
		}
		local res = {}
		for k, v in next, t do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_hash_maps)
	local expected = {
		{ "key1", "hello" },
		{ "a", "world" },
		{ 4.5, "test" },
	}
	assert_iteration_results(res, expected)
end

do -- table has array part and hash part
	local itern_no_holes = function()
		local t = {
			10, 20, 30, -- array part
			a = "hello", b = "world", c = "bye", -- hash part
		}

		local res = {}
		for k, v in next, t do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_no_holes)
	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
		{ "a", "hello" },
		{ "b", "world" },
		{ "c", "bye" }
	}
	assert_iteration_results(res, expected)
end

do -- table has holes in array and hash parts - they're skipped
	local itern_holes = function()
		local t = {
			nil, nil, 10, 20, nil, 30, -- array part has holes
			a = "hello", b = nil, c = "bye", -- hash part has holes
		}

		local res = {}
		for k, v in next, t do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_holes)
	local expected = {
		{ 3, 10 },
		{ 4, 20 },
		{ 6, 30 },
		{ "a", "hello" },
		{ "c", "bye" }
	}
	assert_iteration_results(res, expected)
end

do -- table is empty
	local itern_empty_table = function()
		local t = {}
		local res = {}
		for k, v in next, t do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_empty_table)
	local expected = {}
	assert_iteration_results(res, expected)
end

--------------------------
-- despecialization tests
--------------------------

local custom_next = function(t, i)
	if not i then
		i = 0
	end
	i = i + 1
	local v = t[i]
	if v then
		-- return (v + 1) to check that this was called, not the builtin
		return i, v + 1
	end
end

do -- callable is not a function - despecialization occurs
	local itern_despecialize_callable_not_a_function = function()
		local next = {}
		setmetatable(next,
			{
				__call = function(self, t, i)
					i = i + 1
					local v = t[i]
					if v then
						return i, v + 1
					end
				end
			})
		local t = { 10, 20, 30 }
		local res = {}
		for k, v in next, t, 1 do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_despecialize_callable_not_a_function)
	local expected = {
		{ 2, 21 },
		{ 3, 31 },
	}
	assert_iteration_results_ordered(res, expected)
end

do -- state is not a table - despecialization occurs
	local itern_despecialize_state_is_not_table = function()
		local t = "test"
		local res = {}
		for k, v in next, t, 1 do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local caller = function()
		local ok, msg = pcall(itern_despecialize_state_is_not_table)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "(table expected, got string)")
end

do -- control var is not nil - despecialization occurs
	local itern_despecialize_control_var_not_nil = function()
		local t = { 10, 20, 30 }
		local res = {}
		for k, v in next, t, 1 do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_despecialize_control_var_not_nil)
	local expected = {
		{ 2, 20 },
		{ 3, 30 },
	}
	assert_iteration_results_ordered(res, expected)

	-- call again to check that BC_ITERL target is restored properly
	res = cinterpcall(itern_despecialize_control_var_not_nil)
	assert_iteration_results_ordered(res, expected)
end

do -- control var is not nil - despecialization occurs
	local itern_despecialize_control_var_not_nil = function()
		local t = { 10, 20, 30 }
		local res = {}
		for k, v in next, t, 1 do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_despecialize_control_var_not_nil)
	local expected = {
		{ 2, 20 },
		{ 3, 30 },
	}
	assert_iteration_results_ordered(res, expected)
end

do -- callable is not FF next - despecialization will occur
	local itern_despecialize_not_next_ff = function()
		local next = custom_next
		local t = { 10, 20, 30 }
		local res = {}
		for k, v in next, t do
			res[#res + 1] = { k, v }
		end
		return res
	end

	local res = cinterpcall(itern_despecialize_not_next_ff)
	local expected = {
		{ 1, 11 },
		{ 2, 21 },
		{ 3, 31 },
	}
	assert_iteration_results_ordered(res, expected)
end
