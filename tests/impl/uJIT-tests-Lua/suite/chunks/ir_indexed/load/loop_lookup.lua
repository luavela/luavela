-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Checks that JIT throws an error when there are more than 100 iterations
-- of metaobject lookup (see LJ_MAX_IDXCHAIN for more info).

local HOTLOOP = 200

jit.on()
assert(jit.status())
jit.opt.start('hotloop=' .. HOTLOOP)

local arr = {}
local MAX_LOOKUP = 100 -- Should match with LJ_MAX_IDXCHAIN.

for i = 1, MAX_LOOKUP + 1 do
	local curr = {}

	if (i ~= 1) then
		local prev = arr[i - 1]
		local mt = { __index = prev }
		setmetatable(curr, mt)
	end

	arr[i] = curr
end

for j = 1, HOTLOOP + 1 do
	if (j == HOTLOOP + 1) then
		-- Recording starts here.
		local tbl = arr[MAX_LOOKUP + 1]
		local _ = tbl[10] -- 10 is a random number.
	end
end
