-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {{x=1}, {x=2}, {x=3}, {x=4}, {x=5}, {x=6}, {x=7}, {x=8}}
local ref = {{x=1}, {x=2}, {x=3}, {x=4}, {x=5}, {x=6}, {x=7}, {x=8}}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")
jit.on()

for i = 1, #src do
	src[i].x = src[i].x
end

jit.off()

for i = 1, #src do
	assert(src[i].x == ref[i].x, "Iteration " .. i)
end
