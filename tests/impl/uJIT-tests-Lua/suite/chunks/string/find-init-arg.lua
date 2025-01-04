-- Tests on recording of string.find with various 'init' argument values.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "jitstr", "hotloop=1", "hotexit=1")

local s = "ab1cd"
-- Every case is tripled to be surely executed on a trace
local t = {
  {init = -10, res = 3},
  {init = -10, res = 3},
  {init = -10, res = 3},
  {init = -4, res = 3},
  {init = -4, res = 3},
  {init = -4, res = 3},
  {init = -1, res = nil},
  {init = -1, res = nil},
  {init = -1, res = nil},
  {init = 0, res = 3},
  {init = 0, res = 3},
  {init = 0, res = 3},
  {init = 1, res = 3},
  {init = 1, res = 3},
  {init = 1, res = 3},
  {init = 3, res = 3},
  {init = 3, res = 3},
  {init = 3, res = 3},
  {init = 4, res = nil},
  {init = 4, res = nil},
  {init = 4, res = nil},
  {init = 5, res = nil},
  {init = 5, res = nil},
  {init = 5, res = nil},
  {init = 10, res = nil},
  {init = 10, res = nil},
  {init = 10, res = nil},
}
for i = 1, #t do
  assert(string.find(s, "1", t[i].init) == t[i].res)
end
