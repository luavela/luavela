-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- fails with "missing metamethod" message (grep NOMM for more info).

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

local n = 0
local mt = { __newindex = nil }

debug.setmetatable(n, mt)

for i = 1, 3 do
	-- Recording starts here.
	if (i == 2) then
		n[1] = "mystring"
	end
end
