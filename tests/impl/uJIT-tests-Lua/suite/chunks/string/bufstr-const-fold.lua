-- Tests on BUFSTR constant folding.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start('fold', 'jitstr')

local a
local b
for _ = 1, 100 do
  a = string.format("%s", "a")
  b = string.format("%s%s", a, "b")
end
assert(a == "a")
assert(b == "ab")

local s
for _ = 1, 100 do
  s = string.format("%s", "")
end
assert(s == "")
