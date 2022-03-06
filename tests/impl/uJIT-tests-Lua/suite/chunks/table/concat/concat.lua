-- Tests for table.concat and its recording.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("jittabcat", "hotloop=3")

local str
local tab = {"abc", 126, 0.25}

for i=1, 10 do
  local concat = table.concat{"abcdefg", 123, 1/3, "zxcvb", 0.25, 345}
  -- Lua uses "%.14g" for number to string conversion
  assert(concat == "abcdefg1230.33333333333333zxcvb0.25345")

  concat = table.concat({127, 0, 0, 1, nil}, ".")
  assert(concat == "127.0.0.1")

  concat = table.concat({"a", "b"}, "", 3, 1)
  assert(concat == "")

  concat = table.concat({"a", "b", "c"}, "", 2)
  assert(concat == "bc")

  -- nil is default, i.e. 3 here for 'end'
  concat = table.concat({"a", "b", "c"}, "", 2, nil)
  assert(concat == "bc")

  -- nil is default, i.e. 1 for 'start'
  concat = table.concat({"a", "b", "c"}, "", nil, 2)
  assert(concat == "ab")

  local t = {"a", "b", "c", "d"}
  concat = table.concat(t, "", nil, #t)
  assert(concat == "abcd")

  -- 'sep' can be a number
  concat = table.concat({1, 2, 3, 4}, 4, 2)
  assert(concat == "24344")

  concat = table.concat({})
  assert(concat == "")

  -- 'start' and 'end' can be negative
  concat = table.concat({[-2] = "a", [-1] = "b", [0] = "c", "d", "e"}, "", -2)
  assert(concat == "abcde")

  str = table.concat(tab, i)
end

assert(str == "abc10126100.25")
