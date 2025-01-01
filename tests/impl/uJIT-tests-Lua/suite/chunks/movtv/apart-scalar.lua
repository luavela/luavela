-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local nmov = 0
local function mov(dst, src, idx)
	nmov = nmov + 1
	dst[idx] = src[idx]
end

local src = {"val1", 2, "val3", 4, "val5", 6}
local dst = {}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1", "hotexit=3")
jit.on()

mov(dst, src, 1)
mov(dst, src, 2)
mov(dst, src, 3)
mov(dst, src, 4)
mov(dst, src, 5)
mov(dst, src, 6)

jit.off()

assert(#dst == #src)
assert(#dst == nmov)

for i = 1, nmov do
	assert(src[i] == dst[i])
end
