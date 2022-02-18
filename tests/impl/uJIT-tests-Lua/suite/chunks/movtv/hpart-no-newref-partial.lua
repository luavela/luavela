-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local src = {key1 = 1, key2 = 2, key3 = 3}
local dst = {}

jit.opt.start(4, "movtv", "movtvpri", "hotloop=1")
jit.on()

for _ = 1, 5 do
	-- copying via slot leads to some loads leaking snapshots,
	-- which prohibits MOVTV optimization. At the same time, please note
	-- that LOOP optimisation folds SLOADs to HLOADs which allows to fully
	-- apply MOVTV inside the looping part of the trace.
	local var1 = src.key1
	local var2 = src.key2
	local var3 = src.key3

	dst.key1 = var1
	dst.key2 = var2
	dst.key3 = var3
end

jit.off()

assert(ujit.table.size(src) == ujit.table.size(dst))

for k, _ in pairs(dst) do
    assert(src[k] == dst[k], "Key " .. k)
end
