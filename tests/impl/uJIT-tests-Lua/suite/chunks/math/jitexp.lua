-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- disable fold optimization, start recording after 3rd iteration
jit.opt.start(0, "hotloop=3")

local function magic_number()
  return 3.141592654
end

local function exp_func()
  return math.exp(magic_number())
end

local vm = exp_func()
for _ = 1,5 do
  local jit = exp_func()
  assert(jit == vm)
end
