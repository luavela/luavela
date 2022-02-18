-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

assert(jit.status() == false)

local metrics = ujit.getmetrics()

-- Reset counters
metrics = ujit.getmetrics()

collectgarbage()

metrics = ujit.getmetrics()

assert(metrics.gc_allocated > 0)
local getmetrics_alloc = metrics.gc_allocated

-- NB: Avoid operations that use internal global string buffer
-- (such as concatenation, string.format, table.concat)
-- while creating the string. Otherwise gc_freed/gc_allocated relations
-- will not be so straightforward.
local str = string.sub("Hello, world", 1, 5)
collectgarbage()

metrics = ujit.getmetrics()

assert(metrics.gc_allocated > getmetrics_alloc)
assert(metrics.gc_freed == getmetrics_alloc)

str = string.sub("Hello, world", 8, -1)

metrics = ujit.getmetrics()

assert(metrics.gc_allocated > getmetrics_alloc)
assert(metrics.gc_freed == 0)
collectgarbage()

metrics = ujit.getmetrics()
assert(metrics.gc_allocated == getmetrics_alloc)
assert(metrics.gc_freed > 2 * getmetrics_alloc)
