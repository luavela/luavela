-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start("hotloop=2")

local function assert_modification_error(...)
    local status, errmsg = pcall(...)
    assert(status == false)
    assert(string.match(errmsg, "attempt to modify .+ object$"))
end

local modify = function (t) t.i = 100 end

local tbl = {}
local nested = {}
for i = 1,5 do
    tbl[i] = ujit.immutable{ i = 2 * i }
    nested[i] = ujit.immutable{ i = {i = 3 * i} }
end

assert_modification_error(modify, tbl[5])
assert_modification_error(modify, nested[5].i)
