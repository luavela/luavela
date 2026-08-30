-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do
	-- large framesize will trigger stack reallocation
	local foo = function(arg1, arg2, arg3, arg4)
		-- occupy a lot of stack slots to trigger uj_state_stack_grow
		local tmp1; local tmp2; local tmp3; local tmp4; local tmp5
		local tmp6; local tmp7; local tmp8; local tmp9; local tmp10
		local tmp11; local tmp12; local tmp13; local tmp14; local tmp15
		local tmp16; local tmp17; local tmp18; local tmp19; local tmp20
		local tmp21; local tmp22; local tmp23; local tmp24; local tmp25

		-- arg3 and arg4 must be properly set to nil when stack reallocated
		return arg1, arg2, arg3, arg4
	end

	local ret1, ret2, ret3, ret4 = cinterpcall(foo, 'test1', 'test2')

	assert(ret1 == 'test1')
	assert(ret2 == 'test2')
	assert(not ret3)
	assert(not ret4)
end

do
	local foo = function(arg1, arg2, arg3, arg4)
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
