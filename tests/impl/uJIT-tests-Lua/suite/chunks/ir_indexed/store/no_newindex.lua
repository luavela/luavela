-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- fails.

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

-- Trace recorder fails because there is no MM for number.

for i = 1, 3 do
	-- Recording starts here.
	if (i == 2) then
		local _
		-- JIT recorder will fail here since '_' doesn't has a metatable.
		_[0] = "mystring"
	end
end
