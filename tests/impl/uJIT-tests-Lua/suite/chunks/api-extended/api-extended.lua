-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local table_size = ujit.table.size

assert(table_size(ujit) == 13)

assert(type(ujit.getmetrics) == "function")
assert(type(ujit.immutable) == "function")
assert(type(ujit.seal) == "function")
assert(type(ujit.usesfenv) == "function")

-- ujit.coverage
assert(table_size(ujit.coverage) == 4)

assert(type(ujit.coverage.pause) == "function")
assert(type(ujit.coverage.start) == "function")
assert(type(ujit.coverage.stop) == "function")
assert(type(ujit.coverage.unpause) == "function")

-- ujit.debug
assert(table_size(ujit.debug) == 1)
assert(type(ujit.debug.gettableinfo) == "function")

-- ujit.dump
assert(table_size(ujit.dump) == 7)

assert(type(ujit.dump.bc) == "function")
assert(type(ujit.dump.bcins) == "function")
assert(type(ujit.dump.mcode) == "function")
assert(type(ujit.dump.stack) == "function")
assert(type(ujit.dump.start) == "function")
assert(type(ujit.dump.stop) == "function")
assert(type(ujit.dump.trace) == "function")

-- ujit.iprof
assert(table_size(ujit.iprof) == 5)

assert(type(ujit.iprof.PLAIN) == "string")
assert(type(ujit.iprof.INCLUSIVE) == "string")
assert(type(ujit.iprof.EXCLUSIVE) == "string")
assert(type(ujit.iprof.start) == "function")
assert(type(ujit.iprof.stop) == "function")

-- ujit.math
assert(table_size(ujit.math) == 6)

assert(type(ujit.math.isfinite) == "function")
assert(type(ujit.math.isinf) == "function")
assert(type(ujit.math.isnan) == "function")
assert(type(ujit.math.isninf) == "function")
assert(type(ujit.math.ispinf) == "function")
assert(type(ujit.math.nan) == "number")

-- ujit.memprof
assert(table_size(ujit.memprof) == 2)

assert(type(ujit.memprof.start) == "function")
assert(type(ujit.memprof.stop) == "function")

-- ujit.profile
assert(table_size(ujit.profile) == 5)

assert(type(ujit.profile.available) == "function")
assert(type(ujit.profile.init) == "function")
assert(type(ujit.profile.start) == "function")
assert(type(ujit.profile.stop) == "function")
assert(type(ujit.profile.terminate) == "function")

-- ujit.string
assert(table_size(ujit.string) == 2)

assert(type(ujit.string.split) == "function")
assert(type(ujit.string.trim) == "function")

-- ujit.table
assert(table_size(ujit.table) == 6)

assert(type(ujit.table.keys) == "function")
assert(type(ujit.table.rindex) == "function")
assert(type(ujit.table.shallowcopy) == "function")
assert(type(ujit.table.size) == "function")
assert(type(ujit.table.toset) == "function")
assert(type(ujit.table.values) == "function")
