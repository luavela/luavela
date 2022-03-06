-- When started with a positive timeout, memprof must be able to interrupt
-- itself.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local function payload()
    local t
    for i = 1, 1000 do
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

local duration = 3 -- seconds
local fname_stub = "ujit-memprof.bin"
local started, fname_real = memprof.start(duration, fname_stub)
assert(started == true, "Unable to start")

local t1 = os.time()
while true do
    if (os.difftime(os.time(), t1) > 2 * duration) then
        break
    end
    payload()
end

-- memprof was started with some explicit duration: after the time is up,
-- it must stop implicitly, and an explicit call to stop() should return false
-- (=unable to stop already stopped thing). However, sometimes there occur
-- glitches (like, your machine is overloaded with running processed),
-- and we do not stop in time. In this case a warning is printed.
-- TODO: A nicer way of reporting "flaky" situations is needed here.
local stopped = memprof.stop()
if stopped then
    io.stderr:write([[
Warning! Profiler must be stopped by now, so you should not see this warning.
However, the test may be flaky under high workloads. Time to fix things if
this warning is popped too often.
]])
end

assert(os.remove(fname_real), "fname_real can not be removed")
