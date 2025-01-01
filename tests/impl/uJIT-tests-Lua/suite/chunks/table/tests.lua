-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local naive = require("naive")
local utils = require("utils")

local function assert_shallowcopy(sample, new_t)
	utils.assert_shallow_equal(sample.t, new_t)
end

local function assert_keys(sample, new_t)
	utils.assert_count(sample.t, new_t)
	utils.assert_array_table(new_t)
	utils.assert_deep_values(sample.keys_as_values, new_t)
	utils.assert_deep_equal(naive.keys(sample.t), new_t)
end

local function assert_values(sample, new_t)
	utils.assert_count(sample.t, new_t)
	utils.assert_array_table(new_t)
	utils.assert_deep_values(sample.values_as_values, new_t)
	utils.assert_deep_equal(naive.values(sample.t), new_t)
end

local function assert_toset(sample, new_t)
	utils.assert_icount(sample.t, utils.count(new_t))
	utils.assert_deep_equal(naive.toset(sample.t), new_t)
end

return {
	shallowcopy = assert_shallowcopy,
	keys = assert_keys,
	values = assert_values,
	toset = assert_toset
}
