-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local q = {1, 2, 3, 4, 5, 6, 7, 8}
local Q = {8, 7, 6, 5, 4, 3, 2, 1}
local i = 1
local j = #q

jit.opt.start(4, "hotloop=1")
jit.on()

repeat
	q[i], q[j] = q[j], q[i]
	i = i + 1
	j = j - 1
until i >= j

jit.off()

assert(#q == #Q, "#q == #Q")
for n = 1, #q do
	assert(q[n] == Q[n], "Iteration " .. n)
end
