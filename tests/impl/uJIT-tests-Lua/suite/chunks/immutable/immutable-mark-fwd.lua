-- Tests elimination of redundant immutability guards.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3)

local sum = 0
for i=1,100 do
  local t = {i}
  sum = sum + t[1]
end
assert(sum == 5050)
