-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Check coverage interaction with hotcounting/trace recording.
-- Trace with COVERG will be compiled as no-op.
jit.opt.start("hotloop=2")

local sum = 0
for i=1,7 do
  sum = sum + i
end
assert(sum == 28)
