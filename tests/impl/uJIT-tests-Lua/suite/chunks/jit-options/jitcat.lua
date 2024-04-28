-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local jit_on = jit.status()
assert(jit_on, "JIT must be on for this test")

jit.opt.start("hotloop=3")

local s = ''
for i = 1, 10 do
    s = s .. i
end

assert(s == '12345678910')
