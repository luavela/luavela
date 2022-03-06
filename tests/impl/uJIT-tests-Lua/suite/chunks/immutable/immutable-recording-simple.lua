-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("hotloop=2")
for _ = 1,5 do
    ujit.immutable(1)
    ujit.immutable("hello")
    ujit.immutable(jit.opt.start)
end
