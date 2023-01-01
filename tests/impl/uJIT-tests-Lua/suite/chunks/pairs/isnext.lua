-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(4, "jitpairs", "hotloop=1", "hotexit=1")
local t = {1, 2, 3, 4, 5, 6, a = 7, b = 8, c = 9, d = 10, e = 11, f = 12}

local count = 0
local function dummy() end
local function f(next)
  for _, v in next, t, nil do
    count = count + 1
    if v == 10 then
      -- These will cause despecialization of ISNEXT/ITERN/JITRNL
      -- to JMP/ITERC/ITERL. If ITERL target is restored incorrectly,
      -- uJIT will likely segfault.
      f(dummy)
    end
  end
end

f(next)
assert(count == 12)
