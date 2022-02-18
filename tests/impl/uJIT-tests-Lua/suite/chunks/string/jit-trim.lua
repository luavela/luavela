-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("jitcat", "hotloop=1")

for i=1,200 do
    assert(ujit.string.trim("   \n\t" .. tostring(i) .. " \t \t") == tostring(i))
end
