-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
--
-- Portions taken verbatim or adapted from official Lua 5.1 testsuite

-- A lot of parsing inside this test invokes GC.
-- Coverage internal object should survive it.

f = [[
return function ( a , b , c , d , e )
  local x = a >= b or c or ( d and e ) or nil
  return x
end , { a = 1 , b = 2 >= 1 , } or { 1 };
]]
f = string.gsub(f, "%s+", "\n");   -- force a SETLINE between opcodes
f,a = loadstring(f)();
assert(a.a == 1 and a.b)

function F(a)
  assert(debug.getinfo(1, "n").name == 'F')
  return a,2,3
end

a,b = F(1)~=nil; assert(a == true and b == nil);
a,b = F(nil)==nil; assert(a == true and b == nil)

----------------------------------------------------------------
-- creates all combinations of
-- [not] ([not] arg op [not] (arg op [not] arg ))
-- and tests each one

function ID(x) return x end

function f(t, i)
  local b = t.n
  local res = math.mod(math.floor(i/c), b)+1
  c = c*b
  return t[res]
end

local arg = {" ( 1 < 2 ) ", " ( 1 >= 2 ) ", " F ( ) ", "  nil "; n=4}

local op = {" and ", " or ", " == ", " ~= "; n=4}

local neg = {" ", " not "; n=2}

local i = 0
repeat
  c = 1
  local s = f(neg, i)..'ID('..f(neg, i)..f(arg, i)..f(op, i)..
            f(neg, i)..'ID('..f(arg, i)..f(op, i)..f(neg, i)..f(arg, i)..'))'
  local s1 = string.gsub(s, 'ID', '')
  K,X,NX,WX1,WX2 = nil
  s = string.format([[
      local a = %s
      local b = not %s
      K = b
      local xxx;
      if %s then X = a  else X = b end
      if %s then NX = b  else NX = a end
      while %s do WX1 = a; break end
      while %s do WX2 = a; break end
      repeat if (%s) then break end; assert(b)  until not(%s)
  ]], s1, s, s1, s, s1, s, s1, s, s)
  assert(loadstring(s))()
  assert(X and not NX and not WX1 == K and not WX2 == K)
  if math.mod(i,4000) == 0 then print('+') end
  i = i+1
until i==c

print'OK'
