-- Test on sink optimization.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Portions taken verbatim or adapted from LuaJIT testsuite
-- Copyright (C) 2005-2017 Mike Pall.

jit.opt.start(3, "sink")

do --- Sink outermost table of nested TNEW/TDUP.
  local x
  for i=1,100 do --- Sink TNEW
    local t = {[0]={{1,i}}}
    if i == 90 then x = t end
    assert(t[0][1][2] == i)
  end
  assert(x[0][1][2] == 90)
  for i=1,100 do --- Sink TDUP
    local t = {foo={bar={baz=i}}}
    if i == 90 then x = t end
    assert(t.foo.bar.baz == i)
  end
  assert(x.foo.bar.baz == 90)
end

