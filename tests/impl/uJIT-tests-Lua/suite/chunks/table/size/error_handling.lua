-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function wrong_arg_type_test(arg)
    assert(type(arg) ~= 'table')
    local ok, msg = pcall(ujit.table.size, arg)
    assert(not ok, "Calling ujit.table.size with non-table argment doesn't fail")
    assert(string.match(msg, "bad argument #1"),
        "Error message ('" .. msg .. "') doesn't contain 'bad argument #1'")
end

wrong_arg_type_test(nil)
wrong_arg_type_test(42)
