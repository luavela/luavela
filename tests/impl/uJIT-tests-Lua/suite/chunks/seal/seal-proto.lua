-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

assert(jit.status() == true)

local dump_start = ujit.dump.start
local dump_stop  = ujit.dump.stop

local function start_dumping()
        local started, fname = dump_start("/tmp/test_sealed_proto.txt")
        assert(started == true and fname)
        return fname
end

local function stop_dumping(fname)
        assert(dump_stop() == true)

        local file = assert(io.open(fname, "r"))
        local contents = file:read("*all")
        io.close(file)
        assert(os.remove(fname))

        return contents
end

local function test_no_recording_inside_sealed_proto()
        local foo = function (x, y)
                local s = 0
                for i = 1, 100 do -- recording does not trigger here
                        s = s + i
                end
                return x * y + s
        end
        ujit.seal(foo)

        local fname = start_dumping()
        local result = foo(6,7)
        local dump = stop_dumping(fname)

        assert(result == 5092)
        assert(dump:find("mcode") == nil)
end

local function test_no_inlining_of_sealed_proto()
        local s   = 0
        local foo = function (x, y)
                return x * y
        end
        ujit.seal(foo)

        local fname = start_dumping()
        for i = 1, 100 do
                s = s + foo(i, i + 1) -- recording will be aborted here
        end
        local dump = stop_dumping(fname)

        assert(s == 343400)
        assert(dump:find("mcode") == nil)
        assert(dump:find(" abort .+ blacklisted"))
end

local function test_enforcing_jit_doesnt_affect_sealed_proto()
        local foo = function (x, y)
                local s = 0
                for i = 1, 100 do -- recording does not trigger here
                        s = s + i
                end
                return x * y + s
        end
        ujit.seal(foo)

        jit.on(foo, true)

        local fname = start_dumping()
        local result = foo(6,7)
        local dump = stop_dumping(fname)

        assert(result == 5092)
        assert(dump:find("mcode") == nil)
end

test_no_recording_inside_sealed_proto()
test_no_inlining_of_sealed_proto()
test_enforcing_jit_doesnt_affect_sealed_proto()
