-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Compiled scalar trace with a direct exit to the interpreter
--

-- For calls it will be 2 * hotloop (see lj_dispatch.{c,h} + hotcall@vm_x86.dasc)
jit.opt.start(3, "hotloop=2", "hotexit=3")

local function foo(i)
    return i <= 15 and i or tostring(i)
end

local metrics

foo( 1) -- interp only
foo( 2) -- interp only
foo( 3) -- interp only
foo( 4) -- compile trace during this call
foo( 5) -- follow the trace
foo( 6) -- follow the trace
foo( 7) -- follow the trace
foo( 8) -- follow the trace
foo( 9) -- follow the trace
foo(10) -- follow the trace

metrics = ujit.getmetrics()

-- No exits triggering snap restore so far: snapshot
-- restoration was inlined into the machine code.
assert(metrics.jit_snap_restore == 0)

-- Simply 2 side exits from the trace:
foo(20)
foo(21)

metrics = ujit.getmetrics()
assert(metrics.jit_snap_restore == 2)
