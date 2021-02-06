-- Tests string.find compilation implementation details.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "hotloop=3", "hotexit=2")

-- Tests specialization on 'plain' argument.
-- If 'plain' argument is not a literal, a guard on it should be emitted.

local plain = {true, true, true, true, false, false, false, false, false, false}

local res
for i = 1, #plain do
  res = string.find(i .. "." .. i, ".", 1, plain[i])
end
assert(res == 1)
