-- Tests on string.format implementation defined cases.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function tostr()
  return "Hello, world"
end

local t = {}

-- Note: vanilla Lua 5.1 throws 'bad argument #2'
-- on string.format calls tested below

do -- __tostring returns builtin function
  setmetatable(t, {__tostring = function() return math.sin end})
  local formatted = string.format("%s", t)
  assert(formatted == tostring(tostring(t)))
  assert(formatted:match("function: builtin#%d"))
end

do -- __tostring returns regular function
  setmetatable(t, {__tostring = function() return tostr end})
  local formatted = string.format("%s", t)
  assert(formatted == tostring(tostring(t)))
  assert(formatted:match("function: 0x%x"))
end
