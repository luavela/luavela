-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- there must be no errors.

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

-- First metatable has an another metatable (wich also set to
-- the __newindex field). uJIT will use mt1 as a regular table, not as metatable.

local n = 0
local tmp
local mt1 = { __newindex = function(_, _, _) tmp = "mt1"; end }
local mt2 = { __newindex = mt1 }

debug.setmetatable(mt2, mt1)
debug.setmetatable(n, mt2)

for j = 1, 3 do
	-- Recording starts here.
	if (j == 2) then
		n[1] = nil
	end
end

print(tmp)
