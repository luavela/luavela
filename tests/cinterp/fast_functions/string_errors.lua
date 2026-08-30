-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

do -- string.char(number), number < 0
	local string_char_below_zero = function()
		local ok, msg = pcall(function() string.char(-1) end)
		return ok, msg
	end

	local ok, msg = cinterpcall(string_char_below_zero)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'char' (invalid value)")
end

do -- string.char(number), number > 255
	local string_char_above_255 = function()
		local ok, msg = pcall(function() string.char(256) end)
		return ok, msg
	end

	local ok, msg = cinterpcall(string_char_above_255)
	assert(not ok)
	assert_ends_with(msg, "bad argument #1 to 'char' (invalid value)")
end

do
	--[[
	* fn - function name
	* et - expected type
	* as - arguments
	* en - index of bad argument

	Basically, we do:
			string[fn](table.unpack(as))

	and check that:
			type(as[en]) ~= et

	]]--

	local tests = {
		-- string.len() - missing first arg
		{ fn = "len", et = "string", as = {}, en = 1 },
		-- string.len(table) - first arg not a string
		{ fn = "len", et = "string", as = {{}}, en = 1 },

		-- string.byte() - missing first arg
		{ fn = "byte", et = "string", as = {}, en = 1 },
		-- string.byte(table) - first arg not a string
		{ fn = "byte", et = "string", as = {{}}, en = 1 },
		-- string.byte(string, table) - second arg not a string
		{ fn = "byte", et = "number", as = {"hello", {}}, en = 2 },

		-- string.char(string) - first arg not a number
		{ fn = "char", et = "number", as = {"hello"}, en = 1 },
		-- string.char(number, string) - second arg not a number
		{ fn = "char", et = "number", as = {80, "hello"}, en = 2 },

		-- string.sub() - missing first arg
		{ fn = "sub", et = "string", as = {}, en = 1 },
		-- string.sub(string) - missing second arg
		{ fn = "sub", et = "number", as = {"hello"}, en = 2 },
		-- string.sub(table) - first arg not a string
		{ fn = "sub", et = "string", as = {{}}, en = 1 },
		-- string.sub(string, table) - second arg not a string
		{ fn = "sub", et = "number", as = {"hello", {}}, en = 2 },
		-- string.sub(string, number, table) - third arg not a string
		{ fn = "sub", et = "number", as = {"hello", 2, {}}, en = 3 },

		-- string.rep() - missing first arg
		{ fn = "rep", et = "string", as = {}, en = 1 },
		-- string.rep(string) - missing second arg
		{ fn = "rep", et = "number", as = {"hello"}, en = 2 },
		-- string.rep(table) - first arg not a string
		{ fn = "rep", et = "string", as = {{}}, en = 1 },
		-- string.rep(string, table) - second arg not a number
		{ fn = "rep", et = "number", as = {"hello", {}}, en = 2 },

		-- string.reverse() - missing first arg
		{ fn = "reverse", et = "string", as = {}, en = 1 },
		-- string.reverse(table) - first arg not a string
		{ fn = "reverse", et = "string", as = {{}}, en = 1 },

		-- string.lower() - missing first arg
		{ fn = "lower", et = "string", as = {}, en = 1 },
		-- string.lower(table) - first arg not a string
		{ fn = "lower", et = "string", as = {{}}, en = 1 },

		-- string.upper() - missing first arg
		{ fn = "upper", et = "string", as = {}, en = 1 },
		-- string.upper(table) - first arg not a string
		{ fn = "upper", et = "string", as = {{}}, en = 1 },
	}

	local do_test = function(func_name, expected_type, args, err_arg_no)
		local test = function()
			local ok, msg = pcall(function()
				local f = string[func_name]
				f(unpack(args))
			end)
			return ok, msg
		end

		local ok, msg = cinterpcall(test)
		assert(not ok)

		local got_type = "no value"
		if args[err_arg_no] ~= nil then
			got_type = type(args[err_arg_no])
		end

		err_msg = string.format(
			"bad argument #%d to 'f' (%s expected, got %s)",
			err_arg_no, expected_type, got_type)

		assert_ends_with(msg, err_msg)
	end

	for _, t in ipairs(tests) do
		do_test(t.fn, t.et, t.as, t.en)
	end
end

