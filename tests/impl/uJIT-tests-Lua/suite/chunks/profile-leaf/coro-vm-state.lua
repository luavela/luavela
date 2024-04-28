-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Profiling coroutines: ensure VM state is switched correctly
--
local aux = require('profile_aux')

local co = coroutine.create(function (x, y)
    -- Almost no payload before yield:
    local sum = x
    coroutine.yield()
    -- Heavier calculations after yield:
    for i = 1, y do
        sum = sum + i
    end
    return sum
end)

local fname_real = aux.init_and_start_profiling(200)

coroutine.resume(co, 1E3, 1E8) -- invoke coroutine for the firts time

-- Now resume the coroutine. "Secondary" resume is partly implemented
-- via BC_RET* under the hood, so if VM state swtiching works,
-- profiler should definitely count something inside coroutine's payload
local status, result = coroutine.resume(co)
assert(status == true)
print(result)

local counters = aux.stop_and_terminate_profiling(fname_real)

if jit.status() then
    assert(counters.TRACE > 0)
else
    assert(counters.LFUNC > 0)
end
