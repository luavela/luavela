-- This performance benchmark was initially exposed in LuaJIT mailing list
-- by Laurent Deniau (Laurent.Deniau@cern.ch):
--
-- This particualar version is slightly modified in order to make
-- callgrind profiling possible.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function find_duplicates(inp)
  local out = {}
  for _,v in ipairs(inp) do
    out[v] = out[v] and out[v]+1 or 1
  end
  for _,v in ipairs(inp) do
    if out[v] > 1 then out[#out+1], out[v] = v, 0 end
  end
  return out
end

local inp, out
inp = {"b","a","c","c","e","a","c","d","c","d"}

-- Different iteration counts are possible for different situations.
-- In case you need an integral performance test (i.e. to run with `time`),
-- you can use original 10'000'000 iteration count.
-- In case you need a speedrun, callgrind profile or readable JIT dump, you
-- can use smaller numbers, i.e. 100'000. Note that big iteration counts will
-- result in callgrind OOMs.
--for i=1,100000 do
for _=1,10000000 do
  out = find_duplicates(inp)
end

io.write('{ "', table.concat(out, '", "'), '" }\n')

