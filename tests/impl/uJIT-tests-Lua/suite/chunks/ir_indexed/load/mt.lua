-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test assumes that JIT is on, starts recording on 2-th iteration and
-- there must be no errors.

jit.on()
assert(jit.status())
jit.opt.start('hotloop=1')

local res = {}

-- W/O userdata
local n = 0
local str = "123"
local null = nil
local bool = true
local func = function () end
local tbl = {}
local co = coroutine.create(function () end)

local mt = { __index = function(_, _) return "__index"; end }
local counter = 0

debug.setmetatable(n, mt)
debug.setmetatable(str, mt)
debug.setmetatable(null, mt)
debug.setmetatable(bool, mt)
debug.setmetatable(func, mt)
debug.setmetatable(tbl, mt)
debug.setmetatable(co, mt)

for i = 1, 3 do
	if (i == 2) then
		res.num = n[1]
		res.str = str[1]
		res.null = null[1]
		res.bool = bool[1]
		res.func = func[1]
		res.tbl = tbl[1]
		res.co = co[1]
	end
end

jit.off()

for _, v in pairs(res) do
	assert(v == "__index")
	counter = counter + 1
end

assert(counter == 7)
