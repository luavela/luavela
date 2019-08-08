-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local a = { 5, 5, 5, 5, 5 }
local sum = 0

for _, v in ipairs(a) do
    sum = sum + v
end

print(sum)
