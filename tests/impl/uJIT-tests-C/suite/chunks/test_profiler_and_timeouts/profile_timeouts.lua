-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local PROFILING_INTERVAL = 10 -- usec
local profile_bin

-- Dummy constants to be used as upvalues in coroutine_payload
local N     = 10000
local N_str = "10000"
local function coroutine_payload(n)
        local i = 0
        local s = 0
        while true do -- never returns
                i = i + n
                s = s + i + (i % 20 == 0 and -N or 2 * tonumber(N_str))
        end
end

local function caller2(n)
        local mt = {
                __add = function (t1, t2)
                        return coroutine_payload(n) + t1.x + t2.x
                end,
        }
        local t1 = setmetatable({x = 10}, mt)
        local t2 = setmetatable({x = 20}, mt)
        return t1 + t2
end

local function caller1(n)
        local rv = caller2(n)
        return rv * 2
end

function coroutine_start(n)
        return caller1(n)
end

function chunk_start()
        local profiling_started

        jit.off()

        assert(ujit.profile.init() == true)

        profiling_started, profile_bin = ujit.profile.start(
                PROFILING_INTERVAL, 'callgraph', 'profile_timeouts.bin')

        assert(profiling_started)
end

function chunk_exit()
        local counters = ujit.profile.stop()

        assert(type(counters) == 'table')
        assert(counters._SAMPLES > 0)
        assert(counters.INTERP   > 0)
        assert(counters.LFUNC    > 0)

        assert(ujit.profile.terminate() == true)

        if profile_bin then
                assert(os.remove(profile_bin))
        end
end
