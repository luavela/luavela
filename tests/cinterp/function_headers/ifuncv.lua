-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

---------------------------- HELPER FUNCTIONS -----------------------------

-- when function has '...' argument, first bytecode
-- always will be IFUNCV even when varargs aren't used

local foo_only_vararg = function(...)
	local a = 1 -- several local vars to change framesize
	local b = 2
	local c = 3
	local d = 4
	local e = 5
end

local foo_only_vararg_with_return = function(...)
	local a = 1 -- several local vars to change framesize
	local b = 2
	local c = 3
	local d = 4
	local e = 5
	return a, b, c, d, e
end

local foo_one_arg = function(arg1, ...)
	local a = 1
	local b = 2
	local c = 3
	local d = 4
	local e = 5
	return arg1
end

local foo_two_args = function(arg1, arg2, ...)
	local a = 1
	local b = 2
	local c = 3
	local d = 4
	local e = 5
	return arg1, arg2
end

local foo_three_args = function(arg1, arg2, arg3, ...)
	local a = 1
	local b = 2
	local c = 3
	local d = 4
	local e = 5
	return arg1, arg2, arg3
end

-------------------------------- TESTCASES --------------------------------

do -- testcase is similar to ifuncf.lua except that first bytecode of foo will be IFUNCV
	local foo = function(arg1, arg2, arg3, arg4, ...)
		local tmp1; local tmp2; local tmp3; local tmp4; local tmp5
		local tmp6; local tmp7; local tmp8; local tmp9; local tmp10
		local tmp11; local tmp12; local tmp13; local tmp14; local tmp15
		local tmp16; local tmp17; local tmp18; local tmp19; local tmp20
		local tmp21; local tmp22; local tmp23; local tmp24; local tmp25

		return arg1, arg2, arg3, arg4
	end

	local ret1, ret2, ret3, ret4 = cinterpcall(foo, 'test1', 'test2')

	assert(ret1 == 'test1')
	assert(ret2 == 'test2')
	assert(not ret3)
	assert(not ret4)
end

do
	local foo = function(arg1, arg2, arg3, arg4, ...)
		local tmp1; local tmp2; local tmp3; local tmp4; local tmp5
		local tmp6; local tmp7; local tmp8; local tmp9; local tmp10
		local tmp11; local tmp12; local tmp13; local tmp14; local tmp15
		local tmp16; local tmp17; local tmp18; local tmp19; local tmp20
		local tmp21; local tmp22; local tmp23; local tmp24; local tmp25

		return arg1, arg2, arg3, arg4
	end

	local ret1, ret2, ret3, ret4 = cinterpcall(foo, 'test1', 'test2', 42, 43)

	assert(ret1 == 'test1')
	assert(ret2 == 'test2')
	assert(ret3 == 42)
	assert(ret4 == 43)
end

-- numparams == 0
do
	local ret = cinterpcall(foo_only_vararg)
	assert(ret == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_only_vararg)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret = cinterpcall(foo_only_vararg, 42)
	assert(ret == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_only_vararg, 42)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret = cinterpcall(foo_only_vararg, 42, 43)
	assert(ret == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_only_vararg, 42, 43)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_only_vararg, 42, 43, "extra_arg")
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret1, ret2, ret3, ret4, ret5 =
		cinterpcall(foo_only_vararg_with_return, 42, 43, "extra_arg")
	assert(ret1 == 1)
	assert(ret2 == 2)
	assert(ret3 == 3)
	assert(ret4 == 4)
	assert(ret5 == 5)
end

-- numparams == 1
do
	local ret = cinterpcall(foo_one_arg)
	assert(ret == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_one_arg)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret = cinterpcall(foo_one_arg, 42)
	assert(ret == 42)
end

do
	local ret1, ret2 = cinterpcall(foo_one_arg, 42)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local ret = cinterpcall(foo_one_arg, 42, 43)
	assert(ret == 42)
end

do
	local ret1, ret2 = cinterpcall(foo_one_arg, 42, 43)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_one_arg, 42, 43, "extra_arg")
	assert(ret1 == 42)
	assert(ret2 == nil)
end

-- numparams == 2
do
	local ret1, ret2 = cinterpcall(foo_two_args)
	assert(ret1 == nil)
	assert(ret2 == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_two_args, 42)
	assert(ret1 == 42)
	assert(ret2 == nil)
end

do
	local ret1, ret2 = cinterpcall(foo_two_args, 42, 43)
	assert(ret1 == 42)
	assert(ret2 == 43)
end

do
	local ret1, ret2 = cinterpcall(foo_two_args, 42, 43, "extra_arg")
	assert(ret1 == 42)
	assert(ret2 == 43)
end

-- numparams == 3
do
	local ret1, ret2, ret3 = cinterpcall(foo_three_args)
	assert(ret1 == nil)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_three_args, 42)
	assert(ret1 == 42)
	assert(ret2 == nil)
	assert(ret3 == nil)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_three_args, 42, 43)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == nil)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_three_args, 42, 43, 44)
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == 44)
end

do
	local ret1, ret2, ret3 = cinterpcall(foo_three_args, 42, 43, 44, "extra_arg")
	assert(ret1 == 42)
	assert(ret2 == 43)
	assert(ret3 == 44)
end
