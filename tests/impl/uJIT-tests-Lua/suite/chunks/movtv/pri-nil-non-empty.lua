-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {
	{y = "y"}, -- non-empty table that does not contain required key
	{y = "y"},
	{y = "y"},
	{y = "y"},
	{x = "x"},
	{x = "x"},
	{x = "x"},
	{x = "x"},
}

local dst = {}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1", "hotexit=2")
jit.on()

for i = 1, #src do
	dst[i] = {}
	dst[i].x = src[i].x
end

jit.off()

assert(#src == #dst)
for i = 1, #src do
	assert(dst[i].x == src[i].x)
end
