-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test checks that JIT doesn't emit NEWREF for existing key.

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=2')

local _ = {}

for i = 1, 50 do
	if i >= 3 then
		_[i] = "val"
	end
end
