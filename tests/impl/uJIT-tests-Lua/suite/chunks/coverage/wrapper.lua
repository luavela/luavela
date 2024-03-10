-- Wrapper to run chunks with enabled platform-level coverage.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

ujit.coverage.start(arg[2])
loadfile(arg[1])()
ujit.coverage.stop()
