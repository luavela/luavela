-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")
jit.on()

local setmetatable = setmetatable
local rindex = ujit.table.rindex

local tab = setmetatable({}, {__index = function (_, _) return "OOPS" end})
local src = {
	{x = {y = {bar = "baz"}}},
	{x = {y = {bar = "baz"}}},
	{x = {y = {bar = "baz"}}},
	{x = {y = {bar = "baz"}}},
	{x = {y = tab}},
	{x = {y = tab}},
	{x = {y = tab}},
	{x = {y = tab}},
}
local dst = {}

for i = 1, #src do
	-- The line below results in a TVLOAD which returns a table which
	-- should be extra instrumented with a "no-metatable" guard.
	local t = rindex(src[i], "x", "y")
	dst[i] = {
		foo = t.bar
	}
end

jit.off()

local getmetatable = getmetatable

for i = 1, #src do
	local t = rindex(src[i], "x", "y")
	local mt = getmetatable(t)
	assert(getmetatable(dst[i]) == nil)
	if mt == nil then
		assert(dst[i].foo == t.bar)
	else
		assert(dst[i].foo == "OOPS")
		assert(rawget(t, "bar") == nil)
	end
end
