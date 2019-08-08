-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local t = {}
for i = 1, 2 do
  table.insert(t, i)
end

for i = 1, 8 do
  t[i * 1e6] = i
end

jit.on()
-- Recording will happen in hash
jit.opt.start(4, "jitpairs", "hotloop=3", "hotexit=1")

local r = {}
for k, v in pairs(t) do
  r[k] = v
end

jit.off()
for k, v in pairs(r) do
  assert(t[k] == v)
end
