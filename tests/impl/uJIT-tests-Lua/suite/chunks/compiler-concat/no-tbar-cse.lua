-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

assert(jit.status() == true)
jit.opt.start(0, 'fold', 'loop', 'dse', 'cse', 'jitcat')

-- BUFSTR IR causes an allocation, so corresponding TBAR
-- should not be eliminated by the CSE optimization.

local N = 1e3
local out = {''}

for i = 1, N do
        out[i + 1] = "foo" .. out[i]
end

print("out_len=" .. #out)
