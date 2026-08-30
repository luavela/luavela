-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'

local cinterpcall = ujit.debug.cinterpcall

local tests = {
	-- func, arg, expected
	-- tobit
	{ "tobit", 0, 0 },
	{ "tobit", 42, 42 },
	{ "tobit", -42, -42 },
	{ "tobit", 0xffffffd6, bit.tobit(0xffffffd6) },
	{ "tobit", 0x80000000, -2147483648 }, -- INT32_MIN
	{ "tobit", 0x7fffffff, 2147483647 }, -- INT32_MAX
	{ "tobit", 0xffffffff, -1},
	{ "tobit", 0xffffffff + 1, 0 }, -- overflow
	{ "tobit", 0xffffffff + 2, 1 }, -- overflow
	{ "tobit", {42, 43, 44}, 42 }, -- extra args
	-- bnot
	{ "bnot", 0, -1 },
	{ "bnot", 42, -43 },
	{ "bnot", -42, 41 },
	{ "bnot", 0xffffffff, 0 },
	{ "bnot", 0xffffffff + 1, -1 }, -- arg overflows to 0
	{ "bnot", 0xffffffff + 2, -2 }, -- args overflows to 1
	{ "bnot", {0, 1, 2, 3}, -1 }, -- extra args
	-- bswap
	{ "bswap", 0, 0 },
	{ "bswap", 0x12345678, 0x78563412 },
	{ "bswap", 0x78563412, 0x12345678 },
	{ "bswap", 0xffffffd6, bit.tobit(0xd6ffffff) },
	{ "bswap", {0, 1, 2, 3}, 0 }, -- extra args
	-- lshift
	{ "lshift", {0x0000002a, -1}, 0 },
	{ "lshift", {0x0000002a, 0}, 0x000002a },
	{ "lshift", {0x0000002a, 4}, 0x00002a0 },
	{ "lshift", {0x0000002a, 8}, 0x0002a00 },
	{ "lshift", {0x87654321, 12}, 0x54321000 },
	-- rshift
	{ "rshift", {0x000000d6, 0}, 0x000000d6 },
	{ "rshift", {0x000000d6, 4}, 0x0000000d },
	{ "rshift", {0xffffffd6, 4}, 0x0ffffffd }, -- sign bit not carried
	-- arshift
	{ "arshift", {0x000000d6, 0}, 0x000000d6 },
	{ "arshift", {0x000000d6, 4}, 0x0000000d },
	{ "arshift", {0xffffffd6, 4}, bit.tobit(0xfffffffd) }, -- sign bit carried
	-- rol
	{ "rol", {0x76543210, 0},  0x76543210 },
	{ "rol", {0x76543210, 4},  0x65432107 },
	{ "rol", {0x76543210, 8},  0x54321076 },
	{ "rol", {0x76543210, 60}, 0x07654321 },
	{ "rol", {0x76543210, 64}, 0x76543210 },
	{ "rol", {0x76543210, -4}, 0x07654321 },
	-- ror
	{ "ror", {0x76543210, 0},  0x76543210 },
	{ "ror", {0x76543210, 4},  0x07654321 },
	{ "ror", {0x76543210, 8},  0x10765432 },
	{ "ror", {0x76543210, 60}, 0x65432107 },
	{ "ror", {0x76543210, 64}, 0x76543210 },
	{ "ror", {0x76543210, -4}, 0x65432107 },
	-- band
	{ "band", 0xfff, 0xfff },
	{ "band", {0xfff, 0xff0}, 0xff0 },
	{ "band", {0xfff, 0x0f0, 0xff}, 0xf0 },
	{ "band", {-1, -1}, -1 }, -- the result is signed
	-- bor
	{ "bor", 0xfff, 0xfff },
	{ "bor", {0xfff, 0xff0}, 0xfff },
	{ "bor", {0xf00, 0x0f0, 0xf}, 0xfff },
	{ "bor", {-1, -1}, -1 }, -- the result is signed
	-- bxor
	{ "bxor", 0xfff, 0xfff },
	{ "bxor", {0xfff, 0x000}, 0xfff },
	{ "bxor", {0xfff, 0x0f0}, 0xf0f },
	{ "bxor", {-1, 1}, -2 }, -- the result is signed
}

local makeErrorString = function(fname, arg, expected, actual)
	if type(arg) == 'number' then
		return string.format("bit.%s(%d) - expected: %d (0x%s), got: %d (0x%s)",
			fname, arg, expected, bit.tohex(expected), actual, bit.tohex(actual))
	end

	assert(type(arg) == 'table')
	local argStrs = {}
	for i, v in ipairs(arg) do
		argStrs[i] = bit.tohex(v)
	end
	return string.format("bit.%s(%s) - expected: %d (0x%s), got: %d (0x%s)",
		fname, table.concat(argStrs, ", "), expected, bit.tohex(expected), actual, bit.tohex(actual))
end

for i, t in ipairs(tests) do
	local fname, arg, expected = t[1], t[2], t[3]

	local test = function()
		if type(arg) == 'table' then
			return bit[fname](unpack(arg))
		end
		return bit[fname](arg)
	end

	local ret = cinterpcall(test)
	local errmsg
	if ret ~= expected then
		errmsg = makeErrorString(fname, arg, expected, ret)
	end
	assert(ret == expected, errmsg)
end
