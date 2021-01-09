-- Tests on BUFSTR CSE.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start('fold', 'cse', 'dse', 'loop', 'jitstr')

local s
local t = {"hello", "world"}
for _ = 1, 100 do
  -- string creation will be hoisted only if CSE of BUF IR chains is implemented
  s = string.format("%s, %s!", t[1], t[2])
end
assert(s == "hello, world!")
