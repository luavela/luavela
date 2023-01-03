-- Tests if table.concat throws an error after being recorded.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("jittabcat", "hotloop=2")

local t = {
  {1, 2, 3},
  {4, 5, 6},
  {7, 8, 9}, -- Recording
  {1, 2, 3},
  {4, nil, 6}
}

local N = #t

for i=1,N do
  table.concat(t[i], "", 1, 3)
end

