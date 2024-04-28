-- Ensure that ujit.table.toset handles correctly various literal tables.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

require("jit").off()

local toset = require("ujit.table").toset
local tests = {}

function tests.explicit_int_keys1()
	local t = {[1] = "abc"}

	local s = toset(t)
	assert(s.abc == true)
end

function tests.explicit_int_keys2()
	local t = {"abc", [2] = "def"}
	assert(#t == 2)

	local s = toset(t)
	assert(s.abc == true)
	assert(s.def == true)
end

function tests.explicit_int_keys3()
	local t = {[2] = "def", "abc"}
	assert(#t == 2)

	local s = toset(t)
	assert(s.abc == true)
	assert(s.def == true)
end

for _, test in pairs(tests) do
	test()
end
