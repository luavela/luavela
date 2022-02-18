-- Test on sink optimization.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Portions taken verbatim or adapted from LuaJIT testsuite
-- Copyright (C) 2005-2017 Mike Pall.

jit.opt.start(3, "sink")

-- Two traces will be recorded.
-- uJIT should be able to sink TDUP in the first one.
do --- Sink of stores with numbers.
  local x = {1.5, 0}
  for i=1,200 do x = {x[1]+1, 99.5}; x[2]=4.5; if i > 100 then end end
  assert(x[1] == 201.5)
  assert(x[2] == 4.5)
end

