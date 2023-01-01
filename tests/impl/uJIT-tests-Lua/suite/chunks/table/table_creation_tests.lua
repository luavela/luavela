-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local data = require("data")
local tests = require("tests")
local utils = require("utils")

local test_funcs = {}

-- functions from ujit.table which have form t2 = f(t1)
local table_creation_funcs = { 'keys', 'shallowcopy', 'values', 'toset' }

for _, fname in ipairs(table_creation_funcs) do
    local impl = ujit.table[fname]
    table.insert(test_funcs,
        {name = fname, tested = impl, assert_f = tests[fname]})
end

utils.table_creation_tests(data.tables, test_funcs)
