-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {"x", "y", false, false, false}
local dst = {}
local n = 0

jit.opt.start(4, "hotloop=1")

for i = 1, #src do
    if src[i] then -- BC_ISF
        n = n + 1
    end
    dst[i] = src[i]
end

jit.off()

assert(#src == #dst, "#src == #dst")
local m = 0
for i = 1, #src do
    assert(dst[i] == src[i], "iteration " .. i)
    if src[i] then
        m = m + 1
    end
end
assert(n == m, "n == m")
