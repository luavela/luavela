-- Tests string.find compilation implementation details.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local patterns = {}
for i=1,100 do
  -- Test (along with assert in runner) that we don't specialize
  -- on pattern value
  patterns[i] = "abc"..i
end

-- Test (along with asserts below) that we don't overspecialize
-- on finding plain patterns
patterns[100] = "(%d).*(%d)"

jit.on()
-- 'hotloop' value and iteration count value below
-- give a possibility to create side traces
jit.opt.start("jitstr", "hotloop=10")

local s, e
for i=1,100 do
  s, e = string.find("dgh12abc3iup", patterns[i])
end
assert(s == 4)
assert(e == 9)

