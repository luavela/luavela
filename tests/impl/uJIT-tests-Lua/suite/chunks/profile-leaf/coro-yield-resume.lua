-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Profiling across several coroutine switches
--
local aux = require('profile_aux')

local co = coroutine.create(function (x, y)
    local sum1 = x
    local sum2 = 0
    for i = 1, y do
        sum1 = sum1 + i - 5
        sum2 = sum2 + sum1
        if i % 0.25E7 == 0 then
            coroutine.yield()
        end
    end
    print(sum1 + sum2)
    return sum1 + sum2
end)

local fname_real = aux.init_and_start_profiling(200)

coroutine.resume(co, 1E3, 1E7)

local total = 0
for i = 1, 3 do
    local sum = 0
    for j = 1, i * 1E6 do
        sum = sum + (-1) * i * j
    end
    total = total + sum
    coroutine.resume(co)
end

coroutine.resume(co)
print(total)

aux.stop_and_terminate_profiling(fname_real)
