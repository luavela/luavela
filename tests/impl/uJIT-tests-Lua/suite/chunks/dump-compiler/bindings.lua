-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function foo(a)
    local x = a * 10
    for i = 1, a do
        x = x + i
    end
    return x
end

assert(foo(10000) == 50105000)

ujit.dump.trace(io.stdout, 1)
ujit.dump.mcode(io.stdout, 1)

-- Non-existent trace number:
ujit.dump.trace(io.stdout, 1E6)
ujit.dump.mcode(io.stdout, 1E6)

-- Zero arguments:
assert(not pcall(ujit.dump.trace))
assert(not pcall(ujit.dump.mcode))

-- One argument:
assert(not pcall(ujit.dump.trace, io.stderr))
assert(not pcall(ujit.dump.mcode, io.stderr))

-- Malformed first argument (not IO):
assert(not pcall(ujit.dump.trace, debug, 1))
assert(not pcall(ujit.dump.mcode, debug, 1))
