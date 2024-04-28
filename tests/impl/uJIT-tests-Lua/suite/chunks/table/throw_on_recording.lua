-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local args = {...}

local func_name = args[1]

assert(ujit.table[func_name],
       string.format("No such function '%s' in 'ujit.table' extension library.", func_name))

local f = ujit.table[func_name]

local HOTLOOP = 1 -- recording start the _next_ iteration
jit.opt.start(3, "hotloop=" .. HOTLOOP)

local RECORDING_ITERATION = HOTLOOP + 1

local t = {}
t[HOTLOOP] = {}
t[RECORDING_ITERATION] = 1 -- invalid arg triggers an error

for i = 1, RECORDING_ITERATION do
	local _ = f(t[i])
end
