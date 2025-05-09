-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {"x", "y", false, false, false}
local dst = {}
local n = 0

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")

for i = 1, #src do
    local flag = not src[i] -- BC_NOT
    if flag then
        n = n + 1
    end
    dst[i] = src[i]
end

jit.off()

assert(#src == #dst, "#src == #dst")
local m = 0
for i = 1, #src do
    assert(dst[i] == src[i], "iteration " .. i)
    if not src[i] then
        m = m + 1
    end
end
assert(n == m, "n == m")
