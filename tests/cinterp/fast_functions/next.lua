-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local tu = require 'testutil'
local assert_iteration_results = tu.assert_iteration_results
local assert_ends_with = tu.assert_ends_with

do -- next(t) - nil added as the second argument implicitly
	local next_one_arg = function()
		local t = { 10, 20, 30}
		local k, v = next(t)
		return k, v
	end

	local k, v = cinterpcall(next_one_arg)
	assert(k == 1)
	assert(v == 10)
end

do -- next(t, nil) - explicit nil
	local next_nil = function()
		local t = { 10, 20, 30}
		local k, v = next(t, nil)
		return k, v
	end

	local k, v = cinterpcall(next_nil)
	assert(k == 1)
	assert(v == 10)
end

do -- next(t, idx)
	local next_idx = function()
		local t = { 10, 20, 30}
		local k, v = next(t, 1)
		return k, v
	end

	local k, v = cinterpcall(next_idx)
	assert(k == 2)
	assert(v == 20)
end

do -- next(t, idx) - idx invalid
	local next_invalid_idx = function()
		local t = { 10, 20, 30}
		local k, v = next(t, 1000)
		return k, v
	end

	local caller = function()
		local ok, msg = pcall(next_invalid_idx)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "invalid key to 'next'")
end

do -- iterate complex table with holes
	local next_iterate = function()
		local t = {
			nil, nil, 10, 20, nil, 30, -- array part has holes
			a = "hello", b = nil, c = "bye", -- hash part has holes
		}
		local res = {}

		local k, v = next(t, nil)
		while k do
			res[#res + 1]  = { k, v }
			k, v = next(t, k)
		end

		return res
	end

	local expected = {
		{ 3, 10 },
		{ 4, 20 },
		{ 6, 30 },
		{ "a", "hello" },
		{ "c", "bye" }
	}

	local res = cinterpcall(next_iterate)
	assert_iteration_results(res, expected)
end

do -- check that ISNEXT correctly initializes control variable
	local func = function() end
	local tab = {1, 2, 3, 4, 5}
	local counter = 0

	local function iter(next)
		for k, v in next, tab, nil do
			counter = counter + 1

			if v == 5 then
				iter(func)
			end
		end
	end

	local check_control_var = function()
		iter(next)
		return counter
	end

	assert(cinterpcall(check_control_var) == 5)
end

do -- same as above but with __call metamethod handled by ITERC
	local callable = setmetatable({}, {__call = function() end})
	local tab = {1, 2, 3, 4, 5}
	local counter = 0

	local function iter(next)
		for k, v in next, tab, nil do
			counter = counter + 1

			if v == 5 then
				iter(callable)
			end
		end
	end

	local check_control_var_mm = function()
		iter(next)
		return counter
	end

	assert(cinterpcall(check_control_var_mm) == 5)
end

do -- check that non-callable object correctly handled by ITERC
	local not_callable = {}
	local tab = {1, 2}

	local function iter(next)
		for k, v in next, tab, nil do
			iter(not_callable)
		end
	end

	local caller = function()
		return pcall(iter, next)
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "attempt to call a table value")
end
