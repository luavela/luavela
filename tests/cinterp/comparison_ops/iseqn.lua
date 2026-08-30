-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISEQN (num, const num): n1 == n2
	local iseqn_equal_nums = function()
		local n = 42
		return (n == 42)
	end

	assert(cinterpcall(iseqn_equal_nums) == true)
end

do -- ISEQN (num, const num): n1 != n2
	local iseqn_not_equal_nums = function()
		local n = 42
		return (n == 43)
	end

	assert(cinterpcall(iseqn_not_equal_nums) == false)
end

do -- ISEQN (string, const num): always false
	local iseqn_string = function()
		local str = "42"
		return (str == 42)
	end

	assert(cinterpcall(iseqn_string) == false)
end
