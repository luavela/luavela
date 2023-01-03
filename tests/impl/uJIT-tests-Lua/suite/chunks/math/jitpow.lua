-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- disable fold optimization, start recording after 3rd iteration
jit.opt.start(0, "hotloop=3")

local function magic_number()
  return 3.141592654
end

local function magic_exp()
  return 4.5
end

local function pow_func()
  return magic_number() ^ magic_exp()
end

local function math_pow_func()
  return math.pow(magic_number(), magic_exp())
end

local vm = pow_func()
for _ = 1,5 do
  local jit = pow_func()
  local math_jit = math_pow_func()
  assert(jit == math_jit)
  assert(jit == vm)
end
