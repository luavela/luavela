-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISEQP (pri, const pri): pri1 = pri2
	local iseqp_equal_pris = function()
		local pri1 = true
		return (pri1 == true)
	end

	assert(cinterpcall(iseqp_equal_pris) == true)
end

do -- ISEQP (pri, const pri): pri1 != pri2
	local iseqp_not_equal_pris = function()
		local pri1 = true
		return (pri1 == false)
	end

	assert(cinterpcall(iseqp_not_equal_pris) == false)
end

do -- ISEQP (number, const pri): always false
	local iseqp_num = function()
		local num = 42
		return (num == true)
	end

	assert(cinterpcall(iseqp_num) == false)
end
