-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {
	{x = "str1"},
	{x = "str2"},
	{x = "str3"},
	{x = "str4"},
	{x = 123456},
	{x = {}},
}
local dst = {
	{x = false},
	{x = false},
	{x = false},
	{x = false},
	{x = false},
	{x = false},
}
assert(#src == #dst, "#src == #dst")

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1", "hotexit=2")
jit.on()

for i = 1, #src do
	dst[i].x = src[i].x
end

jit.off()

for i = 1, #src do
	assert(dst[i].x == src[i].x, "Iteration " .. i)
end
