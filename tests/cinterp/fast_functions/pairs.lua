-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- pairs fast function gets replaced with ITERN if the loop has the following form:
--	for k, v in pairs(t) do
--		...
--	end
--
-- If the loop changes to something like:
--	local ps = pairs(t)
--	for k, v in ps(t) do
--		...
--	end
--
-- then ITERC is used (and fast function next is used for iteration)
-- So, most of the test cases are covered by ITERC/ITERN/next tests, really.

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

-- pairs order is not guaranteed, so we're doing unordered checks
local assert_iteration_results = tu.assert_iteration_results
local assert_ends_with = tu.assert_ends_with

do
	local pairs_empty = function()
		local t = {}
		local res = {}
		for k, v in pairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local res = cinterpcall(pairs_empty)
	assert(#res == 0) -- {}
end

do
	local pairs_iterate = function()
		local t = {10, 20, 30, k1 = "hello", k2 = "world", k3 = "bye"}
		local res = {}
		for k, v in pairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
		{ "k1", "hello" },
		{ "k2", "world" },
		{ "k3", "bye" },
	}

	local res = cinterpcall(pairs_iterate)
	assert_iteration_results(res, expected)
end

do -- the same as the previous test, but uses ITERC instead of ITERN
	local pairs_iterate_iterc = function()
		local t = {10, 20, 30, k1 = "hello", k2 = "world", k3 = "bye"}
		local res = {}
		local ps = pairs -- prevent ITERN bytecode generation here
		for k, v in ps(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
		{ "k1", "hello" },
		{ "k2", "world" },
		{ "k3", "bye" },
	}

	local res = cinterpcall(pairs_iterate_iterc)
	assert_iteration_results(res, expected)
end

do -- __pairs metamethod
	local mt = {
		__pairs = function(t)
			local function iter(t, k)
				k, v = next(t, k)
				if v then
					return k, v * 2
				end
			end

			return iter, t, nil
		end,
	}

	local pairs_mt = function()
		local t = {a = 10, b = 20, c = 30}
		setmetatable(t, mt)
		local res = {}
		for k, v in pairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	-- __pairs metamethod is only honored under Lua 5.2 semantics
	local lua52compat = (rawlen ~= nil)
	local expected = lua52compat
		and {
			{ "a", 20 },
			{ "b", 40 },
			{ "c", 60 },
		}
		or {
			{ "a", 10 },
			{ "b", 20 },
			{ "c", 30 },
		}

	local res = cinterpcall(pairs_mt)
	assert_iteration_results(res, expected)
end

do
	local pairs_bad_argument = function()
		for k, v in pairs("a") do
			print("test")
		end
	end

	local caller = function()
		local ok, msg = pcall(pairs_bad_argument)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'pairs' (table expected, got string)")
end

do -- bad pairs argument
	local pairs_exception = function()
		local t = nil
		for k, v in pairs(t) do end
	end

	local caller = function()
		local ok, msg = pcall(pairs_exception)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'pairs' (table expected, got nil)")

end
