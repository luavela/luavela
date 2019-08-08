-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "hotloop=3")

local sealed_object = ujit.seal({key1 = {kkey1 = 'foo'}, key2 = {}})

local objects = {
    {key1 = {kkey1 = 'foo'}, key2 = {}}, -- 1 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {}}, -- 2 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {}}, -- 3 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {}}, -- 4 <-- recording here
    {key1 = {kkey1 = 'foo'}, key2 = {}}, -- 5 <-- mcode execution
    {key1 = {kkey1 = 'foo'}, key2 = {}}, -- 6 <-- mcode execution
    sealed_object,                       -- 7 <-- mcode execution, BOOM
}

local N = #objects
for i = 1, N do
    objects[i].key1.kkey1 = 'bar' -- Potential store to a sealed object by TSETS
end
