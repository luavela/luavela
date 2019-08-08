-- Tests on POW folding optimization.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("fold", "hotloop=3")

local function assert_almost_equal(a, b, delta)
  assert(math.abs(a - b) < delta)
end

-- We want folding enabled to test it, but we don't want all constants
-- be folded by platform, instead we need some way to get deterministic
-- constant during compiled code execution, i.e. a function that would be
-- compiled, but would not be folded (hence math functions are not suitable).
-- Here is one way to do this.
local ffi = require("ffi")
ffi.errno(4)

local function magic_number()
  return ffi.errno() * 0.78539
end

local function magic_exp()
  return ffi.errno() * 1.23
end

local function magic_iexp()
  return ffi.errno()
end

-- should be recorded as 1.0
local function one_func()
  return 1.0 ^ magic_exp()
end

-- should be recorded as 1.0
local function zero_exp_func()
  return magic_number() ^ 0
end

-- should be recorded as 'arg'
local function identity_func(arg)
  return arg ^ 1
end

-- should be recorded as ldexp(1.0, magic_iexp())
local function iexp2_func()
  return 2 ^ magic_iexp()
end

-- should be recorded as x = magic_number(); x * x * x
local function multiply_func()
  return magic_number() ^ 3
end

-- should be recorded as exp2(magic_exp())
local function exp2_func()
  return 2 ^ magic_exp()
end

local iexp2_vm = iexp2_func()
local exp2_vm = exp2_func()
local vm = magic_number()
local vm3 = vm ^ 3

for _ = 1,5 do
  assert(one_func() == 1)
  assert(zero_exp_func() == 1)
  assert(identity_func(vm) == vm)
  assert(iexp2_func() == iexp2_vm)
  assert(exp2_func() == exp2_vm)
  -- fp errors may occur while pow to multiplication conversion
  assert_almost_equal(multiply_func(), vm3, 1e-14)
end
