-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Ensure VM state is switched correctly with fast functions
--
local aux = require('aux')

assert(ujit.profile.init() == true)

-- Following call is wrapped into a fast function intentionally.
-- VM state switch should be handled correctly on return from fast functions.
-- If everything is done properly, profiler should definitely count something
-- inside TRACE or LFUNC state.
assert(ujit.profile.start(10, 'default') == true)

local sum = 0
for i = 1, 1E8 do
    sum = sum + i
end

print(sum)

local counters = aux.stop_and_terminate_profiling()
if jit.status() then
    assert(counters.TRACE > 0)
else
    assert(counters.LFUNC > 0)
end
