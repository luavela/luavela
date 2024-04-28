-- Tests 'pause' functionality of platform-level coverage.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

ujit.coverage.pause()
local sum = 0
for i=1,10 do
  sum = sum + i
end
ujit.coverage.unpause()
print(sum)
