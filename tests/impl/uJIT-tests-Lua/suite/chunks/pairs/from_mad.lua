-- Test on pairs/next compilation.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Portions taken verbatim or adapted from MAD testsuite
-- Copyright CERN 2016+

jit.opt.start(4, "jitpairs")

local function pairs_iter(var, key)
  local k, v = next(var, key)
  while type(k) ~= 'nil' and type(k) ~= 'string' do
    k, v = next(var, k)
  end
  return k, v
end

local function my_pairs(var)
  return pairs_iter, var, nil
end

local tostr = function(s)
  local str = ''
  for k, _ in my_pairs(s) do str = str .. tostring(k) .. ', ' end
  return str .. '#=' .. tostring(#s)
end

local s
local t = {2, 3, z = function() return "3" end }
for _ = 1, 100 do
  s = tostr(t)
end
assert(s == "z, #=2")
