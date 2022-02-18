-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test checks that HREF don't return reference to g->niltv.val. See
-- uj_record_indexed_load() for more info.

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=1')

local t = {1, 2, 3, 4, 5}

for i = 1, 5 do
	t[i] = t[i]
end
