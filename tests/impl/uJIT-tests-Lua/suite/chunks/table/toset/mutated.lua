-- Ensure that ujit.table.toset works correctly on tables
-- mutated in various manner.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
require("jit").off()

local toset = require("ujit.table").toset
local tests = {}

function tests.tsetb()
	local t = {}
	t[1] = "abc"
	assert(#t == 1)

	local s = toset(t)
	assert(s.abc == true)
end

function tests.tsetv()
	local t = {}
	local i = 1
	t[i] = "abc"
	assert(#t == 1)

	local s = toset(t)
	assert(s.abc == true)
end

function tests.table_insert()
	local t = {}
	table.insert(t, "abc")
	assert(#t == 1)

	local s = toset(t)
	assert(s.abc == true)
end

function tests.set_first()
	local t = {nil, "def"}
	t[1] = "abc"
	assert(#t == 2)

	local s = toset(t)
	assert(s.abc == true)
	assert(s.def == true)
end

function tests.set_first_explicit_int_keys()
	local t = {[2] = "def"}
	t[1] = "abc"
	assert(#t == 2)

	local s = toset(t)
	assert(s.abc == true)
	assert(s.def == true)
end

function tests.nil_first()
	local t = {"abc", "def"}
	t[1] = nil
	-- assert(#t == ?) -- nope, not a sequence.

	local s = toset(t)
	assert(s.abc == nil)
	assert(s.def == nil)
end

function tests.nil_first_explicit_int_keys()
	local t = {[1] = "abc", [2] = "def"}
	t[1] = nil
	-- assert(#t == ?) -- nope, not a sequence.

	local s = toset(t)
	assert(s.abc == nil)
	assert(s.def == nil)
end

function tests.nil_last()
	local t = {"abc", "def"}
	t[2] = nil
	assert(#t == 1)

	local s = toset(t)
	assert(s.abc == true)
	assert(s.def == nil)
end

function tests.nil_last_explicit_int_keys()
	local t = {[1] = "abc", [2] = "def"}
	t[2] = nil
	assert(#t == 1)

	local s = toset(t)
	assert(s.abc == true)
	assert(s.def == nil)
end

for _, test in pairs(tests) do
	test()
end
