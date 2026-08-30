-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'
local assert_ends_with = tu.assert_ends_with

local cinterpcall = ujit.debug.cinterpcall

do
	local tests = {
		-- function_name, expected_type, args, error_arg_index

		-- bit.tobit() - missing first arg
		{ "tobit", "number", {}, 1 },
		-- bit.tobit({}) - first arg not a number
		{ "tobit", "number", {{}}, 1 },

		-- bnot
		{ "bnot", "number", {}, 1 },
		{ "bnot", "number", {{}}, 1 },

		-- bit.lshift() - missing first arg
		{ "lshift", "number", {}, 1 },
		-- bit.lshift(42) - missing second arg
		{ "lshift", "number", {42}, 2 },
		-- bit.lshift({}) - first arg not a number
		{ "lshift", "number", {{}}, 1 },
		-- bit.lshift(42, {}) - second arg not a number
		{ "lshift", "number", {42, {}}, 2 },

		-- rshift
		{ "rshift", "number", {}, 1 },
		{ "rshift", "number", {42}, 2 },
		{ "rshift", "number", {{}}, 1 },
		{ "rshift", "number", {42, {}}, 2 },

		-- arshift
		{ "arshift", "number", {}, 1 },
		{ "arshift", "number", {42}, 2 },
		{ "arshift", "number", {{}}, 1 },
		{ "arshift", "number", {42, {}}, 2 },

		-- rol
		{ "rol", "number", {}, 1 },
		{ "rol", "number", {{}}, 1 },

		-- ror
		{ "ror", "number", {}, 1 },
		{ "ror", "number", {{}}, 1 },

		-- band
		{ "band", "number", {}, 1 },
		{ "band", "number", {{}}, 1 },

		-- bor
		{ "bor", "number", {}, 1 },
		{ "bor", "number", {{}}, 1 },

		-- bxor
		{ "bxor", "number", {}, 1 },
		{ "bxor", "number", {{}}, 1 },
	}

	local do_test = function(func_name, expected_type, args, err_arg_no)
		local test = function()
			local ok, msg = pcall(function()
				local f = bit[func_name]
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
		local func_name, expected_type, args, err_arg_no = t[1], t[2], t[3], t[4]
		do_test(func_name, expected_type, args, err_arg_no)
	end
end
