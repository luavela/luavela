-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local t = {}
local mt = {
  __lt = function (_, _)
           print("__lt")
           return true
         end
}

setmetatable(t, mt)

print(t > 42)
