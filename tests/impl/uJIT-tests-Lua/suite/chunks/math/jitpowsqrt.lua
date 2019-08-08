-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- disable fold optimization, start recording after 3rd iteration
jit.opt.start(0, "hotloop=3")

local function magic_number()
  return 3.141592654
end

local function sqrt_func()
  return magic_number() ^ 0.5
end

local vm = sqrt_func()
for _ = 1,5 do
  local jit = sqrt_func()
  assert(jit == vm)
end
