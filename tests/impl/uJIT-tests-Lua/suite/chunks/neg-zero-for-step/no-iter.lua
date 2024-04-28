-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Both stock Lua 5.1 and LuaJIT/uJIT
-- do not produce a single iteration with this code:
for _ = 0, 1, -0  do print'OK' end

