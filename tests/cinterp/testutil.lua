-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local find_actual_value = function(actual, key)
	for idx, kv in ipairs(actual) do
		if kv[1] == key then
			return kv[2]
		end
	end
end

-- Compares results of iteration in ordered or unordered manner
-- The results should be contained in a table in the following form:
-- { { key1, value1 }, { key2, value2 }, ... }
local assert_iteration_results = function(actual, expected, ordered)
	assert(#actual == #expected,
		string.format("iteration didn't produce the expected number of items: expected %d, got %d", #expected, #actual))
	for idx, kv in ipairs(expected) do
		expected_key, expected_value = kv[1], kv[2]
		if ordered then
			actual_key, actual_value = actual[idx][1], actual[idx][2]
		else
			actual_value = find_actual_value(actual, expected_key)
		end
		assert(actual_value ~= nil,
			string.format("expected to find (%s, %s) pair in actual results",
				expected_key, expected_value))
		assert(actual_value == expected_value,
			string.format("expected != actual: values differ for key \"%s\" (%s != %s)",
				expected_key, expected_value, actual_value))
	end
end

local assert_iteration_results_ordered = function(actual, expected)
	assert_iteration_results(actual, expected, true)
end

local ends_with = function(str, suffix)
	return suffix == "" or str:sub(-#suffix) == suffix
end

local assert_ends_with = function(str, suffix)
		assert(ends_with(str, suffix),
			string.format("expected '%s' to end with '%s'",
				str, suffix))
end

return {
	assert_iteration_results = assert_iteration_results,
	assert_iteration_results_ordered = assert_iteration_results_ordered,
	assert_ends_with = assert_ends_with,
}
