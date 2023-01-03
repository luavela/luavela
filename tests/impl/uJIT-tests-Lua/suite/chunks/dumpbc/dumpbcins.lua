-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function test(expected_res, ...)
    local res, msg = pcall(...)
    assert(res == expected_res)
    print(msg)
end

local function foo(bar)
    if bar > 5 then
        return bar - 10
    end
    return 'return'
end

assert(ujit.dump.bcins)

test(false, ujit.dump.bcins, nil)
test(false, ujit.dump.bcins, nil, foo, 0)
test(true , ujit.dump.bcins, io.stdout, foo, 0)
test(true , ujit.dump.bcins, io.stdout, test, 0)
test(true , ujit.dump.bcins, io.stdout, math.sin, 0)
-- pc is ignored when dumping non-Lua function:
test(true , ujit.dump.bcins, io.stdout, math.sin, -100500)
test(false, ujit.dump.bcins, io.stdout, foo, -100500)
test(false, ujit.dump.bcins, io.stdout, foo, 1 / 0)
test(false, ujit.dump.bcins, io.stdout, foo, 0 / 0)
test(false, ujit.dump.bcins, io.stdout, foo, 1E20)

assert(ujit.dump.bcins(io.stdout, foo, 10000) == false)
assert(ujit.dump.bcins(io.stdout, foo, 8)     == true)
