-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

assert(jit.status() == false)

local metrics
local _ = ujit.getmetrics()

metrics = ujit.getmetrics()
--strhash_hit and strhash_miss are already registered
assert(metrics.strhash_hit  == 2, metrics.strhash_hit)
assert(metrics.strhash_miss == 14, metrics.strhash_miss)

metrics = ujit.getmetrics()
assert(metrics.strhash_hit  == 16, metrics.strhash_hit)
assert(metrics.strhash_miss == 0, metrics.strhash_miss)

local str1  = "strhash" .. "_hit"

metrics = ujit.getmetrics()
assert(metrics.strhash_hit  == 17, metrics.strhash_hit)
assert(metrics.strhash_miss == 0, metrics.strhash_miss)

metrics = ujit.getmetrics()
assert(metrics.strhash_hit  == 16, metrics.strhash_hit)
assert(metrics.strhash_miss == 0, metrics.strhash_miss)

local str2 = "new" .. "string"

metrics = ujit.getmetrics()
assert(metrics.strhash_hit  == 16, metrics.strhash_hit)
assert(metrics.strhash_miss == 1, metrics.strhash_miss)
