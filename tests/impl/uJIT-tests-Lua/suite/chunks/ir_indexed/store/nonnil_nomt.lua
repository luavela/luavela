-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test checks IR's when old value from table (without metatable) wasn't nil.

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=1')

local _ = {1, 2}

for i = 1, 3 do
	if i == 2 then
		_[1] = i
	end
end
