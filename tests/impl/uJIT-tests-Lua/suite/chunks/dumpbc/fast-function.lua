-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- no failure, just a note that the function
-- is not a Lua function:
ujit.dump.bc(io.stdout, math.sin)
