-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- place for tests that don't fit anywhere
local cinterpcall = ujit.debug.cinterpcall

-- check that C function result will be returned correctly when
-- there is tail call with return to C frame (see restored_RA in FUNCC)
do
	local test_pack = function()
		return unpack({10, 20, 30})
	end

	local ret = {cinterpcall(test_pack)}
	assert(#ret == 3)
	assert(ret[1] == 10)
	assert(ret[2] == 20)
	assert(ret[3] == 30)
end

do -- same as above but restored_RA > 0
	local test_unpack = function()
		return unpack({10, 20, 30, 40, 50})
	end

	local ret1, ret2, ret3, ret4, ret5, ret6 = cinterpcall(test_unpack)
	assert(ret1 == 10)
	assert(ret2 == 20)
	assert(ret3 == 30)
	assert(ret4 == 40)
	assert(ret5 == 50)
	assert(ret6 == nil)
end

-- several checks that interpreter handles __call metamethod correctly
do
	local t = {}
	local mt = {__call = function() end}
	setmetatable(t, mt)

	assert(cinterpcall(t) == nil)
end

do
	local t = {}
	local mt = {__call = function(tab) return tab end}
	setmetatable(t, mt)

	assert(cinterpcall(t) == t)
end

do
	local t = {}
	local mt = {__call = function(tab, n) return tab, n end}
	setmetatable(t, mt)

	local tab, n = cinterpcall(t, 42)
	assert(tab == t)
	assert(n == 42)
end
