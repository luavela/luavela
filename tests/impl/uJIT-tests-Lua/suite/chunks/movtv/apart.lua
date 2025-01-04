-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local N = 100
local src = {}
local dst = {}

for i = 1, N do
	src[i] = "src" .. i
	dst[i] = false
end

-- Now add various data types to the source:

assert(N > 20)
src[10] = 1
src[10 + 1] = 2
src[10 + 2] = 3

assert(N > 30)
src[20] = "str"
src[20 + 1] = "str2"
src[20 + 2] = "str3"

assert(N > 40)
src[30] = nil
src[30 + 1] = nil
src[30 + 2] = nil

assert(N > 50)
src[40] = true
src[40 + 1] = true
src[40 + 2] = true

assert(N > 60)
src[50] = "st5"
src[50 + 1] = "s6"
src[50 + 2] = "s7"

assert(N > 70)
src[60] = {}
src[60 + 1] = {}
src[60 + 2] = {}

assert(N > 80)
src[70] = function () end
src[70 + 1] = function () end
src[70 + 2] = function () end

jit.opt.start(4, "movtv", "movtvpri", "hotloop=2", "hotexit=2")
jit.on()

for i = 1, N do
	dst[i] = src[i]
end

jit.off()

for i = 1, N do
	assert(src[i] == dst[i])
end
