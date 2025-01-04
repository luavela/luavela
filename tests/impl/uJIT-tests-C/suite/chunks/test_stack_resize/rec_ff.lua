 -- This is a part of uJIT's testing suite.
 -- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 -- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("hotloop=1") -- second call to payload triggers its recording

local ncalls = 0

function payload()
	-- Number of local variables is intentionally crafted to trigger stack
	-- resize during a call to table.insert.
	local var1 = "str1"
	local var2 = "str2"
	local var3 = "str3"
	local var4 = "str4"
	local var5 = "str5"
	local var6 = "str6"
	local var7 = "str7"
	local var8 = "str8"
	local var9 = "str9"
	local var10 = "str10"
	local var11 = "str11"
	local var12 = "str12"
	local var13 = "str13"
	local var14 = "str14"
	local var15 = "str15"
	local var16 = "str16"
	local var17 = "str17"
	local var18 = "str18"
	local var19 = "str19"
	local var20 = "str20"

	ncalls = ncalls + 1

	if ncalls == 2 then
		-- This is to ensure that following code executes when
		-- the compiler is active:
		local t = {}
		table.insert(t, "wow")
	end
end
