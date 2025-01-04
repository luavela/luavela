-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Passing fnum as parameter to prevent constant folding when running in jit-fp-classify.lua
local function test(fnum)
    local inum = 42
    local pinf = 1 / 0
    local ninf = -1 / 0
    local nan = ujit.math.nan

    -- ispinf
    assert(ujit.math.ispinf(inum) == false)
    assert(ujit.math.ispinf(fnum) == false)
    assert(ujit.math.ispinf(pinf) == true)
    assert(ujit.math.ispinf(ninf) == false)
    assert(ujit.math.ispinf(nan)  == false)

    -- isninf
    assert(ujit.math.isninf(inum) == false)
    assert(ujit.math.isninf(fnum) == false)
    assert(ujit.math.isninf(pinf) == false)
    assert(ujit.math.isninf(ninf) == true)
    assert(ujit.math.isninf(nan)  == false)

    -- isinf
    assert(ujit.math.isinf(inum) == false)
    assert(ujit.math.isinf(fnum) == false)
    assert(ujit.math.isinf(pinf) == true)
    assert(ujit.math.isinf(ninf) == true)
    assert(ujit.math.isinf(nan)  == false)

    -- isnan
    assert(ujit.math.isnan(inum) == false)
    assert(ujit.math.isnan(fnum) == false)
    assert(ujit.math.isnan(pinf) == false)
    assert(ujit.math.isnan(ninf) == false)
    assert(ujit.math.isnan(nan)  == true)

    -- isfinite
    assert(ujit.math.isfinite(inum) == true)
    assert(ujit.math.isfinite(fnum) == true)
    assert(ujit.math.isfinite(pinf) == false)
    assert(ujit.math.isfinite(ninf) == false)
    assert(ujit.math.isfinite(nan)  == false)

    -- test compatibility with Lua's math.huge
    local lua_pinf = math.huge
    local lua_ninf = -math.huge

    assert(ujit.math.ispinf(lua_pinf) == true)
    assert(ujit.math.ispinf(lua_ninf) == false)

    assert(ujit.math.isninf(lua_pinf) == false)
    assert(ujit.math.isninf(lua_ninf) == true)

    assert(ujit.math.isinf(lua_pinf) == true)
    assert(ujit.math.isinf(lua_ninf) == true)

    assert(ujit.math.isnan(lua_pinf) == false)
    assert(ujit.math.isnan(lua_ninf) == false)

    assert(ujit.math.isfinite(lua_pinf) == false)
    assert(ujit.math.isfinite(lua_ninf) == false)
end

test(42.5) -- test itself

return test -- return function for jit-fp-classify.lua to test
