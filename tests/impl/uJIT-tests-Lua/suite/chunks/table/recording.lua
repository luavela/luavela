-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local args = {...}

local func_name = args[1]

assert(ujit.table[func_name],
       string.format("No such function '%s' in 'ujit.table' extension library.", func_name))

local f = ujit.table[func_name]

local HOTLOOP = 1
jit.opt.start(3, "hotloop=" .. HOTLOOP)

local t = {
	{},
	{},
	{}
}

-- Trace is recorded at 'HOTLOOP + 1' and executed at 'HOTLOOP + 2'.
local RECORDING_ITERATION = HOTLOOP + 1

assert(#t >= RECORDING_ITERATION)

for i = 1, #t do
	c = f(t[i])
end
