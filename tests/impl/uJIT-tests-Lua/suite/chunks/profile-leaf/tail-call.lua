-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Profiling a tail call
--
local aux = require('profile_aux')

local fname_real = aux.init_and_start_profiling(100)

local function foo()
    local sum1 = 1E3
    local sum2 = 0
    for i = 1, 1E7 do
        sum1 = sum1 + i - 5
        sum2 = sum2 + sum1
    end
    print(sum1 + sum2)
end

local function bar()
    local _ = 1
    return foo()
end

bar()

aux.stop_and_terminate_profiling(fname_real)
