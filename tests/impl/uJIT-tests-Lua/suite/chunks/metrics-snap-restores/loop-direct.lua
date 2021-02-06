-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Compiled loop with a direct exit to the interpreter
--

jit.opt.start(0, "hotloop=2")

local sum = 0
for i = 1, 20 do
    sum = sum + i
end

local metrics = ujit.getmetrics()

-- A single snapshot restoration happened on loop finish:
assert(metrics.jit_snap_restore == 1)
