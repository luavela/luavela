-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Checks that JIT emits right IR's for converting value and when new entry
-- must be added to the table.

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=1')

local _ = {1, 2, 3}

for i = 1, 4 do
	if i == 2 then
		_[100] = i
		_.value = i
	end
end
