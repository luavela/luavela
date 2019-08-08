-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- Profiling code with a tail call to a fast function
-- Unfortunately, this test is not 100% determenistic,
-- but parameters are picked to guarantee ~100% crashes
-- in case of an error.
--
local aux = require('aux')

local fname_real = aux.init_and_start_profiling(10)

local function baz1(wee)
    return math.sin(wee)
end

local function baz2(wee)
    return math.cos(wee)
end

local function baz3(wee)
    return math.abs(wee)
end

local function foo(bar)
    local sum = 0
    for i = 1, bar do
        sum = sum + (baz1(i + bar) + baz2(i + bar)) / baz3(i + bar)
    end
    print(sum)
end

foo(1E7)

aux.stop_and_terminate_profiling(fname_real)
