--
-- Performance benchmarking is implemented by comparing copy operation timings on
-- tables of different nature (see below) and considerable sizes.
--
-- For the majority of function from table module, they don't traverse a table in depth but
-- deal with the top level only, there is no need to vary table level count or
-- its contents. When top level is being iterated through, only numeric values are copied;
-- table values are copied by reference (in terms of implementation, it means that
-- different TValue-s share a pointer to a same table heap memory blob).
-- In terms of test's structure, the latter means there is no difference in whether
-- a table value is a table of a considerable size or an empty one
-- (TODO: make sure empty tables are not optimized out anyhow).
--
-- Performance testing is designed as follows:
-- a table of arbitrary type (see below) and size is generated.
--
-- Type of a table can be (these types resemble those in lua tests):
-- 1. array: a table with integer sequentially continuous keys
-- 2. hash: a table with arbitrary non-integer keys (the requirement of 'non-integer'
--   originates from a desire to have array part of table storage be empty)
-- 3. mixed: integer and other-typed keys
--
-- Size (length) is varied in a provided range to make sure performance gain
-- is not specific for one length only.
--
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local BENCHMARK_DELTA = 1e-8

local TABLE_TYPES = {
	"array", "hash", "mixed"
}

-- Factory for table kv-pairs generation of 'array'/'hash'/'mixed' types
local function KVPairGenFactory(length, ttype)
	assert("number" == type(length))
	assert("string" == type(ttype))

	local i = 0

	local is_within_length = function()
		i = i + 1
		if (i > length) then
			return nil
		end
		return i  -- ?
	end

	local dispatch_type_table = {
		array = function () return i, 1 end,
		hash = function() return "k_" .. i, 1 end,
		mixed = function()
			local r = math.random(1, 2)
			if (1 == r) then
				return i, 1
			else
				return "k_" .. i, 1
			end
		end
	}

	local get_kv = dispatch_type_table[ttype]

	return function ()
		if (is_within_length()) then
			return get_kv()
		end
	end
end

-- Generates a table of particular type defined by generator.
local function _generate_table(kv_generator)
	local t = {}
	for k, v in kv_generator
	do
		t[k] = v
	end
	return t
end

local function generate_table(length, ttype)
	local kv_generator = KVPairGenFactory(length, ttype)
	return _generate_table(kv_generator)
end

-- QUESTION: will 'os.clock' do?
--
-- Benchmarks function on arbitrary input provided.
local function benchmark(f, ...)
	local start = os.clock()
	f(...)
	local finish = os.clock()
	return finish - start
end

-- Factory to create a comparator of two arbitrary functions
-- (meant to be doing something similar :) ) provided with the same arguments.
local function ComparatorFactory(f1, f2, compared_name)
	return { get_diff_and_ratio = function(...)
			local t1 = benchmark(f1, ...)
			local t2 = benchmark(f2, ...)
			return t1 - t2,
			       (t1 + BENCHMARK_DELTA)/(t2 + BENCHMARK_DELTA)
		end,
		compared_name = compared_name
	}
end

local function create_comparators(compared_module, function_list)
	local c_list = {}
	if (function_list == nil) then
		function_list = {}
		for func, _ in pairs(compared_module) do
			table.insert(function_list, func)
		end
	end
	local naive = require("naive") -- probably, this one can be as well supplied as an arg
	for _, func in ipairs(function_list)
	do
		assert(compared_module[func], "No implementation of '" .. func .. "' in bencmarked module.")
		assert(naive[func], "No naive implementation of  '" .. func .. "'.")
		table.insert(c_list,
			     ComparatorFactory(naive[func], compared_module[func], func))
	end
	return c_list
end

return {
	create_comparators = create_comparators,
	generate_table = generate_table,
	TABLE_TYPES = TABLE_TYPES
}
