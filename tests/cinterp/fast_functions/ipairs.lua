-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local tu = require 'testutil'
local assert_iteration_results_ordered = tu.assert_iteration_results_ordered
local assert_ends_with = tu.assert_ends_with

do
	local ipairs_empty = function()
		local t = {}
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local res = cinterpcall(ipairs_empty)
	assert(#res == 0) -- {}
end

do
	local ipairs_array = function()
		local t = {10, 20, 30}
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
	}

	local res = cinterpcall(ipairs_array)
	assert_iteration_results_ordered(res, expected)
end

do -- iteration stops on holes in array
	local ipairs_array_holes = function()
		local t = {10, 20, 30, nil, 40}
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
	}

	local res = cinterpcall(ipairs_array_holes)
	assert_iteration_results_ordered(res, expected)
end

do
	local ipairs_array_holes_at_start = function()
		local t = {nil, 10, 20, 30, nil, 40}
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local res = cinterpcall(ipairs_array_holes_at_start)
	assert(#res == 0) -- {}
end

do -- hash part of array is normally not iterated on
	local ipairs_only_iterates_array_part = function()
		local t = {10, 20, 30, test = "hello", other = "world"}
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
	}

	local res = cinterpcall(ipairs_only_iterates_array_part)
	assert_iteration_results_ordered(res, expected)
end

do -- unless hash part has integer indices in it and no holes
	local ipairs_iterates_hash_part = function()
		local t = {
			[3] = 30, -- hash part
			10, 20, -- array part
		}
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	local expected = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 },
	}

	local res = cinterpcall(ipairs_iterates_hash_part)
	assert_iteration_results_ordered(res, expected)
end

do -- __ipairs metamethod
	local mt = {
		__ipairs = function(t)
			local function iter(t, i)
				i = i + 1
				local v = t[i]
				if v then
					return i, v * 2
				end
			end

			return iter, t, 0
		end,
	}

	local ipairs_mt = function()
		local t = {10, 20, 30}
		setmetatable(t, mt)
		local res = {}
		for k, v in ipairs(t) do
			res[#res + 1]  = { k, v }
		end
		return res
	end

	-- __ipairs metamethod is only honored under Lua 5.2 semantics
	local lua52compat = (rawlen ~= nil)
	local expected = lua52compat
		and {
			{ 1, 20 },
			{ 2, 40 },
			{ 3, 60 },
		}
		or {
			{ 1, 10 },
			{ 2, 20 },
			{ 3, 30 },
		}

	local res = cinterpcall(ipairs_mt)
	assert_iteration_results_ordered(res, expected)
end

do
	local ipairs_bad_argument = function()
		for k, v in ipairs("a") do
			print("test")
		end
	end

	local caller = function()
		local ok, msg = pcall(ipairs_bad_argument)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'ipairs' (table expected, got string)")
end

do -- bad ipairs argument
	local ipairs_exception = function()
		local t = nil
		for k, v in ipairs(t) do end
	end

	local caller = function()
		local ok, msg = pcall(ipairs_exception)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "bad argument #1 to 'ipairs' (table expected, got nil)")

end
