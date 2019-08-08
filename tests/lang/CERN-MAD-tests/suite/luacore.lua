--[=[
 o-----------------------------------------------------------------------------o
 |
 | Lua core feature regression tests
 |
 | Methodical Accelerator Design - Copyright CERN 2016+
 | Support: http://cern.ch/mad  - mad at cern.ch
 | Authors: L. Deniau, laurent.deniau at cern.ch
 | Contrib: -
 |
 o-----------------------------------------------------------------------------o
 | You can redistribute this file and/or modify it under the terms of the GNU
 | General Public License GPLv3 (or later), as published by the Free Software
 | Foundation. This file is distributed in the hope that it will be useful, but
 | WITHOUT ANY WARRANTY OF ANY KIND. See http://gnu.org/licenses for details.
 o-----------------------------------------------------------------------------o

  Purpose:
  - Provide a small set of test suites for some Lua features.

 o-----------------------------------------------------------------------------o
]=]

-- locals ---------------------------------------------------------------------o

local utest = MAD and MAD.utest or require("luaunit")

local assertEquals       = utest.assertEquals
local assertAlmostEquals = utest.assertAlmostEquals

-- regression test suite ------------------------------------------------------o

TestLuaCore = {}

-- modf regression test -------------------------------------------------------o

function TestLuaCore:testFrac()
  local modf = math.modf
  local eps  = 2.2204460492503131e-16
  local tiny = 2.2250738585072012e-308
  local huge = 1.7976931348623158e+308
  local inf  = 1/0
  local num = { 2^31, 2^32, 2^52, 2^53, 2^63, 2^64, huge, inf }

  local function second(a,b) return b end
  local function frac(x) return second(modf(x)) end

  for i,v in ipairs(num) do
    if v == inf then break end -- pb with luajit?
    assertEquals( frac( v+eps), second(modf( v+eps)) )
    assertEquals( frac( v-eps), second(modf( v-eps)) )
    assertEquals( frac(-v+eps), second(modf(-v+eps)) )
    assertEquals( frac(-v-eps), second(modf(-v-eps)) )
    assertEquals( frac( v+0.1), second(modf( v+0.1)) )
    assertEquals( frac( v-0.1), second(modf( v-0.1)) )
    assertEquals( frac(-v+0.1), second(modf(-v+0.1)) )
    assertEquals( frac(-v-0.1), second(modf(-v-0.1)) )
    assertEquals( frac( v+0.7), second(modf( v+0.7)) )
    assertEquals( frac( v-0.7), second(modf( v-0.7)) )
    assertEquals( frac(-v+0.7), second(modf(-v+0.7)) )
    assertEquals( frac(-v-0.7), second(modf(-v-0.7)) )
  end

  assertEquals( frac(    0) ,     0 )
  assertEquals( frac( tiny) ,  tiny )
  assertEquals( frac(  0.1) ,   0.1 )
  assertEquals( frac(  0.5) ,   0.5 )
  assertEquals( frac(  0.7) ,   0.7 )
  assertEquals( frac(    1) ,     0 )
  assertEquals( frac(  1.5) ,   0.5 )
  assertEquals( frac(  1.7) ,   0.7 )
  assertEquals( frac( huge) ,     0 )
--  assertEquals( frac(  inf) ,     0 ) -- get NaN, pb with luajit?
  assertEquals( frac(-   0) , -   0 )
  assertEquals( frac(-tiny) , -tiny )
  assertEquals( frac(- 0.1) , - 0.1 )
  assertEquals( frac(- 0.5) , - 0.5 )
  assertEquals( frac(- 0.7) , - 0.7 )
  assertEquals( frac(-   1) , -   0 )
  assertEquals( frac(- 1.5) , - 0.5 )
  assertEquals( frac(- 1.7) , - 0.7 )
  assertEquals( frac(-huge) , -   0 )
--  assertEquals( frac(- inf) ,     0 ) -- get NaN, pb with luajit?
end

-- get_primes regression test -------------------------------------------------o

local function get_primes(n)
  local function isPrimeDivisible(self, c)
    for i=3, self.prime_count do
      if self.primes[i] * self.primes[i] > c then break end
      if c % self.primes[i] == 0 then return true end
    end
    return false
  end

  local function addPrime(self, c)
    self.prime_count = self.prime_count + 1
    self.primes[self.prime_count] = c
  end

  local p = { prime_count=3, primes={ 1,2,3 } }
  local c = 5
  while p.prime_count < n do
    if not isPrimeDivisible(p, c) then
      addPrime(p, c)
    end
    c = c + 2
  end
  return p
end

function TestLuaCore:testPrimes()
  local p = get_primes(1e3)
  assertEquals( p.primes[p.prime_count-0], 7907 )
  assertEquals( p.primes[p.prime_count-1], 7901 )
  assertEquals( p.primes[p.prime_count-2], 7883 )
  assertEquals( p.primes[p.prime_count-3], 7879 )
  assertEquals( p.primes[p.prime_count-4], 7877 )
  assertEquals( p.primes[p.prime_count-5], 7873 )
end

-- find_duplicate regression test ---------------------------------------------o

local function find_duplicates(inp)
  local res = {}
  for _,v in ipairs(inp) do
    res[v] = res[v] and res[v]+1 or 1
  end
  for _,v in ipairs(inp) do
    if res[v] and res[v] > 1 then
      res[#res+1] = v
    end
    res[v] = nil
  end
  return res
end

local function find_duplicates2(inp, res)
  for i=1,#res do res[i]=nil end
  for _,v in ipairs(inp) do
    res[v] = res[v] and res[v]+1 or 1
  end
  for _,v in ipairs(inp) do
    if res[v] and res[v] > 1 then
      res[#res+1] = v
    end
    res[v] = nil
  end
end

function TestLuaCore:testDuplicates()
  local inp = {'b','a','c','c','e','a','c','d','c','d'}
  local out = {'a','c','d'}
  local res = {}
  assertEquals( find_duplicates(inp), out )
  assertEquals( find_duplicates2(inp, res) or res, out )
end

-- performance test suite -----------------------------------------------------o

Test_LuaCore = {}

function Test_LuaCore:testPrimes()
  local t0, p = os.clock()
  p = get_primes(2e5)
  local dt = os.clock() - t0
  assertEquals( p.primes[p.prime_count], 2750131 )
  -- UJIT: Disabled potentially unstable part of the test
  -- assertAlmostEquals( dt , 0.5, 1 )
end

function Test_LuaCore:testDuplicates()
  local inp = {'b','a','c','c','e','a','c','d','c','d'}
  local out = {'a','c','d'}
  local t0, res = os.clock()
  for i=1,5e5 do res = find_duplicates(inp) end
  local dt = os.clock() - t0
  assertEquals( res, out )
  -- UJIT: Disabled potentially unstable part of the test
  -- assertAlmostEquals( dt , 0.4, 1 )
end

function Test_LuaCore:testDuplicates2()
  local inp = {'b','a','c','c','e','a','c','d','c','d'}
  local out = {'a','c','d'}
  local res = {}
  local t0 = os.clock()
  for i=1,5e5 do find_duplicates2(inp, res) end
  local dt = os.clock() - t0
  assertEquals( res, out )
  -- UJIT: Disabled potentially unstable part of the test
  -- assertAlmostEquals( dt , 0.2, 1 )
end

function Test_LuaCore:testLinkedList()
  local nxt = {}

  local function generate(n)
    local t = {x=1}
    for j=1,n do t = {[nxt]=t} end
    return t
  end

  local function find(t,k)
    if t[k] ~= nil then return t[k] end
    return find(t[nxt],k)
  end

  local l, s, n = generate(10), 0, 5e6
  local t0 = os.clock()
  for i=1,n do s = s + find(l, 'x') end
  local dt = os.clock() - t0
  assertEquals( s, n )
  -- UJIT: Disabled potentially unstable part of the test
  -- assertAlmostEquals( dt , 0.5, 1 )
end

-- end ------------------------------------------------------------------------o

-- run as a standalone test
if MAD == nil then
  -- UJIT: Amended in favor of TAP-based stand-alone run.
  -- os.exit( utest.LuaUnit.run() )
  local runner = require('luaunit').LuaUnit.new()
  runner:setOutputType("tap")
  runner:runSuite()
end
