-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function errmsg(s)
  local f = loadstring(s)
  local status, msg = pcall(f)
  assert(not status, "Code expected to fail")
  return msg
end

do
  local msg = errmsg("local a = {}; local b = 5; local c = (a or b) + 1")
  assert(string.find(msg, "attempt to perform arithmetic"))
  assert(string.find(msg, "a table value"))
  assert(not string.find(msg, "local 'b'"))
  assert(string.find(errmsg("local a = {}; local c = a + 1"), "local 'a'"))
end

do
  local msg = errmsg("a = {}; local b = 3; local c = (b or a)(3)")
  assert(string.find(msg, "attempt to call"))
  assert(string.find(msg, "a number value"))
  assert(not string.find(msg, "global 'a'"))
  assert(string.find(errmsg("a = {}; local c = a(3)"), "global 'a'"))
end

do
  local msg = errmsg("local m = {a = {}, b = 5}; local c = #(m.b or m.a)")
  assert(string.find(msg, "attempt to get length"))
  assert(string.find(msg, "a number value"))
  assert(not string.find(msg, "field 'a'"))
  assert(string.find(errmsg("local m = {b = 5}; local c = #m.b"), "field 'b'"))
end

do
  local msg = errmsg("local a = 'hello'; local function foo() local b = {}; return (b or a)..'world'; end; foo()")
  assert(string.find(msg, "attempt to concatenate"))
  assert(string.find(msg, "a table value"))
  assert(not string.find(msg, "upvalue 'a'"))
  assert(string.find(errmsg("local a = {}; local function foo() return a..'world'; end; foo()"),
                     "upvalue 'a'"))
end

do
  local msg = errmsg("local s = {}; s:find()")
  assert(string.find(msg, "attempt to call"))
  assert(string.find(msg, "a nil value"))
  assert(string.find(msg, "method 'find'"))
end
