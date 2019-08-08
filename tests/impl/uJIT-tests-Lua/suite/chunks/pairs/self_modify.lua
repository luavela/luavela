-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local t = {}
for i = 1, 8 do
  table.insert(t, i)
end

for i = 1, 8 do
  t[tostring(i)] = i
end

jit.on()
jit.opt.start(4, "jitpairs", "hotloop=1", "hotexit=1")

local sum = 0
for k, v in pairs(t) do
  sum = sum + v
  if type(k) == "number" then
    t[tostring(k)] = v * 2
  end
end
assert(sum == 108)

sum = 0
for k, v in pairs(t) do
  sum = sum + v
  t[tostring(k)] = nil
end
assert(sum == 36)
