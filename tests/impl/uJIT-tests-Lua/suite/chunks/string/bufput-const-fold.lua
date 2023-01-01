-- Tests on BUFPUT constant folding.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start('fold', 'jitstr')

local s
for i = 1, 100 do
  local n = 42.42
  s = string.format("%d-%s-%s-%d-%s", i, "foo", "bar", 2 * n, "")
end
assert(s == "100-foo-bar-84-")
