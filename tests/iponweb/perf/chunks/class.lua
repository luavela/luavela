-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
--
-- Reqired by history.lua and history2.lua benchmarks

local Class = {}

Class.__index = Class

function Class:new(object)
   object = object or {}
   object.__parent = self
   object.__index = object
   return setmetatable(object, self)
end

return Class
