-- Tests 'exclude' functionality of platform-level coverage.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local excludes = {}
table.insert(excludes, arg[3])

local started = ujit.coverage.start(arg[2], excludes)
print(string.format("started: %s", tostring(started)))
loadfile(arg[1])()
ujit.coverage.stop()
