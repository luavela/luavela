-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

-- several functions have more than one testcase to cover some edge cases
-- (for other functions they are omitted since they are defined with the same macro)
local math_funcs_test_data = {
	-- func   arg         expected
	{'abs',   {-1.5},       {'1.5'}},
	{'abs',   {1.9, {}},    {'1.9'}}, -- 2nd and further args are ignored
	{'floor', {0.4},        {'0'}},
	{'ceil',  {1.01},       {'2'}},
	{'sqrt',  {100},        {'10'}},
	{'log10', {1000},       {'3'}},
	{'exp',   {2.302585},   {'9.999'}},
	{'sin',   {1.5707963},  {'1'}},
	{'cos',   {0},          {'1'}},
	{'tan',   {2.35619},    {'-1.00'}},
	{'asin',  {0.8},        {'0.927'}},
	{'acos',  {0.8},        {'0.643'}},
	{'atan',  {0.5},        {'0.463'}},
	{'sinh',  {1.5},        {'2.129'}},
	{'cosh',  {1.5},        {'2.352'}},
	{'tanh',  {1.5},        {'0.905'}},
	{'deg',   {0.785},      {'44.97'}},
	{'rad',   {45},         {'0.785'}},
	{'log',   {2},          {'0.693'}}, -- log accepts exactly 1 arg
	{'log',   {2, 10},      {'0.301'}}, -- otherwise calls a fallback
	{'log',   {2, 10, 20},  {'0.301'}}, -- function
	{'log',   {2, 10, {}},  {'0.301'}},
	{'atan2', {1, 2},       {'0.463'}},
	{'atan2', {1, 2, {}},   {'0.463'}},
	{'pow',   {2, 3},       {'8'}},
	{'fmod',  {3.5, 1.1},   {'0.2'}},
	{'ldexp', {2, 3},       {'16'}},
	{'frexp', {20},         {'0.625', '5'}},
	{'frexp', {20, 30},     {'0.625', '5'}},
	{'modf',  {10.15},      {'10', '0.15'}},
	{'modf',  {10.15, 1.3}, {'10', '0.15'}},
	{'min',   {2},          {'2'}},
	{'min',   {2, 1},       {'1'}},
	{'min',   {2, 0.1, 1},  {'0.1'}},
	{'max',   {1},          {'1'}},
	{'max',   {2, 1},       {'2'}},
	{'max',   {2, 1, 3.1},  {'3.1'}}
}

local math_call = function(func_name, args)
	return {math[func_name](unpack(args))}
end

local caller = function(func_name, args)
	local ok, msg = pcall(math_call, func_name, args)
	return ok, msg
end

local iterate_and_check_errmsg = function(wrong_args, func_name)
	for _, wrong_args_vals in ipairs(wrong_args) do
		local args, expected_err_msg = unpack(wrong_args_vals)
		local ok, msg = cinterpcall(caller, func_name, args)
		assert(not ok)
		assert_ends_with(msg, expected_err_msg)
	end
end

--------------------------- TEST CASES ---------------------------

do -- all builtins must return expected values
	local cmp_results = function(got, expected)
		assert(#got == #expected)
		for i = 1, #got do
			assert(tostring(got[i]):sub(1, 5) == expected[i])
		end
	end

	for _, v in ipairs(math_funcs_test_data) do
		local func_name, args, expected_res = unpack(v)
		local ret = cinterpcall(math_call, func_name, args)
		cmp_results(ret, expected_res)
	end
end

do -- every builtin should fail if first argument is nil or not a number
	local wrong_args = {
		{{},      "bad argument #1 to '?' (number expected, got no value)"}, -- 1st arg is nil
		{{false}, "bad argument #1 to '?' (number expected, got boolean)"} -- 1st arg has wrong type (boolean)
	}

	for _, v in ipairs(math_funcs_test_data) do
		local func_name = v[1]
		iterate_and_check_errmsg(wrong_args, func_name)
	end
end

do -- check that second argument is also validated (only for builtins where 2nd arg is present)
	local wrong_args = {
		{{1.0},        "bad argument #2 to '?' (number expected, got no value)"}, -- 2nd arg is nil
		{{1.0, false}, "bad argument #2 to '?' (number expected, got boolean)"} -- 2nd arg has wrong type (boolean)
	}

	for _, func_name in ipairs({'atan2', 'pow', 'fmod', 'ldexp'}) do
		iterate_and_check_errmsg(wrong_args, func_name)
	end

end

do -- additional math.min and math.max wrong arguments tests
	local wrong_args = {
		{{},           "bad argument #1 to '?' (number expected, got no value)"},
		{{false},      "bad argument #1 to '?' (number expected, got boolean)"},
		{{1, {}},      "bad argument #2 to '?' (number expected, got table)"},
		{{1, 2, true}, "bad argument #3 to '?' (number expected, got boolean)"}
	}

	for _, func_name in ipairs{'min', 'max'} do
		iterate_and_check_errmsg(wrong_args, func_name)
	end
end

do -- check that ffunc can be restarted after fallback (FFH_RETRY case)
	local foo = function()
		local ret = math.abs("42") -- fallback will convert string to number and call ffunc once again
		return ret
	end

	assert(cinterpcall(foo) == 42)
end

do -- same as above but returns to C frame
	local foo = function()
		return math.abs("42")
	end

	assert(cinterpcall(foo) == 42)
end
