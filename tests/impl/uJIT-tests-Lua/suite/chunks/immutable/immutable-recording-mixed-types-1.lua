-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("hotloop=2")

local mutator = function (t) t.key = "value" end

local obj = {
    "",
    "",
    "", -- Recording
    "",
    "",
    {},
}
local N = #obj
for i = 1, N do
    ujit.immutable(obj[i])
end

assert(not pcall(mutator, obj[N]))
