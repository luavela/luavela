-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local traverse_iterate = traverse_iterate

local function get_table()
        return {
                ['(AY{6&Zb{v'] = 'a',
                ['M*;tbAp*BS'] = 'b',
                ['Hx|eH8]PU#'] = 'c',
                ['Q6Hjz1M9G,'] = 'd',
                ['a/^)k7Y%-E'] = 'e',
                ['GbY|Z,NkaV'] = 'f',
                ['sz@h*W+U@|'] = 'g',
                ['3Tu[C&m2w8'] = 'h',
                ['V{}!6{iG!P'] = 'i',
                ['|)*7L2IEiL'] = 'j',
                ['ESLUE'] = 1,
                ['7d*Fp'] = 2,
                ['W|%ue'] = 3,
                ['_d_nR'] = 4,
                ['}"!eH'] = 5,
                ['vGjty'] = 6,
                ['):uNi'] = 7,
                ['kG5r<'] = 8,
                ['jzJHb'] = 9,
                ['Y6y*j'] = 10,
        }
end

local N = 1e8
local t = get_table()
local n = 0
for _ = 1, N do
        n = n + traverse_iterate(t)
end
print(n)
