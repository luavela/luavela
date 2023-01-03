-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local src = {"a", "b", "c", "d", "e", "f", "g", "h"}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")

-- If you want to change the name of the variable, tune .luacheckrc as well :-)
FOO = nil
for i = 1, #src do
	FOO = src[i]
end

jit.off()

assert(FOO == src[#src])
