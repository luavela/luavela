-- Tests 'pause' functionality of platform-level coverage.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

ujit.coverage.pause()
local sum = 0
for i=1,10 do
  sum = sum + i
end
ujit.coverage.unpause()
print(sum)
