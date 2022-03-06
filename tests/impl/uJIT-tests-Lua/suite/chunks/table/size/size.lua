-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- { table, expected_size }
local test_cases = {
    { {}, 0 },
    { { 1 }, 1 },
    { { 1, 2, 3, 4 }, 4 },
    { { hello = 2 }, 1 },
    { { hello = 2, world = 3 }, 2 },
    { { hello = 2, world = 4, 1, 2, 3 }, 5 },
    { { 1, 2, 3, nil, 4 }, 4 },
    { { 1, 2, 3, nil, 4, nil, 5 }, 5 },
    { { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 }, 18 },
    { { 1, 2, [100] = 5 }, 3 },
    { { [100] = 5, [1000] = 2 }, 2 },
    { { [-1] = 2, [1] = 5, [20] = 10 }, 3 }
}

for i, test_case in ipairs(test_cases) do
    local t, expected_size = table.unpack(test_case)
    local size = ujit.table.size(t)
    assert(size == expected_size,
        "Test case #" .. i .. " failed: expected table to have size = " .. expected_size ..
        ", but ujit.table.size(t) == " .. size)
end

