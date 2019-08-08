-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- there must be no errors.

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

local res = {}
local counter = 0

-- W/O userdata
local n = 0
local str = "123"
local null = nil
local bool = true
local func = function () end
local tbl = {}
local co = coroutine.create(function () end)

local mt = { __newindex = function(_, _, _)
		res[counter] = "__newindex"
		counter = counter + 1
	     end
}

debug.setmetatable(n, mt)
debug.setmetatable(str, mt)
debug.setmetatable(null, mt)
debug.setmetatable(bool, mt)
debug.setmetatable(func, mt)
debug.setmetatable(tbl, mt)
debug.setmetatable(co, mt)

for i = 1, 3 do
	if (i == 2) then
		n[1] = 1
		str[2] = 2
		null[3] = 3
		bool[4] = 4
		func[5] = 5
		tbl[6] = 6
		co[7] = 7
	end
end

jit.off()

for _, v in pairs(res) do
	if (v ~= "__newindex") then
		os.exit(1)
	end
end
