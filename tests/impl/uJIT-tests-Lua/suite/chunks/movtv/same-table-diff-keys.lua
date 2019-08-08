-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {{x=1}, {x=2}, {x=3}, {x=4}, {x=5}, {x=6}, {x=7}, {x=8}}

jit.opt.start(4, "hotloop=1")
jit.on()

for i = 1, #src do
	src[i].y = src[i].x
end

jit.off()

for i = 1, #src do
	assert(src[i].x == src[i].y, "Iteration " .. i)
end
