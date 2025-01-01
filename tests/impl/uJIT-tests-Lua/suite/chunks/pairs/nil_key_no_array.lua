-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.on()
jit.opt.start(4, "jitpairs", "hotloop=2")

local function copy(t)
  local res = {}
  for k, v in next, t, nil do -- 'pairs' call will abort trace
    res[k] = v
  end
  return res
end

local t = {[10] = 20, [20] = 30}
local r = {}
for _ = 1, 10 do
  r = copy(t)
end

jit.off()
for k, v in pairs(r) do
  assert(t[k] == v)
end
