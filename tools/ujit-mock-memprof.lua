jit.off()

local argv = {...}

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

assert(type(argv[1]) == "string", "Output fname required on the command line")

local duration = 0 -- infinite
local fname_stub = argv[1]
local started, fname_real = memprof.start(duration, fname_stub)
assert(started == true, "Unable to start")

payload()

local stopped = memprof.stop()
assert(stopped == true, "Unable to stop")

print(fname_real)
