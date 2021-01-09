-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- NB! These tests assert trace abort scenarios,
-- so loop-invariant bodies are totally ok.

jit.on()
jit.opt.start(4, "hotloop=1")

local rindex = ujit.table.rindex

do -- unrecordable: non-nil and non-table as a first argument
	local res
	for _ = 1, 3 do
		res = rindex("")
	end
	assert(res == "")
end

do -- unrecordable: table without arguments
	local res
	for _ = 1, 3 do
		res = rindex({})
	end
	assert(res == nil)
end

do -- unrecordable: too many arguments (implementation-dependent limit)
	-- May be worth rewriting if unpack is compiled one day.
	local t = {x={x={x={x={x={x={x={x={x={x={x={x={x={x={x=0}}}}}}}}}}}}}}}
	local res
	for _ = 1, 3 do
		res = rindex(t, "x", "x", "x", "x", "x", "x", "x",
			     "x", "x", "x", "x", "x", "x", "x", "x")
	end
	assert(res == 0)
end

do -- unrecordable: metatable is set, __index metamethod is not triggered
	local res
	local t = setmetatable({key = "value"}, {
		__index = function (_, _) return "value" end
	})
	for _ = 1, 3 do
		res = rindex(t, "key")
	end
	assert(res == "value")
end

do -- unrecordable: metatable is set, __index metamethod is triggered
	local res
	local t = setmetatable({}, {
		__index = function (_, _) return "value" end
	})
	for _ = 1, 3 do
		res = rindex(t, "key")
	end
	assert(res == "value")
end

do -- unrecordable: metatable is set, no __index metamethod
	local res
	local t = setmetatable({key = "value"}, {
		__newindex = function (_, _, _) assert(false) end
	})
	for _ = 1, 3 do
		res = rindex(t, "key")
	end
	assert(res == "value")
end

do -- unrecordable: NYI variant of BC_VARG
	local res
	for _ = 1, 3 do
		res = rindex({}, ...)
	end
	assert(res == nil)
end
