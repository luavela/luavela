-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function foo1()
  local ret = 0
  for i = 1, 1000000 do
    if i % 2 then
      ret = ret - i
    else
      ret = ret + i
    end
  end
end

local function foo2()
  local ret = 0
  for i = 1, 1e2 do
    if i % 2 then
      ret = ret - i
    else
      ret = ret + i
    end
  end
end


local function foo3()
  local ret = 0
  for i = 1, 1e2 do
    foo1()
    foo2()
    if i % 2 then
      ret = ret - i
    else
      ret = ret + i
    end
  end
end

local function foo4()
  local ret = 0
  foo2()
  foo3()
  for i = 1, 1e2 do
    if i % 2 then
      ret = ret - i
    else
      ret = ret + i
    end
  end
end

local function foo5()
  local ret = 0
  foo3()
  foo4()
  for i = 1, 1e2 do
   foo1()
   if i % 2 then
      ret = ret - i
    else
      ret = ret + i
    end
  end
end

local function foo6()
  local ret = 0
  foo4()
  foo5()
  for i = 1, 1e2 do
    if i % 2 then
      ret = ret - i
    else
      ret = ret + i
    end
  end
end

local fname = ''

if ujit.profile.available() then
  assert(ujit.profile.init() == true)
  print "Start profiling"

  local interval = 50
  local binary_name = 'profile.bin'
  local started, fname_real = ujit.profile.start(interval, 'callgraph', binary_name)
  local fname_stub_len = string.len(binary_name)

  assert(started)
  assert(string.sub(fname_real, 1, fname_stub_len) == binary_name)
  assert(string.match(
      string.sub(fname_real, fname_stub_len + 1, -1),
      "^%.[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$"
  ))
  fname = fname_real
end

foo6()

local counters = ujit.profile.stop()

assert(counters.IDLE   >= 0)
assert(counters.INTERP >= 0)
assert(counters.GC     >= 0)
assert(counters.EXIT   >= 0)
assert(counters.RECORD >= 0)
assert(counters.OPT    >= 0)
assert(counters.ASM    >= 0)
assert(counters.TRACE  >= 0)
assert(counters.LFUNC  >= 0)
assert(counters.CFUNC  >= 0)

ujit.profile.terminate()

if fname then
    os.remove(fname)
end
