-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local aux = require('aux')

local function foo()
    local sum = 0
    for i = 1, 1E7 do
        sum = sum + i * i + math.sin(i)
    end
    return sum
end

local function run_profiled()
    local fname_real = aux.start_profiling(200)

    local v = foo()
    print(v)

    aux.stop_profiling(fname_real)
end

-- Now run several times to make sure that restarting works:
aux.init_profiling()
run_profiled()
run_profiled()
run_profiled()
aux.terminate_profiling()
