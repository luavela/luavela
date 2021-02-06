-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")
jit.on()

local setmetatable = setmetatable
local getmetatable = getmetatable

local mt1 = {__index = {}}
local mt2 = setmetatable({}, {__index = function (_, _) return "OOPS" end})
local src = {
	setmetatable({}, mt1),
	setmetatable({}, mt1),
	setmetatable({}, mt1),
	setmetatable({}, mt1),
	setmetatable({}, mt2),
	setmetatable({}, mt2),
}
local dst = {}

for i = 1, #src do
	-- The line below assembles to FLOAD which returns a table which
	-- should be extra instrumented with a "no-metatable" guard.
	local mt = getmetatable(src[i])
	dst[i] = {
		__index = mt.__index
	}
end

jit.off()

for i = 1, #src do
	local mt = getmetatable(src[i])
	assert(getmetatable(dst[i]) == nil)
	if mt == mt1 then
		assert(dst[i].__index == mt.__index)
	elseif mt == mt2 then
		assert(dst[i].__index == "OOPS")
	else
		assert(false)
	end
end
