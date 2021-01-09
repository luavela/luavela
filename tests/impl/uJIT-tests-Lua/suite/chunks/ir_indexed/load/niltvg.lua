-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test checks that HREF must return reference to g->niltv.val (see niltv
-- at uj-record_indexed_load()).

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=10')

local t = {}

for i = 1, 30 do
	t[i] = t[i]
end
