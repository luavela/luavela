-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Compiled loop with a side exit which does not get compiled
--

jit.opt.start(0, "hotloop=2", "hotexit=2")

local function foo(i)
    return i <= 5 and i or string.rep("a", i):len() -- string.rep is not compiled!
end

local sum = 0
for i = 1, 10 do
    sum = sum + foo(i)
end

local metrics = ujit.getmetrics()

-- Side exits from the root trace could not get compiled
assert(metrics.jit_snap_restore == 5)
