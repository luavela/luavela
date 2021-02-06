-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {
	{x = true},
	{x = true},
	{x = true},
	{x = true},
	{x = false},
	{x = false},
	{x = false},
	{x = false},
	{},
	{},
	{},
	{},
}

local dst = {}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1", "hotexit=3")
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
