-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local jit_on = jit.status()
assert(jit_on, "JIT must be on for this test")

jit.opt.start("hotloop=3", "hotexit=3")

-- Number of iterations for the foo()'s loop and JIT options are tuned for the
-- following scenario of the loop execution during the first foo() call:
-- iteration 1 -- interpreter
-- iteration 2 -- interpreter
-- iteration 3 -- interpreter
-- iteration 4 -- root trace recording
-- iteration 5 -- mcode execution
-- iteration 6 -- mcode execution, side exit hit
-- iteration 7 -- mcode execution, side exit hit
-- iteration 8 -- mcode execution, side exit hit
-- iteration 9 -- side trace recording with possible escape via RETF

local function foo(N)
    local S = 0
    for i = 1, N do
        if i <= 5 then
            S = S + math.pow(-1, i) * i
        else
            S = S + i
        end
    end
    return S
end

local function main()
    local bar = foo(9)
    return bar
end

local res = main()
assert(res == 27)
