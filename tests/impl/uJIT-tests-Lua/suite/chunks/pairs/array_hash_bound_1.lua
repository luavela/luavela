-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local hotloop = 4
local t = {}
for i = 1, hotloop do
  table.insert(t, i)
end
for i = 1, 4 do
  t[i * 1e6] = i
end

jit.on()
-- Recording will happen on last array element
jit.opt.start(4, "jitpairs", "hotloop=" .. hotloop)

local r = {}
for k, v in pairs(t) do
  r[k] = v
end

jit.off()
for k, v in pairs(r) do
  assert(t[k] == v)
end
