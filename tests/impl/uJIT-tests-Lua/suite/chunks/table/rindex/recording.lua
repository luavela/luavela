-- Tests for recording of ujit.table.rindex
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.on()
jit.opt.start(4, "hotloop=1")

local rindex = ujit.table.rindex

do --- basic scenario: numeric keys and string keys
	local t = {
		{y = "z1"},
		{y = "z2"},
		{y = "z3"},
	}

	local res
	for i = 1, #t do
		res = rindex(t, i, "y")
	end
	assert(res == "z3")
end

do --- basic scenario: all string keys
	local t = {
		x1 = {y = "z1"},
		x2 = {y = "z2"},
		x3 = {y = "z3"},
	}

	local indices = {"x1", "x2", "x3"}

	local res
	for i = 1, #indices do
		res = rindex(t, indices[i], "y")
	end
	assert(res == "z3")
end

do --- repetition of a key must not be eliminated
	local t = {
		x1 = {x1 = {y = "z1"}},
		x2 = {x2 = {y = "z2"}},
		x3 = {x3 = {y = "z3"}},
	}

	local indices = {"x1", "x2", "x3"}

	local res
	for i = 1, #indices do
		res = rindex(t, indices[i], indices[i], "y")
	end
	assert(res == "z3")
end

do --- table as keys
	local t = {
		x1 = {x1 = {y = "z1"}},
		x2 = {x2 = {y = "z2"}},
		x3 = {x3 = {y = "z3"}},
	}
	t.x1[t] = t.x1.x1
	t.x2[t] = t.x2.x2
	t.x3[t] = t.x3.x3

	local indices = {"x1", "x2", "x3"}

	local res
	for i = 1, #indices do
		res = rindex(t, indices[i], t, "y")
	end
	assert(res == "z3")
end

do --- cdata keys
	local indices = {0x200000000ULL, 0x200000001ULL, 0x200000002ULL}
	local t = {
		{[indices[1]] = "z1"},
		{[indices[2]] = "z2"},
		{[indices[3]] = "z3"},
	}

	local res
	for i = 1, #t do
		res = rindex(t, i, indices[i])
	end
	assert(res == "z3")
end

do --- cdata keys
	local ffi = require("ffi")
	local ofs = 0x200000000ULL
	local data = {
		ffi.new("uint64_t", 1),
		ffi.new("uint64_t", 2),
		ffi.new("uint64_t", 3),
	}
	local t = {{}, {}, {}}

	local res
	for i = 1, #t do
		local v = data[i] + ofs
		t[i][v] = "z" .. i
		res = rindex(t, i, v)
	end
	assert(res == "z3")
end

do --- failure to take the fast path on trace
	local t = {
		x1 = {y = "z1"},
		x2 = {y = "z2"},
		x3 = setmetatable({}, {__index = function(_, _) return "default" end}),
	}

	local indices = {"x1", "x2", "x3"}

	local res
	for i = 1, #indices do
		res = rindex(t, indices[i], "y")
	end
	assert(res == "default")
end

do --- "path" returned as a list from an inlined function
	local indices = {"x1", "x2", "x3"}
	local function foo(...)
		local idx = select(1, ...)
		return indices[idx], "y", "N"
	end

	local t = {
		x1 = {y = {N = "z1"}},
		x2 = {y = {N = "z2"}},
		x3 = {y = {N = "z3"}},
	}

	local res
	for i = 1, #indices do
		res = rindex(t, foo(i))
	end
	assert(res == "z3")
end
