-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Note that metrics for atomic and finalize phases might spike in case of
-- compiled code execution.
assert(jit.status() == false)

local metrics

-- Some garbage collection has already happened before the next line, i.e.
-- during fronted processing this chunk. Let's put a full garbage collection
-- cycle on top of that, and confirm that non-null values are reported (we are
-- not yet interested in actual numbers):
collectgarbage("collect")
collectgarbage("stop")
metrics = ujit.getmetrics()
assert(metrics.gc_steps_pause       > 0)
assert(metrics.gc_steps_propagate   > 0)
assert(metrics.gc_steps_atomic      > 0)
assert(metrics.gc_steps_sweepstring > 0)
assert(metrics.gc_steps_sweep       > 0)
assert(metrics.gc_steps_finalize   == 0)

-- Previous call to ujit.getmetrics() should've flushed the GC metrics. As long
-- as we stopped the GC, consequent call should return nulls:
metrics = ujit.getmetrics()
assert(metrics.gc_steps_pause       == 0)
assert(metrics.gc_steps_propagate   == 0)
assert(metrics.gc_steps_atomic      == 0)
assert(metrics.gc_steps_sweepstring == 0)
assert(metrics.gc_steps_sweep       == 0)
assert(metrics.gc_steps_finalize    == 0)

-- Now the last phase: run full GC once and make sure that everything is being
-- reported as expected:
collectgarbage("collect")
collectgarbage("stop")
metrics = ujit.getmetrics()
assert(metrics.gc_steps_pause       == 1)
assert(metrics.gc_steps_propagate   >= 1)
assert(metrics.gc_steps_atomic      == 1)
assert(metrics.gc_steps_sweepstring >= 1)
assert(metrics.gc_steps_sweep       >= 1)
assert(metrics.gc_steps_finalize    == 0) -- Nothing to finalize, skipped.


-- We already know that previous call to ujit.getmetrics() flushed all GC
-- metrics. Now let's run three GC cycles to ensure that zero-to-one transition
-- was not a happy coincidence.
collectgarbage("collect")
collectgarbage("collect")
collectgarbage("collect")
collectgarbage("stop")
metrics = ujit.getmetrics()
assert(metrics.gc_steps_pause       == 3)
assert(metrics.gc_steps_propagate   >= 3)
assert(metrics.gc_steps_atomic      == 3)
assert(metrics.gc_steps_sweepstring >= 3)
assert(metrics.gc_steps_sweep       >= 3)
assert(metrics.gc_steps_finalize    == 0) -- Nothing to finalize, skipped.


