-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local array = {}
local hash = {}
local hotloop = 2
local size = 16 - hotloop - 1
for i = 1, size do
  array[i] = i
end
for i = 1, size do
  hash[i * 1e6] = i
end

jit.on()
jit.opt.start(4, "jitpairs", "hotloop=" .. hotloop)

-- Reallocation will happen on insertion of the 17th element, i.e. on a trace
-- Let's try to catch a segfault if trace store pointers to old memory chunk.

for _, v in pairs(array) do
  size = size + 1
  array[size % 32] = v -- '%' needed to avoid infinite insertion
end

for k, v in pairs(hash) do
  hash[k * 2] = v
end

