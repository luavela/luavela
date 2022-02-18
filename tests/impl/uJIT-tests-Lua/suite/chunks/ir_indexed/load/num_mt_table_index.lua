-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- there must be no errors.

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

-- In first case t1 will be used for value lookup.
local function first_case()
	local n = 0
	local tmp

	local mt1 = { 134679, 445, 789 } -- Random numbers.
	local mt2 = { __index = mt1 }

	debug.setmetatable(mt2, mt1)
	debug.setmetatable(n, mt2)

	for i = 1, 3 do
		-- Recording starts here.
		if (i == 2) then
			tmp = n[1]
		end
	end
	assert(tmp == 134679)
end


-- Second case: when first metatable has another metatable (which also set to
-- the __index field). uJIT will use t1 as a regular table, not as metatable,
-- to find the value. Since there is no t1[1], nil will be returned.
local function second_case()
	local n = 0
	local tmp

	local mt1 = { __index = function(_, _) return 976431; end }
	local mt2 = { __index = mt1 }

	debug.setmetatable(mt2, mt1)
	debug.setmetatable(n, mt2)

	for j = 1, 3 do
		-- Recording starts here.
		if (j == 2) then
			tmp = n[1]
		end
	end
	assert(tmp == nil)
end

first_case()
second_case()
