-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {"a", "b", "c", "d", "e", "f", false, "h"}
local dst = {}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")

for i = 1, #src do
    -- We do not record assert explicitly relying on the guarded load.
    -- So although it looks like that the load is used only by the store,
    -- a recording-time fix-up must hint the compiler that the load
    -- was "kinda implicitly" used somewhere else.
    assert(src[i], "OOPS")
    dst[i] = src[i]
end

print(#dst) -- unreachable
