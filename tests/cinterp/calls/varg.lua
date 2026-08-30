-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

---------------------------- HELPER FUNCTIONS -----------------------------

local foo_return_all_vargs = function(...)
	local a -- occupy some stack slots
	local b
	local c
	local d
	return ...
end

local foo_return_some_varargs = function(arg1, arg2, arg3, ...)
	local ret1, ret2 = ...
	return ret1, ret2
end

-------------------------------- TESTCASES --------------------------------

------ Cases when nres1 (RB) == 0 (i.e. all varargs must be copied) -------

-- no results
do
	cinterpcall(foo_return_all_vargs)
end

do
	cinterpcall(foo_return_all_vargs, 42)
end

do
	cinterpcall(foo_return_all_vargs, 42, 43)
end

do
	cinterpcall(foo_return_all_vargs, 42, 43, 44)
end

-- one result
do
	local ret = cinterpcall(foo_return_all_vargs)
	assert(ret == nil)
end

do
	local ret = cinterpcall(foo_return_all_vargs, 42)
	assert(ret == 42)
end

do
	local ret = cinterpcall(foo_return_all_vargs, 42, 43)
	assert(ret == 42)
end

do
	local ret = cinterpcall(foo_return_all_vargs, 42, 43, 44)
	assert(ret == 42)
end

-- two results
do
	local ret1, ret2 = cinterpcall( foo_return_all_vargs)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_return_all_vargs, 42)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_return_all_vargs, 42, 43)
	assert(ret1 == 42)
	assert(ret2 == 43)
end

do
	local ret1, ret2 = cinterpcall(foo_return_all_vargs, 42, 43, 44)
	assert(ret1 == 42)
	assert(ret2 == 43)
end

-- three results
do
	local ret1, ret2, ret3 = cinterpcall(foo_return_all_vargs)
	assert(ret1 == nil)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_return_all_vargs, 42)
	assert(ret1 == 42)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_return_all_vargs, 42, 43)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == nil)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_return_all_vargs, 42, 43, 44)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == 44)
end

-- stack reallocation
do
	local trigger_realloc = function(...)
		return {...}
	end

	local ret = cinterpcall(trigger_realloc,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
		31, 32, 33, 34, 35, 36, 37, 38, 39, 40)

	assert(#ret == 40)
	for i = 1, 40 do
		assert(ret[i] == i)
	end
end

-- Cases when nres1 (RB) != 0 (i.e. only specific varargs must be copied) --

do -- base_varg < base
	local ret1, ret2 = cinterpcall(foo_return_some_varargs, 42, 43, 44, 45, 46)
	assert(ret1 == 45)
	assert(ret2 == 46)
end

do -- base_varg >= base (just set missing varargs to nil)
	local ret1, ret2 = cinterpcall(foo_return_some_varargs, 42, 43, 44)
	assert(ret1 == nil)
	assert(ret2 == nil)
end
