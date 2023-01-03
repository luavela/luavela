-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NB! Using memprof with enabled JIT is redundant.
-- Allocations from traces are stub-reported as internal.
-- See memprof sources for details.
jit.off()

local function payload()
    local t
    for i = 1, 100 do
        t = {"a", "b", "c", "d", "e"}
        if i % 10 == 0 then
            for j = 1, 50 do
                local key = "key" .. tostring(j)
                t[key] = j
            end
        end
    end
    return t
end

local memprof = require("ujit.memprof")

assert(not jit.status(), "JIT should be off")
assert(type(memprof) == "table")

assert(not pcall(memprof.start))
assert(not pcall(memprof.start, 0))

local duration = 0 -- infinite
local fname_stub = "ujit-memprof.bin"
local started, fname_real = memprof.start(duration, fname_stub)
assert(started == true, "Unable to start")
assert(
    string.sub(fname_real, 1, string.len(fname_stub)) == fname_stub,
    "Bad prefix of fname_real"
)

print(payload())

local stopped = memprof.stop()
assert(stopped == true, "Unable to stop")

stopped = memprof.stop()
assert(stopped == false, "Repetitive stop not possible")

assert(os.remove(fname_real), "fname_real can not be removed")

-- ...and restart profiler:

started, fname_real = memprof.start(duration, fname_stub)
assert(started == true, "Unable to start #2")

print(payload())

stopped = memprof.stop()
assert(stopped == true, "Unable to stop #2")

assert(os.remove(fname_real), "fname_real can not be removed #2")
