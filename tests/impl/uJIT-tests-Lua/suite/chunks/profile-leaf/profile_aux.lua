-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NB! Please do not name this file aux.lua, as a weird bug has been filed:
-- https://github.com/luavela/luavela/issues/15

local aux = {}

--
-- Aux tester for leaf profiler. Contains common assertions for
-- initialization, start, stop and termination
--

function aux.init_profiling()
    assert(ujit.profile.init() == true, "Unable to init")
end

function aux.start_profiling(interval, fname_stub)
    interval   = interval   or error('Profiling interval missing!')
    fname_stub = fname_stub or 'profile.bin'

    local fname_stub_len = string.len(fname_stub)

    local started, fname_real = ujit.profile.start(interval, 'leaf', fname_stub)

    assert(started, "Unable to start")
    assert(
        string.sub(fname_real, 1, fname_stub_len) == fname_stub,
        "Bad prefix of fname_real"
    )
    assert(string.match(
        string.sub(fname_real, fname_stub_len + 1, -1),
        "^%.[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$"
    ), "Bad suffix of fname_real")

    return fname_real
end

function aux.init_and_start_profiling(interval, fname_stub)
    aux.init_profiling()
    return aux.start_profiling(interval, fname_stub)
end

function aux.stop_profiling(fname)
    local counters = ujit.profile.stop()

    assert(type(counters) == 'table', "Wrong type of counters")
    assert(counters._SAMPLES > 0, "Invalid number of _SAMPLES")

    if fname then
        assert(os.remove(fname), "fname can not be removed")
    end
    return counters
end

function aux.terminate_profiling()
    assert(ujit.profile.terminate() == true, "Unable to terminate")
end

function aux.stop_and_terminate_profiling(fname)
    local counters = aux.stop_profiling(fname)
    aux.terminate_profiling()
    return counters
end

return aux
