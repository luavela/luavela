-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "jitcat", "jitstr", "hotloop=2")

do
  local s, e, plain
  for i = 1, 10 do
    s, e = string.find("abcd"..i, "1")
    plain = string.find("abcd%"..i, "%1", 0, true)
  end
  assert(s == 5)
  assert(e == 5)
  assert(plain == 5)
end

do
  local upper, lower
  for i = 1, 10 do
    local s = "abcdefABCDEF\0.,?%&"..i
    upper = string.upper(s)
    lower = string.lower(s)
  end
  assert(upper == "ABCDEFABCDEF\0.,?%&10")
  assert(lower == "abcdefabcdef\0.,?%&10")
end
