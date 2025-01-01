-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- disable fold optimization, start recording after 3rd iteration
jit.opt.start(0, "hotloop=3")

local function magic_number()
  return 3.141592654
end

local function exp2_func()
  return 2 ^ magic_number()
end

local vm = exp2_func()
for _ = 1,5 do
  local jit = exp2_func()
  assert(jit == vm)
end

