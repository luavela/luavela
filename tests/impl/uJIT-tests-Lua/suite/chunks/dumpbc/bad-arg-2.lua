-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local bar = 42
ujit.dump.bc(io.stdout, bar)
