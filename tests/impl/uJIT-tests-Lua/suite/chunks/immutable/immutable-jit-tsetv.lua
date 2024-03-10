-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.opt.start(3, "hotloop=3")

local immutable_object = ujit.immutable{key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}

local objects = {
    {key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}, -- 1 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}, -- 2 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}, -- 3 <-- interpreter
    {key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}, -- 4 <-- recording here
    {key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}, -- 5 <-- mcode execution
    {key1 = {kkey1 = 'foo'}, key2 = {kkey = 'kkey1'}}, -- 6 <-- mcode execution
    immutable_object,                                  -- 7 <-- mcode execution, BOOM
}

local N = #objects
for i = 1, N do
    local object = objects[i]
    local key = object.key2.kkey
    object.key1[key] = 'bar' -- Potential store to an immutable object by TSETV
end
