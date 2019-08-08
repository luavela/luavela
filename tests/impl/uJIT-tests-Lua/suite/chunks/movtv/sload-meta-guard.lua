-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "hotloop=1")
jit.on()

local function mov_arr(src)
	local dst = {}
	for i = 1, #src do
		dst[i] = src[i]
	end
	return dst
end

local src1 = ujit.immutable({"a", "b", "c", "d", "e", "f", "g", "h"})
local src2 = ujit.immutable(setmetatable({
	-- NB! The hole exploits some implementation-specific behaviour:
	6, 5, 4, 3, nil, 1,
}, ujit.immutable({__index = {
	6, 5, 4, 3, 2, 1,
}})))

jit.opt.start(4, "hotloop=2")
jit.on()

local dst1 = mov_arr(src1)
local dst2 = mov_arr(src2)

jit.off()

assert(#src1 == #dst1, "#src1 == #dst1")
for i = 1, #dst1 do
	assert(src1[i] == dst1[i], "#1; iteration " .. i)
end

assert(#src2 == #dst2, "#src2 == #dst2")
for i = 1, #dst2 do
	assert(src2[i] == dst2[i], "#2; iteration " .. i)
end
