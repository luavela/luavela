-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- ISNEP (pri, const pri): pri1 = pri2
	local isnep_equal_pris = function()
		local pri1 = true
		return (pri1 ~= true)
	end

	assert(cinterpcall(isnep_equal_pris) == false)
end

do -- ISNEP (pri, const pri): pri1 != pri2
	local isnep_not_equal_pris = function()
		local pri1 = true
		return (pri1 ~= false)
	end

	assert(cinterpcall(isnep_not_equal_pris) == true)
end

do -- ISNEP (number, const pri): always true
	local isnep_num = function()
		local num = 42
		return (num ~= true)
	end

	assert(cinterpcall(isnep_num) == true)
end
