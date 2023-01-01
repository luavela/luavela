-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

math.randomseed(0) -- constant seeding for deterministic results

-- The threshold below is for "biasing" execution flow in order to
-- "hint" compiler which branch to start with:
local threshold = 7
local random    = math.random
local rindex = ujit.table.rindex

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
