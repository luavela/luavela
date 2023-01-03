-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- fails.

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

-- Trace recorder fails because there is no MM for number.

for i = 1, 3 do
	-- Recording starts here.
	if (i == 2) then
		-- t is a random number with no metatable by default (except strings).
		local n = 0
		-- JIT recorder will fail here.
		local _ = n[2]
	end
end
