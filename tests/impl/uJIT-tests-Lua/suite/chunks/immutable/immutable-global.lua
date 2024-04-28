-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- This test is moved to a separate chunk
-- to avoid any side effects in the main chunk.

-- If you want to change the name of the variable, tune .luacheckrc as well :-)
FOO = 'BAR'

local status, errmsg

local function mutate1()
	FOO = 'BAZ'
end

local function mutate2(t)
	t.FOO = 'BAZ'
end

ujit.immutable(_G)

status, errmsg = pcall(mutate1)
assert(status == false)
assert(string.match(errmsg, "attempt to modify .+ object$"))

status, errmsg = pcall(mutate2, _G)
assert(status == false)
assert(string.match(errmsg, "attempt to modify .+ object$"))
