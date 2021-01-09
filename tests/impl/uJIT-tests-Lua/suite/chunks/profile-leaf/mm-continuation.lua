-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Ensure VM state is switched correctly with continuations
--
local aux = require('profile_aux')

local o = setmetatable({}, {__index = function (_, key)
    return #key > 10 and 'long' or 'short'
end})

local fname_real = aux.init_and_start_profiling(10)

-- Return from the __index metamethod is done via a continuation, which should
-- handle VM state switch correctly. If everything is done correctly,
-- profiler should definitely count something inside TRACE or LFUNC state.
print(o.aaaaaaaaaaaaaaa .. ' and ' .. o.aaaaaaaaaa)

local sum = 0
for i = 1, 1E8 do
    sum = sum + i
end

print(sum)

local counters = aux.stop_and_terminate_profiling(fname_real)

if jit.status() then
    assert(counters.TRACE > 0)
else
    assert(counters.LFUNC > 0)
end
