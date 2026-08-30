-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local tu = require 'testutil'
local assert_iteration_results_ordered = tu.assert_iteration_results_ordered

-- simple array iteration function
local iter = function(t, i)
	i = i + 1
	local v = t[i]
	if v then
		return i, v
	end
end

do -- generic for loop
	local generic_for = function()
		local t = { 10, 20, 30 }
		local res = {}
		for i, v in iter, t, 0 do
			res[#res + 1]  = { i, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
	}

	local res = cinterpcall(generic_for)
	assert_iteration_results_ordered(res, expected)
end

do -- generic for loop, but start with 2nd element of the array
	local generic_for_state = function()
		local t = { 10, 20, 30 }
		local res = {}
		for i, v in iter, t, 1 do
			res[#res + 1]  = { i, v }
		end
		return res
	end

	local expected = {
		{ 2, 20 },
		{ 3, 30 },
	}

	local res = cinterpcall(generic_for_state)
	assert_iteration_results_ordered(res, expected)
end

do -- generic for loop, but exit immediately (iter will return nil)
	local generic_for_exit_first_iteration = function()
		local t = { 10, 20, 30 }
		local res = {}
		for i, v in iter, t, 3 do
			res[#res + 1]  = { i, v }
		end
		return res
	end

	local expected = {}

	local res = cinterpcall(generic_for_exit_first_iteration)
	assert_iteration_results_ordered(res, expected)
end

do -- generic for loop which uses __call metamethod
	local generic_for_mt = function()
		local iterTable = {}
		setmetatable(iterTable,
			{
				__call = function(self, t, i)
					i = i + 1
					local v = t[i]
					if v then
						return i, v * 2
					end
				end
			})
		local t = { 10, 20, 30 }
		local res = {}
		for i, v in iterTable, t, 0 do
			res[#res + 1]  = { i, v }
		end
		return res
	end

	local res = cinterpcall(generic_for_mt)
	local expected = {
		{1, 20},
		{2, 40},
		{3, 60}
	}
	assert_iteration_results_ordered(res, expected)
end

-- TODO: error handling inside loops, e.g.
--	* Passing nil callable
--	* Passing nil state
--	* Passing nil control var
--	* Errors inside callable
