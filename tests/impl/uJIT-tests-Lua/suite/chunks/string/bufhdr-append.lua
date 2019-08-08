-- Tests on BUFHDR APPEND optimization.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start('hotloop=2', 'fold', 'jitstr')

local s
for i = 1, 10 do
  s = string.format("%s%d", s, i)
end
assert(s == "nil12345678910")
