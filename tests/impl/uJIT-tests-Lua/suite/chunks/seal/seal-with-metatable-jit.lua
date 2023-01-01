-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

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

local T = {
    __index = {foo = 'bar'},
}

local N = #objects
for i = 1, N do
    setmetatable(objects[i], T)
end
