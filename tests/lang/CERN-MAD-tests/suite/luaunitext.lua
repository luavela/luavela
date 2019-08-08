--[=[
 o-----------------------------------------------------------------------------o
 |
 | Luaunit extension test suites
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
  - Test extensions of luaunit

 o-----------------------------------------------------------------------------o
]=]

-- locals ---------------------------------------------------------------------o

local utest = MAD and MAD.utest or require("luaunit")

local assertEquals           = utest.assertEquals
local assertAlmostEquals     = utest.assertAlmostEquals
local assertAllAlmostEquals  = utest.assertAllAlmostEquals
local assertErrorMsgContains = utest.assertErrorMsgContains

-- regression test suite ------------------------------------------------------o

TestLuaUnitExt = {}

function TestLuaUnitExt:testAllEqualError()
  local errmsg = "assertAllAlmostEquals: must supply only number or table arguments."
  local actual, expected = 0, 0
  assertErrorMsgContains(errmsg, assertAllAlmostEquals, actual, expected)
end

function TestLuaUnitExt:testAllEqualNumbers()
  local actual   = { -1.1*2, 0*2, 1.1*2 }
  local expected = { -2.2  , 0  , 2.2   }
  assertAllAlmostEquals (actual, expected)
end

function TestLuaUnitExt:testAllEqualNumbers2()
  local actual   = { -1.1-1e-15, -1.1, 0-1e-15, 0, 0+1e-15, 1.1, 1.1+1e-15 }
  local expected = { -1.1      , -1.1, 0      , 0, 0      , 1.1, 1.1       }
  local margin   = {      2e-15,    0,   1e-15, 0,   1e-15,   0,     2e-15 }
  assertAllAlmostEquals (actual, expected, margin)
end

function TestLuaUnitExt:testAllEqualMixed()
  local actual   = { -1.1-1e-15, {}, 0-1e-15, 0, 0+1e-15, {}, 1.1+1e-15 }
  local expected = { -1.1      , {}, 0      , 0, 0      , {}, 1.1       }
  local margin   = {      2e-15, {},   1e-15, 0,   1e-15, {},     2e-15 }
  assertAllAlmostEquals (actual, expected, margin)
end

function TestLuaUnitExt:testAllEqualKeyNumbers()
  local actual   = { x=-1.1*2, y=0*2, z=1.1*2 }
  local expected = { x=-2.2  , y=0  , z=2.2   }
  assertAllAlmostEquals (actual, expected)
end

function TestLuaUnitExt:testAllEqualKeyNumbers2()
  local actual   = { a=-1.1-1e-15, b=-1.1, c=0-1e-15, d=0, e=0+1e-15, f=1.1, g=1.1+1e-15 }
  local expected = { a=-1.1      , b=-1.1, c=0      , d=0, e=0      , f=1.1, g=1.1       }
  local margin   = { a=     2e-15, b=   0, c=  1e-15, d=0, e=  1e-15, f=  0, g=    2e-15 }
  assertAllAlmostEquals (actual, expected, margin)
end

function TestLuaUnitExt:testAllEqualKeyMixed()
  local actual   = { a=-1.1-1e-15, b={}, c=0-1e-15, d=0, e=0+1e-15, f={}, g=1.1+1e-15 }
  local expected = { a=-1.1      , b={}, c=0      , d=0, e=0      , f={}, g=1.1       }
  local margin   = { a=     2e-15, b={}, c=  1e-15, d=0, e=  1e-15, f={}, g=    2e-15 }
  assertAllAlmostEquals (actual, expected, margin)
end

function TestLuaUnitExt:testAllEqualMixedMixed()
  local actual   = { a=-1.1-1e-15, b={}, 0-1e-15, d=0, e=0+1e-15, {}, g=1.1+1e-15 }
  local expected = { a=-1.1      , b={}, 0      , d=0, e=0      , {}, g=1.1       }
  local margin   = { a=     2e-15, b={},   1e-15, d=0, e=  1e-15, {}, g=    2e-15 }
  assertAllAlmostEquals (actual, expected, margin)
end

-- end ------------------------------------------------------------------------o

-- run as a standalone test
if not MAD then
  -- UJIT: Amended in favor of TAP-based stand-alone run.
  -- os.exit( utest.LuaUnit.run() )
  local runner = require('luaunit').LuaUnit.new()
  runner:setOutputType("tap")
  runner:runSuite()
end
