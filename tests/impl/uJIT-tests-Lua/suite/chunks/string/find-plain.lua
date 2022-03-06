-- Tests string.find compilation implementation details.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "hotloop=3")

-- Tests specialization on 'plain' argument.
-- Function with its usage below should generate the single trace
-- (i.e. trace that performs plain search and always succeeds).

local function find(str, pat)
  return string.find(str, pat, 1, true)
end

local pat = "."
local res
for i=1,100 do
  res = find(i .. "." .. i, pat)
end
assert(res == 4)
