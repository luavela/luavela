-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local jit_on = jit.status()
assert(jit_on, "JIT must be on for this test")

jit.opt.start("hotloop=3")

local offsets = {
    { foo = -10 }, -- interpreter
    { foo =   5 }, -- interpreter
    { foo = -10 }, -- interpreter
    { foo =   5 }, -- recording
    { foo = -10 }, -- mcode
    { foo =   5 }, -- mcode
    { foo = -10 }, -- mcode
}

local S = 0
local N = #offsets
for i = 1, N do
    -- HREFK is emitted when accessing object's field by constant key below:
    S = S + i + offsets[i].foo
end

assert(S == 3)
