-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local N = 100
local src = setmetatable({}, {__index = function(_, k) return "src" .. k end})
local dst = {}

for i = 1, N do
	if i < N / 2 then
		src[i] = "src" .. i
	else
		src[i] = nil
	end
	dst[i] = false
end

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1", "hotexit=2")
jit.on()

for i = 1, N do
	dst[i] = src[i]
end

jit.off()

for i = 1, N do
	if i < N / 2 then
		assert(src[i] == dst[i])
	else
		assert(rawget(src, i) == nil)
	end
	assert(dst[i] == "src" .. i)
end
