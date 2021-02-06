-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "hotloop=3")

local sealed_object = ujit.seal({key1 = {kkey1 = 'foo'}, key2 = {42}})

local objects = {
    {key1 = {kkey1 = 'foo'}, key2 = {48}}, -- 1 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {47}}, -- 2 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {46}}, -- 3 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {45}}, -- 4 <-- recording here
    {key1 = {kkey1 = 'foo'}, key2 = {44}}, -- 5 <-- mcode execution
    {key1 = {kkey1 = 'foo'}, key2 = {43}}, -- 6 <-- mcode execution
    sealed_object,                         -- 7 <-- mcode execution, BOOM
}

local N = #objects
for i = 1, N do
    objects[i].key2[1] = 0 -- Potential store to a sealed object by TSETB
end
