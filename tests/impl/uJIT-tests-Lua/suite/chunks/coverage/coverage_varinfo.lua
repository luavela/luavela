-- Tests if instrumentation for coverage corrupt variable info
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
--
-- Portions taken verbatim or adapted from LuaJIT testsuite

do --- default level in xpcall
  local line
  local ok, msg = xpcall(function()
    local _ = nil
    line = debug.getinfo(1, "l").currentline; error("emsg")
  end, function(m)
    assert(debug.getlocal(3, 1) == "_")
    return m .."xp"
  end)
  assert(ok == false)
  assert(msg:find("^.-:".. line ..": emsgxp$"))
end

do --- level 2 in xpcall
  local line
  local ok, msg = xpcall(function()
    local function f() error("emsg", 2) end
    line = debug.getinfo(1, "l").currentline; f()
  end, function(m)
    assert(debug.getlocal(4, 1) == "f")
    return m .."xp2"
  end)
  assert(ok == false)
  assert(msg:find("^.-:".. line ..": emsgxp2$"))
end

do
  local function f(name)
    local a = debug.getinfo(1)
    assert(a.name == name and a.namewhat == 'local')
  end

  local function g(x)
    print("hello world")
    x('x')
  end

  g(f)
end
