-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

assert(jit.status() == true)
jit.opt.start('+jitcat')

local dump_start = ujit.dump.start
local dump_stop  = ujit.dump.stop

local function start_dumping()
        local started, fname = dump_start("/tmp/test_concat.txt")
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

local function test_concat_const_folding()
        local s = ''

        local fname = start_dumping()
        for _ = 1, 100 do
                -- The value below will be folded during recording and
                -- hoisted out of the loop
                s = "c" .. 0 .."nst str" .. 1 .. "ng"
        end
        local dump = stop_dumping(fname)

        assert(s == "c0nst str1ng")
        -- As we are folding a constant value, we do not expect any
        -- concatenation-related IRs to be emitted:
        assert(dump:find("str TOSTR ") == nil)
        assert(dump:find("p64 BUFPUT") == nil)
        assert(dump:find("str BUFSTR") == nil)
        -- ...But folding still works and should leave its trace in the dump:
        assert(dump:find("c0nst str1ng"))
end

local function test_concat_recording()
        local s = ''

        local fname = start_dumping()
        for i = 1, 100 do
                -- NB! Thanks to folding, we effectively reduce a large number
                -- of concatenations to a smaller one
                s = s .. i .. "const" .. "const" .. "20" .. 4 .. 2
        end
        local dump = stop_dumping(fname)

        assert(s == "1constconst20422constconst20423constconst20424constconst20425constconst20426constconst20427constconst20428constconst20429constconst204210constconst204211constconst204212constconst204213constconst204214constconst204215constconst204216constconst204217constconst204218constconst204219constconst204220constconst204221constconst204222constconst204223constconst204224constconst204225constconst204226constconst204227constconst204228constconst204229constconst204230constconst204231constconst204232constconst204233constconst204234constconst204235constconst204236constconst204237constconst204238constconst204239constconst204240constconst204241constconst204242constconst204243constconst204244constconst204245constconst204246constconst204247constconst204248constconst204249constconst204250constconst204251constconst204252constconst204253constconst204254constconst204255constconst204256constconst204257constconst204258constconst204259constconst204260constconst204261constconst204262constconst204263constconst204264constconst204265constconst204266constconst204267constconst204268constconst204269constconst204270constconst204271constconst204272constconst204273constconst204274constconst204275constconst204276constconst204277constconst204278constconst204279constconst204280constconst204281constconst204282constconst204283constconst204284constconst204285constconst204286constconst204287constconst204288constconst204289constconst204290constconst204291constconst204292constconst204293constconst204294constconst204295constconst204296constconst204297constconst204298constconst204299constconst2042100constconst2042")
        assert(dump:find("str TOSTR ")) -- numbers are coerced to strings
        assert(dump:find("p64 BUFPUT")) -- IR_BUFPUT is emitted
        assert(dump:find("str BUFSTR")) -- IR_BUFSTR is emitted
        assert(dump:find("constconst2042")) -- folding works
end

local function test_concat_uncoercible_data_type()
        local s = ''
        local t = setmetatable({}, {
                __concat = function(_, _) return '@' end
        })

        local fname = start_dumping()
        for i = 1, 100 do
                -- tables cannot be coerced to strings implicitly
                s = s .. i .. t
        end
        local dump = stop_dumping(fname)

        assert(dump:find("unexpected data type"))
end

test_concat_recording()
test_concat_const_folding()
test_concat_uncoercible_data_type()
