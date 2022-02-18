-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function foo(bar)
    if bar > 5E6 then
        return 2 * bar - 50
    else
        return (bar % 2 == 1 and 1 or (-1)) * (bar + 10)
    end
end

local function baz()
    local sum = foo(0)
    for i = 1, 1E7 do
        sum = sum + foo(i - 5)
    end
    print(sum)
end

if ujit.profile.available() then
    -- bad argument calls to ujit.profile.start, generic
    assert(not pcall(ujit.profile.start))             -- too few args
    assert(not pcall(ujit.profile.start, 42, 43, 44)) -- too many args
    assert(not pcall(ujit.profile.start, '42'))       -- bad type
    assert(not pcall(ujit.profile.start, 0.9))        -- too small value
    assert(not pcall(ujit.profile.start, 2 ^ 52))     -- too large value
    assert(not pcall(ujit.profile.start, 10, ''))         -- bad profiling mode
    assert(not pcall(ujit.profile.start, 10, 'badmode'))  -- bad profiling mode

    -- testing bad argument calls to ujit.profile.start, leaf profile
    assert(not pcall(ujit.profile.start, 10, { }))    -- bad argument type

    local interval = 10 -- microseconds
    assert(ujit.profile.start(interval, 'default') == false) -- init was not called
    assert(ujit.profile.init()          ==  true)
    assert(ujit.profile.start(interval, 'default') ==  true)
    assert(ujit.profile.start(interval, 'default') == false)
end

baz()

if ujit.profile.available() then
    local counters = ujit.profile.stop()
    assert(type(counters) == 'table')

    assert(counters.IDLE   >= 0)
    assert(counters.INTERP >= 0)
    assert(counters.GC     >= 0)
    assert(counters.EXIT   >= 0)
    assert(counters.RECORD >= 0)
    assert(counters.OPT    >= 0)
    assert(counters.ASM    >= 0)
    assert(counters.TRACE  >= 0)

    assert(counters._SAMPLES      >  0)
    assert(counters._NUM_OVERRUNS >= 0)

    assert(type(counters._ID) == 'string' and counters._ID:len() > 0)

    local c, e = ujit.profile.stop()
    assert(c == nil)
    assert(type(e) == 'string' and e:len() > 0)
    assert(ujit.profile.terminate() == true)
end
