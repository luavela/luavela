-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local tab = {}
for i = 1, 8 do
  table.insert(tab, i)
end

for i = 1, 8 do
  tab[tostring(i)] = i
end

local function modify(t, modifier)
  local sum = 0
  for k, v in next, t, nil do
    sum = sum + v
    if type(k) == "number" then
      t[tostring(k)] = modifier
    end
  end
  return sum
end

jit.on()
jit.opt.start(4, "jitpairs", "hotloop=1")

for i = 1, 10 do
  assert(modify(tab, i) == 36 + i * 8)
end
