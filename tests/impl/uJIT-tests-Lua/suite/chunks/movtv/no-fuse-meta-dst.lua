-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local rawset = rawset
local N = 100
local src = {}
local dst = setmetatable({}, {
	__newindex = function(t, k, _) rawset(t, k, "dst" .. k) end
})

for i = 1, N do
	src[i] = "src" .. i

	rawset(dst, i, false)
	if i >= N / 2 then
		dst[i] = nil
	end
end

ujit.immutable(src) -- just in case

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
		assert(dst[i] == "dst" .. i)
	end
end
