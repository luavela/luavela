-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function _is_table(t)
	return (type(t) == "table")
end

local function _assert_is_table(t)
	assert(_is_table(t),
	       string.format("Object '%s' is not of 'table' type.", tostring(t)))
end

-- Returns count of all elements in a table.
local function _count(t)
	_assert_is_table(t)
	local count = 0
	for _, _ in pairs(t) do
		count = count + 1
	end
	return count
end

-- Returns count of elements in a table with consequent integer keys.
-- NB: '#' can't be used as value for table with first nil element is undefined.
local function _icount(t)
	_assert_is_table(t)
	local count = 0
	for _, _ in ipairs(t) do
		count = count + 1
	end
	return count
end

-- Converts table to string only at the top-depth level.
-- Dedicated for short-printing tables in asserts.
local function shallow_tostring(t)
	if (_is_table(t) == false) then
		return tostring(t)
	end
	-- Tables can be rather huge. Truncation makes sense.
	local TABLE_TRUNC_SENTINEL = 10
	local TRUNC_STRING = "..."
	local result = "{"
	local trunc_count = 1
	for k, v in pairs(t) do
		local delim = trunc_count ~= 1 and ", " or ""
		local lk = (_is_table(k) == false and tostring(k)) or "table"
		local lv = (_is_table(v) == false and tostring(v)) or "table"

		result = string.format("%s%s%s: %s", result, delim, lk, lv)

		if (trunc_count == TABLE_TRUNC_SENTINEL) then
			result = result .. TRUNC_STRING
			break
		end
		trunc_count = trunc_count + 1
	end
	result = result .. "}"
	return result
end

local function deep_equal_table(t1, t2)
	if (_count(t1) ~= _count(t2)) then
		return false
	end
	for k, v in pairs(t1) do
		if (t2[k] == nil) then
			return false
		end
		if (type(t2[k]) ~= type(v)) then
			return false
		end
		if (_is_table(v)) then
			if (deep_equal_table(v, t2[k]) == false) then
				return false
			end
		elseif (t2[k] ~= v) then
			return false
		end
	end
	return true
end

local function shallow_equal_table(t1, t2)
	if (_count(t1) ~= _count(t2)) then
		return false
	end
	for k, v in pairs(t1) do
		if (t2[k] == nil) then
			return false
		end
		if (type(t2[k]) ~= type(v)) then
			return false
		end
		if (t2[k] ~= v) then
			return false
		end
	end
	return true
end

local function _has_object(t, obj_sample, is_key, is_deep)
	local equal_compare = is_deep and deep_equal_table or shallow_equal_table
	local is_table = _is_table(obj_sample)

	for k, v in pairs(t) do
		local obj = is_key and k or v
		if (type(obj) == type(obj_sample)) then
			if (_is_table(obj) and is_table) then
				if (equal_compare(obj, obj_sample) == true) then
					return true
				end
			elseif (obj == obj_sample) then
				return true
			end
		end
	end
	return false
end

local function _has_value(t, value, is_deep)
	return _has_object(t, value, false, is_deep)
end

local function _has_key(t, key)
	return _has_object(t, key, true, false)
end

local function _assert_values(t1, t2, is_deep)
	_assert_is_table(t1)
	_assert_is_table(t2)

	local t1_str = shallow_tostring(t1)
	local t2_str = shallow_tostring(t2)

	for _, v in pairs(t1) do
		assert(_has_value(t2, v, is_deep),
		       string.format("Value '%s' of table '%s' is not among table '%s' values.",
				     tostring(v), t1_str, t2_str))
	end
end

local function assert_deep_values(t1, t2)
	_assert_values(t1, t2, true)
end

local function assert_shallow_values(t1, t2)
	_assert_values(t1, t2, false)
end

local function assert_keys(t1, t2)
	_assert_is_table(t1)
	_assert_is_table(t2)

	local t1_str = shallow_tostring(t1)
	local t2_str = shallow_tostring(t2)

	for k, _ in pairs(t1) do
		assert(_has_key(t2, k),
		       string.format("Key '%s' of table '%s' is not among table '%s' keys.",
				     tostring(k), t1_str, t2_str))
	end
end

-- Checks that table has _only_ integer monotonically increasing keys starting from 1.
local function assert_array_table(t)
	assert(_is_table(t), string.format("Object is not a table: '%s'.", tostring(t)))
	assert(_icount(t) == _count(t),
	       string.format("Table '%s' has other keys apart from integer.", shallow_tostring(t)))
end

local function assert_deep_equal(t1, t2)
	assert(deep_equal_table(t1, t2),
	       string.format("Tables '%s' vs. '%s' don't match in deep comparison.",
			     shallow_tostring(t1), shallow_tostring(t2)))
end

local function assert_shallow_equal(t1, t2)
	assert(shallow_equal_table(t1, t2),
	       string.format("Tables '%s' vs. '%s' don't match in shallow comparison.",
			     shallow_tostring(t1), shallow_tostring(t2)))
end

local function assert_count(t1, t2)
	local t1_count = _count(t1)
	local t2_count = (type(t2) == "number") and t2 or _count(t2)
	assert(t1_count == t2_count,
	       string.format("Counts for tables '%s' vs. '%s'  don't match: '%d' vs. '%d'.",
			     shallow_tostring(t1), shallow_tostring(t2), t1_count, t2_count))
end

local function assert_icount(t1, t2)
	local t1_icount = _icount(t1)
	local t2_icount = (type(t2) == "number") and t2 or _icount(t2)
	assert(t1_icount == t2_icount,
	       string.format("Counts for tables '%s' vs. '%s'  don't match: '%d' vs. '%d'.",
			     shallow_tostring(t1), shallow_tostring(t2), t1_icount, t2_icount))
end

local function _assert_table_arg_error(func)
	local function _assert_call_result(call_result)
		assert(call_result == false,
		       string.format("Got 'true' instead of 'false' for 'ujit.table.%s' call with argument error.", func))
	end

	local function _assert_error_msg(error_msg, arg_type)
		local ERROR_FMT_ARG = "(table expected, got %s)"

		local expected_error_msg = string.format(ERROR_FMT_ARG, arg_type)
		assert(string.match(error_msg, expected_error_msg),
		       string.format("Error messages for 'ujit.table.%s' call don't match: '%s' vs. '%s'",
				     func, error_msg, expected_error_msg))
	end

	local args  = { nil, 2, "str", 34.3, true, function() local _ = 1 end}

	for _, arg in pairs(args) do
		local call_result, error_msg = pcall(ujit.table[func], arg)
		_assert_call_result(call_result)
		_assert_error_msg(error_msg, type(arg))
	end

	local call_result, error_msg = pcall(ujit.table[func])
	_assert_call_result(call_result)
	_assert_error_msg(error_msg, "no value")
end

local function table_creation_tests(tables, test_funcs)
	for _, funcs in ipairs(test_funcs) do
        print(string.format("testing '%s'", funcs.name))

		-- Test table creation on various inputs.
		for _, sample in pairs(tables) do
			assert(sample.t, "No required 't' field in table sample.")

			if (sample.mt) then
				setmetatable(sample.t, sample.mt)
			end

			local f = funcs.tested
			local assert_f = funcs.assert_f

			local new_t = f(sample.t)
			assert_f(sample, new_t)

			-- Make sure table created on trace is valid.
			local new_t_rec
			jit.opt.start("hotloop=1")
			for _ = 1, 3 do
				new_t_rec = f(sample.t)
			end
			jit.opt.start("hotloop=0")

			assert_f(sample, new_t_rec)
		end

		-- Test error handling.
		_assert_table_arg_error(funcs.name)
	end
end

return {
	deep_equal_table = deep_equal_table,
	shallow_equal_table = shallow_equal_table,
	assert_array_table = assert_array_table,
	assert_deep_values = assert_deep_values,
	assert_shallow_values = assert_shallow_values,
	assert_keys = assert_keys,
	assert_deep_equal = assert_deep_equal,
	assert_shallow_equal = assert_shallow_equal,
	assert_count = assert_count,
	assert_icount = assert_icount,
	shallow_tostring = shallow_tostring,
	count = _count,
	table_creation_tests = table_creation_tests,
}
