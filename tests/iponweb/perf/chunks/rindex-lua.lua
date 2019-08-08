-- This performance test assesses the effect of fixing the bug in fusing operands in AREF.
-- Prior to the fix, incorrect fusing of AREF's operand resulted in misaligned
-- TValue* pointer and led to trace explosion which slowed down compiled code
-- 15-20x compared to interpreter (sic).
--
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

math.randomseed(0) -- constant seeding for deterministic results

-- The threshold below is for "biasing" execution flow in order to
-- "hint" compiler which branch to start with:
local threshold = 7
local random    = math.random

local rindex = function (t, ...)
    for n = 1, select('#', ...) do
        local index = select(n, ...)
        if type(t) == 'table' then t = t[index] else return nil end
    end
   return t
end

local t = {
    x = {
        y = {
            z = 'yep',
        },
    },
}

local niter = tonumber(arg and arg[1]) or 1e7
local item
for _ = 1, niter do
    local r = random(1, 10)
    local T = r > threshold and t or t.x
    item = rindex(T, 'x', 'y', 'z')
    assert((T == t and item == 'yep') or (T == t.x and item == nil))
end

