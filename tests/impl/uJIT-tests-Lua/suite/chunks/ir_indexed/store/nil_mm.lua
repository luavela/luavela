-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Checking that JIT is placing IRT_NIL type to HREF IR.

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=1')

local t = {1, 2, 3}
local mt = {__newindex = function(_, _, _) return 42; end}

setmetatable(t, mt)

for i = 1, 4 do
	if i == 2 then
		t[100] = i
	end
end
