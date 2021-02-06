-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "jitpairs", "hotloop=1")

local t = {
  1, --recording
  ["key"] = "value"
}

local r = {}
for k, v in pairs(t) do
  table.insert(r, k)
  table.insert(r, v)
end

assert(r[1] == 1)
assert(r[2] == 1)
assert(r[3] == "key")
assert(r[4] == "value")
