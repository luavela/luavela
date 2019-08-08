-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local t = {["abc"] = 23}
assert(type(t) == "table")

for k, v in pairs(t) do
  assert(k ~= nil)
  assert(v ~= nil)
end
