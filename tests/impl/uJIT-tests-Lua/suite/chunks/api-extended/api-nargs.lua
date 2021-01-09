-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test checks arguments handling for all Lua API functions.

jit.off()

assert(jit.status() == false)

-- Some random extra arguments
local ETAB = {}
local ENUM = 42
local ESTR = "1984"
local ENIL = nil

local function match(err, msg)
    local result = string.find(err, msg)

    if result == nil then
        print("[" .. err .. "] doesn't match with expected [" .. msg .. "]")
    end

    assert(result)
end

local function assert_call(argn, right_type, wrong_type, ...)
    local status, errmsg = pcall(...)
    assert(status == false)
    match(errmsg, "bad argument #" .. argn)
    match(errmsg, right_type .. " expected, got " .. wrong_type)
end

-- ujit.dump.bc(io_obj, func)
ujit.dump.bc(io.stdout, assert_call, ENIL)
assert_call(1, "userdata", "no value", ujit.dump.bc)
assert_call(1, "userdata", "nil", ujit.dump.bc, nil)
assert_call(2, "function", "no value", ujit.dump.bc, io.stdout)
assert_call(2, "function", "nil", ujit.dump.bc, io.stdout, nil)

-- ujit.dump.bcins(io_obj, func, pc[, nest_lvl])
local dumped = ujit.dump.bcins(io.stdout, assert_call, 1)
assert(dumped == true)
dumped = ujit.dump.bcins(io.stdout, assert_call, 1, 5)
assert(dumped == true)
dumped = ujit.dump.bcins(io.stdout, assert_call, 1, 5, ETAB, ESTR)
assert(dumped == true)
dumped = ujit.dump.bcins(io.stdout, assert_call, 1, ETAB)
assert(dumped == true)
assert_call(1, "userdata", "no value", ujit.dump.bcins)
assert_call(2, "function", "no value", ujit.dump.bcins, io.stdout)
assert_call(3, "number", "no value", ujit.dump.bcins, io.stdout, assert_call)
assert_call(1, "userdata", "number", ujit.dump.bcins, ENUM, ETAB, ESTR)
assert_call(2, "function", "number", ujit.dump.bcins, io.stdout, ENUM, ESTR)
assert_call(3, "number", "table", ujit.dump.bcins, io.stdout, assert_call, ETAB)

-- ujit.dump.mcode(io_obj, traceno)
jit.on()
jit.opt.start(0, "hotloop=1")

for _ = 1, 20 do
end

ujit.dump.mcode(io.stdout, 1)
ujit.dump.mcode(io.stdout, 1, ETAB, ESTR)
assert_call(1, "userdata", "nil", ujit.dump.mcode, nil, 1)
assert_call(2, "number", "no value", ujit.dump.mcode, io.stdout)
assert_call(2, "number", "nil", ujit.dump.mcode, io.stdout, nil)
jit.off()

-- ujit.dump.stack(io_obj)
ujit.dump.stack(io.stdout)
ujit.dump.stack(io.stdout, ENUM, ESTR)
assert_call(1, "userdata", "no value", ujit.dump.stack)
assert_call(1, "userdata", "nil", ujit.dump.stack, nil)

-- ujit.dump.start([fname_stub])
local _, fname_dump = ujit.dump.start("test", ETAB, ENIL)
ujit.dump.start()
assert_call(1, "string", "table", ujit.dump.start, ETAB)

local stopped = ujit.dump.stop()
ujit.dump.stop(ETAB, ESTR)
os.remove(fname_dump)
assert(stopped == true)

-- ujit.dump.trace(io_obj, traceno)
ujit.dump.trace(io.stdout, 1, ETAB, ENIL)
assert_call(1, "userdata", "no value", ujit.dump.trace)
assert_call(1, "userdata", "nil", ujit.dump.trace, nil)
assert_call(2, "number", "no value", ujit.dump.trace, io.stdout)
assert_call(2, "number", "nil", ujit.dump.trace, io.stdout, nil)

-- ujit.getmetrics()
ujit.getmetrics(ETAB, ESTR, ENUM)
ujit.getmetrics(ENIL)

-- ujit.immutable(val)
local imm_tab = {}
assert(ujit.immutable(imm_tab, ENUM, ENIL) == imm_tab)
assert(ujit.immutable(imm_tab, ENIL) == imm_tab)
ujit.immutable()

-- ujit.memprof.start(interval, fname_stub)
local started, fname_real = ujit.memprof.start(100, "memprof", ETAB, ESTR)
assert(started == true)
assert_call(1, "number", "no value", ujit.memprof.start)
assert_call(1, "number", "nil", ujit.memprof.start, nil)
assert_call(2, "string", "no value", ujit.memprof.start, 100)
assert_call(2, "string", "nil", ujit.memprof.start, 100, nil)

-- ujit.memprof.stop()
stopped = ujit.memprof.stop(ENUM, ETAB)
assert(stopped == true)
ujit.memprof.stop()
os.remove(fname_real)

-- ujit.profile.available()
ujit.profile.available()
ujit.profile.available(ETAB, ESTR)

-- ujit.profile.init()
ujit.profile.init()
ujit.profile.init(ETAB, ESTR)

-- ujit.profile.start(interval, mode[, fname_stub])
started, _ = ujit.profile.start(100, "default", "profile", ETAB, ESTR)
assert_call(1, "number", "no value", ujit.profile.start)
assert_call(1, "number", "nil", ujit.profile.start, nil)
assert_call(2, "string", "no value", ujit.profile.start, 100)
assert_call(2, "string", "nil", ujit.profile.start, 100, nil)
assert(started == true)

-- ujit.profile.stop()
ujit.profile.stop()
ujit.profile.stop(ETAB, ESTR, ENUM)

-- ujit.profile.terminate()
local terminated = ujit.profile.terminate()
ujit.profile.terminate(ENUM, ETAB)
assert(terminated == true)

-- ujit.seal(obj)
local seal_tab = {}
assert(ujit.seal(seal_tab, ETAB, ESTR) == seal_tab)

-- ujit.table.keys(tab)
local tab = {}
ujit.table.keys(tab, ESTR, ENUM)
assert_call(1, "table", "no value", ujit.table.keys)
assert_call(1, "table", "nil", ujit.table.keys, nil)

-- ujit.table.shallowcopy(table)
ujit.table.shallowcopy(tab, "", 123)
assert_call(1, "table", "no value", ujit.table.shallowcopy)
assert_call(1, "table", "nil", ujit.table.shallowcopy, nil)

-- ujit.table.values(table)
ujit.table.values(tab, ESTR, ENUM)
assert_call(1, "table", "no value", ujit.table.values)
assert_call(1, "table", "nil", ujit.table.values, nil)

-- ujit.usesfenv(func)
ujit.usesfenv(assert_call, ETAB, ESTR)
