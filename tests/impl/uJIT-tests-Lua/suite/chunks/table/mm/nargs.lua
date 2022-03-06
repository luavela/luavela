-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local t = {[1] = 42, [2] = 1984}
assert(#t == 2)

setmetatable(t, {
    __len = function(t1, t2, _)
        assert(_ == nil)
        assert(rawequal(t1, t2))
        assert(type(t1) == "table")
        assert(t1[1] == 42)
        assert(t1[2] == 1984)
        return 1
    end,
    __unm = function(t1, t2, _)
        assert(_ == nil)
        assert(rawequal(t1, t2))
        assert(type(t1) == "table")
        assert(t1[1] == 42)
        assert(t1[2] == 1984)
        return -1
    end,
    __metatable = getmetatable,
})
assert(-#t == -t)
assert((getmetatable(t))(t) == getmetatable)
