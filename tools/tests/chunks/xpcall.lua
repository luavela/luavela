-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local interval = 10000 -- microseconds
local sleep = 5 * interval * 1e-6 -- seconds

local function f()
  return "a" + 2
end

local function err(x)
  local start = os.clock()
  while os.clock() < start + sleep do
  end
  return "oh no!"
end

if ujit.profile.available() then
  assert(ujit.profile.init() == true)
  local started = ujit.profile.start(interval, 'leaf', arg[1])
  assert(started == true)
end

xpcall(f, err)

ujit.profile.stop()
ujit.profile.terminate()
