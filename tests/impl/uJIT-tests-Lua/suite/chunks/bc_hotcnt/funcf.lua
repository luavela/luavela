-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local sum = 0

local function foo(a)
    sum = sum + a
end

foo(1)
foo(1)
foo(1)
foo(1)
foo(1)
foo(1)
