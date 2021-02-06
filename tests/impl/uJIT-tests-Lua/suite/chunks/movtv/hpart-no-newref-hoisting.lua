-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local setmetatable = setmetatable
local modules = {{}, {}, {}, {}, {}}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")
jit.on()

local function seeall(mod)
	setmetatable(mod, {__index = _G})
end

for i = 1, #modules do
	seeall(modules[i])
end

jit.off()

for i = 1, #modules do
	assert(modules[i].table == _G.table)
end
