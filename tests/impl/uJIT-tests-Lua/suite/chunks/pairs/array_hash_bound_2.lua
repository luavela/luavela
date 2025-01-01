-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.on()
jit.opt.start(4, "jitpairs", "hotloop=1", "hotexit=1")

local function foo(t)
	local s = 0
	for k, _ in next, t, nil do
		s = s + t[k]
	end
	return s
end

assert(foo({1, 2, 3, 4, 5, 6}) == 21)
assert(foo({1, 2, 3, 4, 5, 6, x = 7, y = 8}) == 36)
