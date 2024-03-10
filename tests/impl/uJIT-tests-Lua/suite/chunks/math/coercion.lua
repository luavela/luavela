-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
--
-- We don't test for proper coercion of strings with uppercase letters
-- (like "INF" or "iNf") here because this is done in tests such as
-- uJIT-tests-C's 'test_cstr.c' and LuaJIT's 'test/misc/tonumber_scan.lua'

-- Passing fnum as parameter to prevent constant folding when running in jit-fp-classify.lua
local function test(fnum)
    local inf = "inf"
    local pinf = "+inf"
    local ninf = "-inf"
    local nan = "nan"

    -- ispinf
    assert(ujit.math.ispinf(fnum) == false)
    assert(ujit.math.ispinf(inf)  == true)
    assert(ujit.math.ispinf(pinf) == true)
    assert(ujit.math.ispinf(ninf) == false)
    assert(ujit.math.ispinf(nan)  == false)

    -- isninf
    assert(ujit.math.isninf(fnum) == false)
    assert(ujit.math.isninf(inf)  == false)
    assert(ujit.math.isninf(pinf) == false)
    assert(ujit.math.isninf(ninf) == true)
    assert(ujit.math.isninf(nan)  == false)

    -- isinf
    assert(ujit.math.isinf(fnum) == false)
    assert(ujit.math.isinf(inf)  == true)
    assert(ujit.math.isinf(pinf) == true)
    assert(ujit.math.isinf(ninf) == true)
    assert(ujit.math.isinf(nan)  == false)

    -- isnan
    assert(ujit.math.isnan(fnum) == false)
    assert(ujit.math.isnan(inf)  == false)
    assert(ujit.math.isnan(pinf) == false)
    assert(ujit.math.isnan(ninf) == false)
    assert(ujit.math.isnan(nan)  == true)

    -- isfinite
    assert(ujit.math.isfinite(fnum) == true)
    assert(ujit.math.isfinite(inf)  == false)
    assert(ujit.math.isfinite(pinf) == false)
    assert(ujit.math.isfinite(ninf) == false)
    assert(ujit.math.isfinite(nan)  == false)
end

test("42.5") -- test itself

return test -- return function for jit-fp-classify.lua to test
