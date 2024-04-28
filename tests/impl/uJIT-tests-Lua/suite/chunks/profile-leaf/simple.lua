-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Simple layout of Lua-Lua calls
--
local aux = require('profile_aux')

local fname_real = aux.init_and_start_profiling(200)

local function foo(bar)
    if bar > 5E6 then
        local z = 555
        for i = 10, 1, -1 do
            z = z * (i % 2 == 1 and 1 or -1)
        end
        return 2 * bar - 50 + z
    else
        return (bar % 2 == 1 and 1 or (-1)) * (bar + 10)
    end
end

local function kmn()
    local z = - 30
    for i = 1, 1E5 do
        z = z + foo(i + 10)
    end
    return z
end

local function baz()
    local sum1 = foo(0)
    local sum2 = kmn()
    for i = 1, 1E7 do
        sum1 = sum1 + foo(i - 5)
    end
    return sum1 + sum2
end

local v = baz()
print(v)

aux.stop_and_terminate_profiling(fname_real)
