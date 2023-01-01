-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Compiled loop with a side exit which gets compiled
--

-- Optimization level is important herwe as loop optimization may
-- unroll the loop body and insert +1 side exit.
jit.opt.start(0, "hotloop=5", "hotexit=5")

local function foo(i)
    return i <= 10 and i or tostring(i)
end

local sum = 0
for i = 1, 20 do
    sum = sum + foo(i)
end

local metrics = ujit.getmetrics()

-- 5 side exits to the interpreter before it gets hot and compiled
-- 1 side exit on loop end
assert(metrics.jit_snap_restore == 6)
