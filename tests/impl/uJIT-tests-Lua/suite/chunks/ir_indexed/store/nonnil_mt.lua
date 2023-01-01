-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test checks IR's when old value from table (with metatable) wasn't nil.

jit.on()
assert(jit.status())
jit.opt.start(0, 'hotloop=1')

local t = {1, 2}
local mt = {__newindex = function(_, _, _) return 42; end}
setmetatable(t, mt)

for i = 1, 3 do
	if i == 2 then
		t[1] = i
	end
end
