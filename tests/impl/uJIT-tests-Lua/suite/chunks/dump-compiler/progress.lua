-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local started, fname_real

started, fname_real = ujit.dump.start("-")
assert(started)
assert(fname_real == "-")

started, fname_real = ujit.dump.start("-") -- already started, expected to fail
assert(not started)
assert(fname_real == nil)

local sum = 0
for i = 1, 1E6 do
    sum = sum + i
end

print(sum)

local buffer = {}
for i = 1, 1E3 do
    table.insert(buffer, string.char(i % 255))
end

assert(    ujit.dump.stop())
assert(not ujit.dump.stop()) -- already stopped
