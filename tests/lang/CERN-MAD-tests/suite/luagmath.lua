--[=[
 o-----------------------------------------------------------------------------o
 |
 | Gmath (pure Lua) regression tests
 |
 | Methodical Accelerator Design - Copyright CERN 2016+
 | Support: http://cern.ch/mad  - mad at cern.ch
 | Authors: L. Deniau, laurent.deniau at cern.ch
 | Contrib: A.Z. Teska, aleksandra.teska at cern.ch
 |
 o-----------------------------------------------------------------------------o
 | You can redistribute this file and/or modify it under the terms of the GNU
 | General Public License GPLv3 (or later), as published by the Free Software
 | Foundation. This file is distributed in the hope that it will be useful, but
 | WITHOUT ANY WARRANTY OF ANY KIND. See http://gnu.org/licenses for details.
 o-----------------------------------------------------------------------------o

  Purpose:
  - Provide regression test suites for the gmath module without MAD extensions.

 o-----------------------------------------------------------------------------o
]=]

-- expected from other modules ------------------------------------------------o

local
  abs, acos, asin, atan, ceil, cos, cosh, deg, exp, floor, log, log10, max,
  min, rad, sin, sinh, sqrt, tan, tanh,                   -- (generic functions)
  atan2, frexp, ldexp, modf, random, randomseed,       -- (functions wo generic)
  fmod, pow,                                         -- (operators as functions)
  huge, pi =                                                      -- (constants)
  math.abs,   math.acos,  math.asin,  math.atan, math.ceil,   math.cos, math.cosh,
  math.deg,   math.exp,   math.floor, math.log,  math.log10,  math.max,
  math.min,   math.rad,   math.sin,   math.sinh, math.sqrt,   math.tan, math.tanh,
  math.atan2, math.frexp, math.ldexp, math.modf, math.random, math.randomseed,
  math.fmod,  math.pow,
  math.huge,  math.pi

local function is_number(a) return type(a) == "number" end
local function is_nan(a)    return type(a) == 'number' and a ~= a end
local function first (a,b) return a end
local function second(a,b) return b end

local eps  = 2.2204460492503130e-16
local huge = 1.7976931348623158e+308
local tiny = 2.2250738585072012e-308

local inf  = 1/0
local Inf  = 1/0
local nan  = 0/0
local NaN  = 0/0
local pi   = pi
local Pi   = pi

-- generic binary functions
local angle = function (x,y) return is_number(x) and atan2(y,x) or x:angle(y) end

-- generic variadic functions
local max = function (x,...) return is_number(x) and max(x,...) or x:max(...) end
local min = function (x,...) return is_number(x) and min(x,...) or x:min(...) end

-- extra generic functions
local sign  = function (x) return is_number(x) and (x>=0 and 1 or x<0 and -1 or x)  or x:sign()end
local step  = function (x) return is_number(x) and (x>=0 and 1 or x<0 and  0 or x)  or x:step()end
local sinc  = function (x) return is_number(x) and (abs(x)<1e-10 and 1 or sin(x)/x) or x:sinc()end
local frac  = function (x) return is_number(x) and second(modf(x)) or x:frac()                 end
local trunc = function (x) return is_number(x) and first (modf(x)) or x:trunc()                end
local round = function (x) return is_number(x) and (x>0 and floor(x+0.5) or x<0 and ceil(x-0.5) or x) or x:round() end

-- operators
local unm = function (x  ) return -x     end
local add = function (x,y) return  x + y end
local sub = function (x,y) return  x - y end
local mul = function (x,y) return  x * y end
local div = function (x,y) return  x / y end
local mod = function (x,y) return  x % y end
local pow = function (x,y) return  x ^ y end

-- logical
local eq = function (x,y) return x == y end
local ne = function (x,y) return x ~= y end
local lt = function (x,y) return x <  y end
local le = function (x,y) return x <= y end
local gt = function (x,y) return x >  y end
local ge = function (x,y) return x >= y end

-- complex generic functions
local carg  = function (x) return is_number(x) and (x>=0 and 0 or x<0 and pi or x) or x:carg() end
local real  = function (x) return is_number(x) and x                               or x:real() end
local imag  = function (x) return is_number(x) and 0                               or x:imag() end
local conj  = function (x) return is_number(x) and x                               or x:conj() end
local rect  = function (x) return is_number(x) and abs(x) or x:rect()  end
local polar = function (x) return is_number(x) and abs(x) or x:polar() end -- TODO +M.carg(x)*1i

-- generic unary functions
local abs   = function (x) return is_number(x) and abs  (x)  or x:abs  () end
local acos  = function (x) return is_number(x) and acos (x)  or x:acos () end
local asin  = function (x) return is_number(x) and asin (x)  or x:asin () end
local atan  = function (x) return is_number(x) and atan (x)  or x:atan () end
local ceil  = function (x) return is_number(x) and ceil (x)  or x:ceil () end
local cos   = function (x) return is_number(x) and cos  (x)  or x:cos  () end
local cosh  = function (x) return is_number(x) and cosh (x)  or x:cosh () end
local deg   = function (x) return is_number(x) and deg  (x)  or x:deg  () end
local exp   = function (x) return is_number(x) and exp  (x)  or x:exp  () end
local floor = function (x) return is_number(x) and floor(x)  or x:floor() end
local log   = function (x) return is_number(x) and log  (x)  or x:log  () end
local log10 = function (x) return is_number(x) and log10(x)  or x:log10() end
local rad   = function (x) return is_number(x) and rad  (x)  or x:rad  () end
local sin   = function (x) return is_number(x) and sin  (x)  or x:sin  () end
local sinh  = function (x) return is_number(x) and sinh (x)  or x:sinh () end
local sqrt  = function (x) return is_number(x) and sqrt (x)  or x:sqrt () end
local tan   = function (x) return is_number(x) and tan  (x)  or x:tan  () end
local tanh  = function (x) return is_number(x) and tanh (x)  or x:tanh () end

-- locals ---------------------------------------------------------------------o

local utest = MAD and MAD.utest or require("luaunit")

local assertFalse        = utest.assertFalse
local assertTrue         = utest.assertTrue
local assertEquals       = utest.assertEquals
local assertNotEquals    = utest.assertNotEquals
local assertAlmostEquals = utest.assertAlmostEquals
local assertNaN          = utest.assertNaN

-- regression test suite ------------------------------------------------------o

TestLuaGmath = {}

local values = {
  num  = {0, tiny, 2^-64, 2^-63, 2^-53, eps, 2^-52, 2*eps, 2^-32, 2^-31, 1e-9,
          0.1-eps, 0.1, 0.1+eps, 0.5, 0.7-eps, 0.7, 0.7+eps, 1-eps, 1, 1+eps,
          1.1, 1.7, 2, 10, 1e2, 1e3, 1e6, 1e9, 2^31, 2^32, 2^52, 2^53,
          2^63, 2^64, huge, inf},
  rad  = {0, eps, 2*eps, pi/180, pi/90, pi/36, pi/18, pi/12, pi/6, pi/4, pi/3, pi/2,
          pi-pi/3, pi-pi/4, pi-pi/6, pi-pi/12, pi},
  rad2 = {0, eps, 2*eps, pi/180, pi/90, pi/36, pi/18, pi/12, pi/6, pi/4, pi/3, pi/2},

  deg  = {0, eps, 2*eps, 1, 2, 5, 10, 15, 30, 45, 60, 90,
          120, 135, 150, 165, 180},
  deg2 = {0, eps, 2*eps, 1, 2, 5, 10, 15, 30, 45, 60, 90},
}

-- keep the order of the import above

-- constant

function TestLuaGmath:testConstant()
  assertEquals(  pi , 3.1415926535897932385 )
  assertEquals(  pi , atan(1)*4 )

  assertEquals(  pi ,  Pi )
  assertEquals( -pi , -Pi )

  assertNotEquals(  tiny,  2.2250738585072011e-308 )
  assertEquals   (  tiny,  2.2250738585072012e-308 ) -- reference
  assertEquals   (  tiny,  2.2250738585072013e-308 )
  assertEquals   (  tiny,  2.2250738585072014e-308 )
  assertEquals   (  tiny,  2.2250738585072015e-308 )
  assertEquals   (  tiny,  2.2250738585072016e-308 )
  assertNotEquals(  tiny,  2.2250738585072017e-308 )

  assertNotEquals( -tiny, -2.2250738585072011e-308 )
  assertEquals   ( -tiny, -2.2250738585072012e-308 )
  assertEquals   ( -tiny, -2.2250738585072013e-308 )
  assertEquals   ( -tiny, -2.2250738585072014e-308 )
  assertEquals   ( -tiny, -2.2250738585072015e-308 )
  assertEquals   ( -tiny, -2.2250738585072016e-308 )
  assertNotEquals( -tiny, -2.2250738585072017e-308 )

  assertNotEquals(  huge,  1.7976931348623156e+308 )
  assertEquals   (  huge,  1.7976931348623157e+308 )
  assertEquals   (  huge,  1.7976931348623158e+308 ) -- reference
  assertNotEquals(  huge,  1.7976931348623159e+308 )

  assertNotEquals( -huge, -1.7976931348623156e+308 )
  assertEquals   ( -huge, -1.7976931348623157e+308 )
  assertEquals   ( -huge, -1.7976931348623158e+308 )
  assertNotEquals( -huge, -1.7976931348623159e+308 )

  assertNotEquals(  eps ,  2.2204460492503129e-16  )
  assertEquals   (  eps ,  2.2204460492503130e-16  ) -- reference
  assertEquals   (  eps ,  2.2204460492503131e-16  )
  assertEquals   (  eps ,  2.2204460492503132e-16  )
  assertEquals   (  eps ,  2.2204460492503133e-16  )
  assertNotEquals(  eps ,  2.2204460492503134e-16  )

  assertNotEquals( -eps , -2.2204460492503129e-16  )
  assertEquals   ( -eps , -2.2204460492503130e-16  )
  assertEquals   ( -eps , -2.2204460492503131e-16  )
  assertEquals   ( -eps , -2.2204460492503132e-16  )
  assertEquals   ( -eps , -2.2204460492503133e-16  )
  assertNotEquals( -eps , -2.2204460492503134e-16  )

  assertEquals   (  inf,  Inf )
  assertEquals   ( -inf, -Inf )

  assertNotEquals(  nan,  nan )
  assertNotEquals( -nan,  nan )
  assertNotEquals(  nan, -nan )
  assertNotEquals( -nan, -nan )

  assertNotEquals(  nan,  NaN )
  assertNotEquals( -nan,  NaN )
  assertNotEquals(  nan, -NaN )
  assertNotEquals( -nan, -NaN )

  assertFalse( is_nan( inf)  )
  assertFalse( is_nan( Inf)  )
  assertFalse( is_nan(-inf)  )
  assertFalse( is_nan(-Inf)  )
  assertTrue ( is_nan( nan)  )
  assertTrue ( is_nan( NaN)  )
  assertTrue ( is_nan(-nan)  )
  assertTrue ( is_nan(-NaN)  )
  assertFalse( is_nan('nan') )
  assertFalse( is_nan('NaN') )

  assertNaN  ( nan)
  assertNaN  (-nan)
  assertNaN  ( NaN)
  assertNaN  (-NaN)
end

-- generic functions

function TestLuaGmath:testAbs()
  for _,v in ipairs(values.num) do
    assertEquals(  abs( v),  v )
    assertEquals(  abs(-v),  v )
    assertEquals( -abs(-v), -v )
  end

  assertEquals( abs( tiny) ,  tiny )
  assertEquals( abs(  0.1) ,   0.1 )
  assertEquals( abs(    1) ,     1 )
  assertEquals( abs( huge) ,  huge )
  assertEquals( abs(-tiny) ,  tiny )
  assertEquals( abs(- 0.1) ,   0.1 )
  assertEquals( abs(-   1) ,     1 )
  assertEquals( abs(-huge) ,  huge )
  assertNaN   ( abs(  nan) )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/abs(- 0 ), inf ) -- check for +0
  assertEquals( 1/abs(  0 ), inf ) -- check for +0
  assertEquals(   abs(-inf), inf )
  assertEquals(   abs( inf), inf )

  assertNaN( abs(nan) )
end

function TestLuaGmath:testAcos()
  for _,v in ipairs(values.rad) do
    assertAlmostEquals( acos(v/pi)    - (pi-acos(-v/pi)) , 0,  2*eps )
    assertAlmostEquals( acos(v/pi)    - (pi/2-asin(v/pi)), 0,  2*eps )
    assertAlmostEquals( acos(cos( v)) - v                , 0, 10*eps ) -- 8@1deg
    assertAlmostEquals( acos(cos(-v)) - v                , 0, 10*eps ) -- 8@1deg
  end
  local r4, r6, r12 = sqrt(2)/2, sqrt(3)/2, sqrt(2)*(sqrt(3)+1)/4
  assertEquals      ( acos(-1  ) -    pi   , 0        )
  assertAlmostEquals( acos(-r12) - 11*pi/12, 0, 2*eps )
  assertAlmostEquals( acos(-r6 ) -  5*pi/6 , 0,   eps )
  assertAlmostEquals( acos(-r4 ) -  3*pi/4 , 0,   eps )
  assertAlmostEquals( acos(-0.5) -  2*pi/3 , 0, 2*eps )
  assertAlmostEquals( acos( 0  ) -    pi/2 , 0,   eps )
  assertAlmostEquals( acos( 0.5) -    pi/3 , 0,   eps )
  assertAlmostEquals( acos( r4 ) -    pi/4 , 0,   eps )
  assertAlmostEquals( acos( r6 ) -    pi/6 , 0,   eps )
  assertAlmostEquals( acos( r12) -    pi/12, 0,   eps )
  assertAlmostEquals( acos( r12) -    pi/12, 0,   eps )
  assertAlmostEquals( acos( r6 ) -    pi/6 , 0,   eps )
  assertAlmostEquals( acos( r4 ) -    pi/4 , 0,   eps )
  assertAlmostEquals( acos( 0.5) -    pi/3 , 0,   eps )
  assertAlmostEquals( acos( 0  ) -    pi/2 , 0,   eps )
  assertAlmostEquals( acos(-0.5) -  2*pi/3 , 0, 2*eps )
  assertAlmostEquals( acos(-r4 ) -  3*pi/4 , 0,   eps )
  assertAlmostEquals( acos(-r6 ) -  5*pi/6 , 0,   eps )
  assertAlmostEquals( acos(-r12) - 11*pi/12, 0, 2*eps )
  assertEquals      ( acos(-1  ) -    pi   , 0        )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( acos( 1    ) ,   0   )
  assertNaN   ( acos(-1-eps) )
  assertNaN   ( acos( 1+eps) )
  assertNaN   ( acos(nan   ) )
end

function TestLuaGmath:testAsin()
  for _,v in ipairs(values.rad2) do
    assertAlmostEquals( asin(v/pi)    - -asin(-v/pi)     , 0, eps )
    assertAlmostEquals( asin(v/pi)    - (pi/2-acos(v/pi)), 0, eps )
    assertAlmostEquals( asin(sin( v)) -  v               , 0, eps )
    assertAlmostEquals( asin(sin(-v)) - -v               , 0, eps )
  end
  local r3, r4, r12 = sqrt(3)/2, sqrt(2)/2, sqrt(2)*(sqrt(3)-1)/4
  assertEquals      ( asin( r12) -  pi/12, 0      )
  assertAlmostEquals( asin( 0.5) -  pi/6 , 0, eps )
  assertAlmostEquals( asin( r4 ) -  pi/4 , 0, eps )
  assertEquals      ( asin( r3 ) -  pi/3 , 0      )
  assertEquals      ( asin( 1  ) -  pi/2 , 0      )
  assertEquals      ( asin( r3 ) -  pi/3 , 0      )
  assertAlmostEquals( asin( r4 ) -  pi/4 , 0, eps )
  assertAlmostEquals( asin( 0.5) -  pi/6 , 0, eps )
  assertEquals      ( asin( r12) -  pi/12, 0      )
  assertEquals      ( asin(-r12) - -pi/12, 0      )
  assertAlmostEquals( asin(-0.5) - -pi/6 , 0, eps )
  assertAlmostEquals( asin(-r4 ) - -pi/4 , 0, eps )
  assertEquals      ( asin(-r3 ) - -pi/3 , 0      )
  assertEquals      ( asin(-1  ) - -pi/2 , 0      )
  assertEquals      ( asin(-r3 ) - -pi/3 , 0      )
  assertAlmostEquals( asin(-r4 ) - -pi/4 , 0, eps )
  assertAlmostEquals( asin(-0.5) - -pi/6 , 0, eps )
  assertEquals      ( asin(-r12) - -pi/12, 0      )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals(        1/asin( 0    ) ,  inf  ) -- check for +0
  assertEquals(        1/asin(-0    ) , -inf  ) -- check for -0
  assertNaN( asin(-1-eps) )
  assertNaN( asin( 1+eps) )
  assertNaN( asin(nan   ) )
end

function TestLuaGmath:testAtan()
  for _,v in ipairs(values.num) do
    assertAlmostEquals( atan(v) - -atan(-v), 0, eps) -- randomly not equal ±eps
  end
  for _,v in ipairs(values.rad2) do
    assertAlmostEquals( atan(tan( v)) -  v, 0, eps )
    assertAlmostEquals( atan(tan(-v)) - -v, 0, eps )
  end
  local r3, r6, r12 = sqrt(3), 1/sqrt(3), 2-sqrt(3)
  assertAlmostEquals( atan(-r3 ) - -pi/3 , 0, eps )
  assertEquals      ( atan(-1  ) - -pi/4 , 0      )
  assertAlmostEquals( atan(-r6 ) - -pi/6 , 0, eps )
  assertAlmostEquals( atan(-r12) - -pi/12, 0, eps )
  assertAlmostEquals( atan( r12) -  pi/12, 0, eps )
  assertAlmostEquals( atan( r6 ) -  pi/6 , 0, eps )
  assertEquals      ( atan( 1  ) -  pi/4 , 0      )
  assertAlmostEquals( atan( r3 ) -  pi/3 , 0, eps )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/atan( 0  ) ,  inf  ) -- check for -0
  assertEquals( 1/atan(-0  ) , -inf  ) -- check for +0
  assertEquals(   atan(-inf) , -pi/2 )
  assertEquals(   atan( inf) ,  pi/2 )
  assertNaN   (   atan( nan) )
end

function TestLuaGmath:testCeil()
  assertEquals( ceil( tiny) ,     1 )
  assertEquals( ceil(  0.1) ,     1 )
  assertEquals( ceil(  0.5) ,     1 )
  assertEquals( ceil(  0.7) ,     1 )
  assertEquals( ceil(    1) ,     1 )
  assertEquals( ceil(  1.1) ,     2 )
  assertEquals( ceil(  1.5) ,     2 )
  assertEquals( ceil(  1.7) ,     2 )
  assertEquals( ceil( huge) ,  huge )
  assertEquals( ceil(-tiny) , -   0 )
  assertEquals( ceil(- 0.1) , -   0 )
  assertEquals( ceil(- 0.5) , -   0 )
  assertEquals( ceil(- 0.7) , -   0 )
  assertEquals( ceil(-   1) , -   1 )
  assertEquals( ceil(- 1.1) , -   1 )
  assertEquals( ceil(- 1.5) , -   1 )
  assertEquals( ceil(- 1.7) , -   1 )
  assertEquals( ceil(-huge) , -huge )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/ceil(-  0 ) , -inf ) -- check for -0
  assertEquals( 1/ceil(   0 ) ,  inf ) -- check for +0
  assertEquals(   ceil(- inf) , -inf )
  assertEquals(   ceil(  inf) ,  inf )

  assertNaN( ceil(nan) )
end

function TestLuaGmath:testCos()
  for _,v in ipairs(values.rad) do
    assertAlmostEquals( cos(v)           - cos(-v)         , 0, eps )
    assertAlmostEquals( cos(v)           - sin(pi/2-v)     , 0, eps )
    assertAlmostEquals( cos(v)           - (1-2*sin(v/2)^2), 0, eps )
    assertAlmostEquals( cos(acos( v/pi)) -  v/pi           , 0, eps )
    assertAlmostEquals( cos(acos(-v/pi)) - -v/pi           , 0, eps )
  end
  local r4, r6, r12 = sqrt(2)/2, sqrt(3)/2, sqrt(2)*(sqrt(3)+1)/4
  assertEquals      ( cos(    pi   ) - -1  , 0      )
  assertAlmostEquals( cos( 11*pi/12) - -r12, 0, eps )
  assertAlmostEquals( cos(  5*pi/6 ) - -r6 , 0, eps )
  assertAlmostEquals( cos(  3*pi/4 ) - -r4 , 0, eps )
  assertAlmostEquals( cos(  2*pi/3 ) - -0.5, 0, eps )
  assertAlmostEquals( cos(    pi/2 ) -  0  , 0, eps )
  assertAlmostEquals( cos(    pi/3 ) -  0.5, 0, eps )
  assertAlmostEquals( cos(    pi/4 ) -  r4 , 0, eps )
  assertAlmostEquals( cos(    pi/6 ) -  r6 , 0, eps )
  assertAlmostEquals( cos(    pi/12) -  r12, 0, eps )
  assertAlmostEquals( cos(-   pi/12) -  r12, 0, eps )
  assertAlmostEquals( cos(-   pi/6 ) -  r6 , 0, eps )
  assertAlmostEquals( cos(-   pi/4 ) -  r4 , 0, eps )
  assertAlmostEquals( cos(-   pi/3 ) -  0.5, 0, eps )
  assertAlmostEquals( cos(-   pi/2 ) -  0  , 0, eps )
  assertAlmostEquals( cos(- 2*pi/3 ) - -0.5, 0, eps )
  assertAlmostEquals( cos(- 3*pi/4 ) - -r4 , 0, eps )
  assertAlmostEquals( cos(- 5*pi/6 ) - -r6 , 0, eps )
  assertAlmostEquals( cos(-11*pi/12) - -r12, 0, eps )
  assertEquals      ( cos(-   pi   ) - -1  , 0      )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( cos( -0 ) ,   1   )
  assertEquals( cos(  0 ) ,   1   )
  assertNaN   ( cos(-inf) )
  assertNaN   ( cos( inf) )
  assertNaN   ( cos( nan) )
end

function TestLuaGmath:testCosh()
  for _,v in ipairs(values.num) do
    assertEquals( cosh(v), cosh(-v) )
    if cosh(v) <= huge then
      assertAlmostEquals( cosh(v) / ((exp(v)+exp(-v))/2), 1,   eps )
      assertAlmostEquals( cosh(v) / ( 1 + 2*sinh(v/2)^2), 1,   eps )
      assertAlmostEquals( cosh(v) / (-1 + 2*cosh(v/2)^2), 1, 2*eps )
      assertAlmostEquals(  exp(v) / (cosh(v) + sinh(v)) , 1,   eps )
    end
  end
  assertEquals( cosh(-711), inf )
  assertEquals( cosh( 711), inf )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( cosh(-0  ), 1   )
  assertEquals( cosh( 0  ), 1   )
  assertEquals( cosh(-inf), inf )
  assertEquals( cosh( inf), inf )

  assertNaN( cosh(nan) )
end

function TestLuaGmath:testDeg()
  local r = 57.2957795130823208768 -- 180/pi
  for _,v in ipairs(values.rad) do
    assertEquals( deg( v) ,  v*r )
    assertEquals( deg(-v) , -v*r )
  end
  assertEquals      ( deg(-inf    )       , -inf      )
  assertEquals      ( deg(-2*pi   ) - -360, 0         )
  assertEquals      ( deg(-  pi   ) - -180, 0         )
  assertEquals      ( deg(-  pi/2 ) - - 90, 0         )
  assertAlmostEquals( deg(-  pi/3 ) - - 60, 0, 32*eps )
  assertEquals      ( deg(-  pi/4 ) - - 45, 0         )
  assertAlmostEquals( deg(-  pi/6 ) - - 30, 0, 16*eps )
  assertAlmostEquals( deg(-  pi/12) - - 15, 0,  8*eps )
  assertEquals      ( deg(-  pi/18) - - 10, 0         )
  assertEquals      ( deg(   0    ) -    0, 0         )
  assertEquals      ( deg(   pi/18) -   10, 0         )
  assertAlmostEquals( deg(   pi/12) -   15, 0,  8*eps )
  assertAlmostEquals( deg(   pi/6 ) -   30, 0, 16*eps )
  assertEquals      ( deg(   pi/4 ) -   45, 0         )
  assertAlmostEquals( deg(   pi/3 ) -   60, 0, 32*eps )
  assertEquals      ( deg(   pi/2 ) -   90, 0         )
  assertEquals      ( deg(   pi   ) -  180, 0         )
  assertEquals      ( deg( 2*pi   ) -  360, 0         )
  assertEquals      ( deg( inf    )       ,  inf      )

  assertAlmostEquals( deg(-  pi/3 ) / - 60, 1, eps )
  assertAlmostEquals( deg(-  pi/6 ) / - 30, 1, eps )
  assertAlmostEquals( deg(   pi/6 ) /   30, 1, eps )
  assertAlmostEquals( deg(   pi/3 ) /   60, 1, eps )

  assertNaN( deg(nan) )
end

function TestLuaGmath:testExp()
  -- SetPrecision[Table[Exp[x],{x, -1, 1, 0.1}],20]
  local val1 = {0.36787944117144233402, 0.40656965974059910973,
  0.44932896411722156316, 0.49658530379140952693, 0.54881163609402638937,
  0.60653065971263342426, 0.67032004603563932754, 0.74081822068171787610,
  0.81873075307798193201, 0.90483741803595962860, 1, 1.1051709180756477124,
  1.2214027581601698547, 1.3498588075760031835, 1.4918246976412703475,
  1.6487212707001281942, 1.8221188003905091080, 2.0137527074704766328,
  2.2255409284924678737, 2.4596031111569498506, 2.7182818284590450908}
  -- SetPrecision[Table[Exp[x],{x, -10, -1, 1}],20]
  local val2 = {0.00004539992976248485154, 0.0001234098040866795495,
  0.0003354626279025118388, 0.0009118819655545162080, 0.002478752176666358423,
  0.006737946999085467097, 0.018315638888734180294, 0.04978706836786394298,
  0.13533528323661269189, 0.36787944117144232160}
  -- SetPrecision[Table[Exp[x],{x, 1, 10, 1}],20]
  local val3 = {2.7182818284590452354, 7.389056098930650227,
  20.085536923187667741, 54.59815003314423908, 148.41315910257660342,
  403.4287934927351226, 1096.6331584284585993, 2980.957987041728275,
  8103.083927575384008, 22026.46579480671652}

  local i
  i=0 for v=-1,1,0.1 do i=i+1 -- should be done with ranges...
    v = -1+(i-1)*0.1
    assertAlmostEquals( exp(v) - val1[i], 0, 2*eps )
  end
  i=0 for v=-10,-1 do i=i+1
    assertAlmostEquals( exp(v) - val2[i], 0, eps )
  end
  i=0 for v=1,10 do i=i+1
    assertAlmostEquals( exp(v) - val3[i], 0, eps )
  end

  for i,v in ipairs(values.num) do
    if v > 1/709.78 and v < 709.78 then
      assertAlmostEquals( exp(v+1/v) / (exp(v)*exp(1/v)) - 1, 0, 25*eps )
      assertAlmostEquals( exp(log(v)) / v - 1, 0, 2*eps )
      assertAlmostEquals( log(exp(v)) / v - 1, 0, 4*eps )
    end
  end

  assertEquals      ( exp(-inf) , 0   )
  assertEquals      ( exp(-  1) , 0.36787944117144232159 )
  assertEquals      ( exp(-0.5) , 0.60653065971263342360 )
  assertAlmostEquals( exp(-0.1) - 0.90483741803595957316, 0, eps )
  assertEquals      ( exp(   0) , 1   )
  assertEquals      ( exp( 0.1) , 1.10517091807564762481 )
  assertEquals      ( exp( 0.5) , 1.64872127070012814684 )
  assertEquals      ( exp(   1) , 2.71828182845904523536 )
  assertEquals      ( exp( inf) , inf )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( exp(-0  ),  1  )
  assertEquals( exp( 0  ),  1  )
  assertEquals( exp(-inf),  0  )
  assertEquals( exp( inf), inf )

  assertNaN( exp(nan) )
end

function TestLuaGmath:testFloor()
  assertEquals( floor( tiny) ,     0 )
  assertEquals( floor(  0.1) ,     0 )
  assertEquals( floor(  0.5) ,     0 )
  assertEquals( floor(  0.7) ,     0 )
  assertEquals( floor(    1) ,     1 )
  assertEquals( floor(  1.1) ,     1 )
  assertEquals( floor(  1.5) ,     1 )
  assertEquals( floor(  1.7) ,     1 )
  assertEquals( floor( huge) ,  huge )
  assertEquals( floor(-tiny) , -   1 )
  assertEquals( floor(- 0.1) , -   1 )
  assertEquals( floor(- 0.5) , -   1 )
  assertEquals( floor(- 0.7) , -   1 )
  assertEquals( floor(-   1) , -   1 )
  assertEquals( floor(- 1.1) , -   2 )
  assertEquals( floor(- 1.5) , -   2 )
  assertEquals( floor(- 1.7) , -   2 )
  assertEquals( floor(-huge) , -huge )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/floor(-  0 ) , -inf ) -- check for -0
  assertEquals( 1/floor(   0 ) ,  inf ) -- check for +0
  assertEquals(   floor(- inf) , -inf )
  assertEquals(   floor(  inf) ,  inf )

  assertNaN( floor(nan) )
end

function TestLuaGmath:testLog()
  -- also used/tested in testExp
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > eps and y > eps  and x < 1/eps and y < 1/eps then
      assertAlmostEquals(log(x*y) - (log(x)+log(y)), 0, 40*eps)
    end
  end end
  for i=0,200 do
    assertAlmostEquals( log(2^ i) -  i*log(2), 0, 150*eps)
    assertAlmostEquals( log(2^-i) - -i*log(2), 0, 150*eps)
  end

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( log(-0) , -inf )
  assertEquals( log( 0) , -inf )
  assertEquals( log(1)  ,  0   )
  assertEquals( log(inf),  inf )

  assertNaN( log(-tiny) )
  assertNaN( log(-inf ) )
  assertNaN( log( nan ) )
end

function TestLuaGmath:testLog10()
  for i,v in ipairs(values.num) do
    if v > 0 and v < inf then
      assertAlmostEquals( log10(v) - log(v)/log(10), 0, 300*eps)
    end
  end
  for i=0,200 do
    assertAlmostEquals( log10(10^ i) -  i, 0, 150*eps)
    assertAlmostEquals( log10(10^-i) - -i, 0, 150*eps)
  end
  assertEquals( log10( 10), 1   )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( log10(- 0),-inf )
  assertEquals( log10(  0),-inf )
  assertEquals( log10(  1),  0  )
  assertEquals( log10(inf), inf )

  assertNaN( log10(-tiny) )
  assertNaN( log10(-inf ) )
  assertNaN( log10( nan ) )
end

function TestLuaGmath:testMax()
  assertEquals( max(table.unpack(values.num )), inf  )
  assertEquals( max(table.unpack(values.rad )),  pi  )
  assertEquals( max(table.unpack(values.deg )), 180  )
  assertEquals( max(table.unpack(values.rad2)),  pi/2)
  assertEquals( max(table.unpack(values.deg2)),  90  )
  local t1, t2, t3, t4, t5 = {}, {}, {}, {}, {}
  for i,v in ipairs(values.num ) do t1[i] = -v end
  for i,v in ipairs(values.rad ) do t2[i] = -v end
  for i,v in ipairs(values.deg ) do t3[i] = -v end
  for i,v in ipairs(values.rad2) do t4[i] = -v end
  for i,v in ipairs(values.deg2) do t5[i] = -v end
  assertEquals( max(table.unpack(t1)), 0 )
  assertEquals( max(table.unpack(t2)), 0 )
  assertEquals( max(table.unpack(t3)), 0 )
  assertEquals( max(table.unpack(t4)), 0 )
  assertEquals( max(table.unpack(t5)), 0 )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( max(nan, -inf), -inf )
  assertEquals( max(nan,   0 ),   0  )
--  assertEquals( max(0  ,  nan),   0  )
  assertEquals( max(nan,  inf),  inf )
  assertEquals( max(nan, 0, 1),   1  )
  assertEquals( max(nan,nan,0),   0  )
--  assertEquals( max(nan,0,nan),   0  )
--  assertEquals( max(0,nan,nan),   0  )
  assertNaN   ( max(nan)         )
  assertNaN   ( max(nan,nan)     )
  assertNaN   ( max(nan,nan,nan) )
end

function TestLuaGmath:testMin()
  assertEquals( min(table.unpack(values.num )), 0 )
  assertEquals( min(table.unpack(values.rad )), 0 )
  assertEquals( min(table.unpack(values.deg )), 0 )
  assertEquals( min(table.unpack(values.rad2)), 0 )
  assertEquals( min(table.unpack(values.deg2)), 0 )
  local t1, t2, t3, t4, t5 = {}, {}, {}, {}, {}
  for i,v in ipairs(values.num ) do t1[i] = -v end
  for i,v in ipairs(values.rad ) do t2[i] = -v end
  for i,v in ipairs(values.deg ) do t3[i] = -v end
  for i,v in ipairs(values.rad2) do t4[i] = -v end
  for i,v in ipairs(values.deg2) do t5[i] = -v end
  assertEquals( min(table.unpack(t1)), -inf  )
  assertEquals( min(table.unpack(t2)), - pi  )
  assertEquals( min(table.unpack(t3)), -180  )
  assertEquals( min(table.unpack(t4)), - pi/2)
  assertEquals( min(table.unpack(t5)), - 90  )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( min(nan, -inf), -inf )
  assertEquals( min(nan,   0 ),   0  )
--  assertEquals( min(0  ,  nan),   0  )
  assertEquals( min(nan,  inf),  inf )
  assertEquals( min(nan, 0,-1), - 1  )
  assertEquals( min(nan,nan,0),   0  )
--  assertEquals( min(nan,0,nan),   0  )
--  assertEquals( min(0,nan,nan),   0  )
  assertNaN   ( min(nan)         )
  assertNaN   ( min(nan,nan)     )
  assertNaN   ( min(nan,nan,nan) )
end

function TestLuaGmath:testModf()
  local s=function(n,f) return n+f end

  for _,v in ipairs(values.num) do
    if v == inf then break end
    assertEquals( s(modf( v+eps)),  v+eps )
    assertEquals( s(modf( v-eps)),  v-eps )
    assertEquals( s(modf(-v+eps)), -v+eps )
    assertEquals( s(modf(-v-eps)), -v-eps )
    assertEquals( s(modf( v+0.1)),  v+0.1 )
    assertEquals( s(modf( v-0.1)),  v-0.1 )
    assertEquals( s(modf(-v+0.1)), -v+0.1 )
    assertEquals( s(modf(-v-0.1)), -v-0.1 )
    assertEquals( s(modf( v+0.7)),  v+0.7 )
    assertEquals( s(modf( v-0.7)),  v-0.7 )
    assertEquals( s(modf(-v+0.7)), -v+0.7 )
    assertEquals( s(modf(-v-0.7)), -v-0.7 )
  end
  assertEquals( {modf(    0)} , {    0,     0} )
  assertEquals( {modf( tiny)} , {    0,  tiny} )
  assertEquals( {modf(  0.1)} , {    0,   0.1} )
  assertEquals( {modf(  0.5)} , {    0,   0.5} )
  assertEquals( {modf(  0.7)} , {    0,   0.7} )
  assertEquals( {modf(    1)} , {    1,     0} )
  assertEquals( {modf(  1.5)} , {    1,   0.5} )
  assertEquals( {modf(  1.7)} , {    1,   0.7} )
  assertEquals( {modf( huge)} , { huge,     0} )
  assertEquals( {modf(  inf)} , {  inf,     0} )
  assertEquals( {modf(-   0)} , {    0, -   0} )
  assertEquals( {modf(-tiny)} , {    0, -tiny} )
  assertEquals( {modf(- 0.1)} , {-   0, - 0.1} )
  assertEquals( {modf(- 0.5)} , {-   0, - 0.5} )
  assertEquals( {modf(- 0.7)} , {-   0, - 0.7} )
  assertEquals( {modf(-   1)} , {-   1, -   0} )
  assertEquals( {modf(- 1.5)} , {-   1, - 0.5} )
  assertEquals( {modf(- 1.7)} , {-   1, - 0.7} )
  assertEquals( {modf(-huge)} , {-huge, -   0} )
  assertEquals( {modf(- inf)} , {- inf, -   0} )

  local n,f
  n,f=modf( 1.1) assertEquals(n,  1) assertAlmostEquals( f-0.1, 0, eps/2 )
  n,f=modf(-1.1) assertEquals(n, -1) assertAlmostEquals( f+0.1, 0, eps/2 )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals(    first(modf(-huge)), -huge )
  assertEquals(    first(modf( huge)),  huge )
  assertEquals( 1/second(modf(-huge)), -inf  ) -- check for -0
  assertEquals( 1/second(modf( huge)),  inf  ) -- check for +0
  assertEquals(    first(modf(-inf )), -inf  )
  assertEquals(    first(modf( inf )),  inf  )
  assertEquals( 1/second(modf(-inf )), -inf  ) -- check for -0
  assertEquals( 1/second(modf( inf )),  inf  ) -- check for +0

  assertNaN(  first(modf(nan)) )
  assertNaN( second(modf(nan)) )
end

function TestLuaGmath:testPow()
  local pow = math.pow

  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > 1/709.78 and y > 1/709.78 and x < 709.78 and y < 709.78 then
      assertAlmostEquals( log(pow(x,y)) - y*log(x), 0, max(abs(y*log(x)) * eps, 4*eps) )
    end
  end end

  -- Check for IEEE:IEC 60559 compliance
  assertEquals   (pow(  0 , - 11),  inf )
  assertEquals   (pow(- 0 , - 11), -inf )

  assertEquals   (pow(  0 , - .5),  inf )
  assertEquals   (pow(- 0 , - .5),  inf )
  assertEquals   (pow(  0 , -  2),  inf )
  assertEquals   (pow(- 0 , -  2),  inf )
  assertEquals   (pow(  0 , - 10),  inf )
  assertEquals   (pow(- 0 , - 10),  inf )

  assertEquals( 1/pow(  0 ,    1),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,    1), -inf ) -- check for -0
  assertEquals( 1/pow(  0 ,   11),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,   11), -inf ) -- check for -0

  assertEquals( 1/pow(  0 ,  0.5),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,  0.5),  inf ) -- check for +0
  assertEquals( 1/pow(  0 ,    2),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,    2),  inf ) -- check for +0
  assertEquals( 1/pow(  0 ,   10),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,   10),  inf ) -- check for +0
  assertEquals( 1/pow(  0 ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,  inf),  inf ) -- check for +0

  assertEquals   (pow(- 1 ,  inf),   1  )
  assertEquals   (pow(- 1 , -inf),   1  )

  assertEquals   (pow(  1 ,   0 ),   1  )
  assertEquals   (pow(  1 , - 0 ),   1  )
  assertEquals   (pow(  1 ,  0.5),   1  )
  assertEquals   (pow(  1 , -0.5),   1  )
  assertEquals   (pow(  1 ,   1 ),   1  )
  assertEquals   (pow(  1 , - 1 ),   1  )
  assertEquals   (pow(  1 ,  inf),   1  )
  assertEquals   (pow(  1 , -inf),   1  )
  assertEquals   (pow(  1 ,  nan),   1  )
  assertEquals   (pow(  1 , -nan),   1  )

  assertEquals   (pow(  0 ,   0 ),   1  )
  assertEquals   (pow(- 0 ,   0 ),   1  )
  assertEquals   (pow( 0.5,   0 ),   1  )
  assertEquals   (pow(-0.5,   0 ),   1  )
  assertEquals   (pow(  1 ,   0 ),   1  )
  assertEquals   (pow(- 1 ,   0 ),   1  )
  assertEquals   (pow( inf,   0 ),   1  )
  assertEquals   (pow(-inf,   0 ),   1  )
  assertEquals   (pow( nan,   0 ),   1  )
  assertEquals   (pow(-nan,   0 ),   1  )

  assertEquals   (pow(  0 , - 0 ),   1  )
  assertEquals   (pow(- 0 , - 0 ),   1  )
  assertEquals   (pow( 0.5, - 0 ),   1  )
  assertEquals   (pow(-0.5, - 0 ),   1  )
  assertEquals   (pow(  1 , - 0 ),   1  )
  assertEquals   (pow(- 1 , - 0 ),   1  )
  assertEquals   (pow( inf, - 0 ),   1  )
  assertEquals   (pow(-inf, - 0 ),   1  )
  assertEquals   (pow( nan, - 0 ),   1  )
  assertEquals   (pow(-nan, - 0 ),   1  )

  assertNaN( pow(- 1  , 0.5) )
  assertNaN( pow(- 1  ,-0.5) )
  assertNaN( pow(- 1  , 1.5) )
  assertNaN( pow(- 1  ,-1.5) )

  assertEquals   (pow(  0   , -inf),  inf )
  assertEquals   (pow(- 0   , -inf),  inf )
  assertEquals   (pow( 0.5  , -inf),  inf )
  assertEquals   (pow(-0.5  , -inf),  inf )
  assertEquals   (pow( 1-eps, -inf),  inf )
  assertEquals   (pow(-1+eps, -inf),  inf )

  assertEquals( 1/pow( 1+eps, -inf),  inf ) -- check for +0
  assertEquals( 1/pow(-1-eps, -inf),  inf ) -- check for +0
  assertEquals( 1/pow( 1.5  , -inf),  inf ) -- check for +0
  assertEquals( 1/pow(-1.5  , -inf),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , -inf),  inf ) -- check for +0
  assertEquals( 1/pow(-inf  , -inf),  inf ) -- check for +0

  assertEquals( 1/pow(  0   ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(- 0   ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow( 0.5  ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(-0.5  ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow( 1-eps,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(-1+eps,  inf),  inf ) -- check for +0

  assertEquals   (pow( 1+eps,  inf),  inf )
  assertEquals   (pow(-1-eps,  inf),  inf )
  assertEquals   (pow( 1.5  ,  inf),  inf )
  assertEquals   (pow(-1.5  ,  inf),  inf )
  assertEquals   (pow( inf  ,  inf),  inf )
  assertEquals   (pow(-inf  ,  inf),  inf )

  assertEquals( 1/pow(-inf  , -  1), -inf ) -- check for -0
  assertEquals( 1/pow(-inf  , - 11), -inf ) -- check for -0
  assertEquals( 1/pow(-inf  , -0.5),  inf ) -- check for +0
  assertEquals( 1/pow(-inf  , -  2),  inf ) -- check for +0
  assertEquals( 1/pow(-inf  , - 10),  inf ) -- check for +0

  assertEquals  ( pow(-inf  ,    1), -inf )
  assertEquals  ( pow(-inf  ,   11), -inf )
  assertEquals  ( pow(-inf  ,  0.5),  inf )
  assertEquals  ( pow(-inf  ,    2),  inf )
  assertEquals  ( pow(-inf  ,   10),  inf )

  assertEquals( 1/pow( inf  , -0.5),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , -  1),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , -  2),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , - 10),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , - 11),  inf ) -- check for +0

  assertEquals  ( pow( inf  ,  0.5),  inf )
  assertEquals  ( pow( inf  ,    1),  inf )
  assertEquals  ( pow( inf  ,    2),  inf )
  assertEquals  ( pow( inf  ,   10),  inf )
  assertEquals  ( pow( inf  ,   11),  inf )

  assertNaN( pow( 0  ,  nan) )
  assertNaN( pow(-0  ,  nan) )
  assertNaN( pow( 0  , -nan) )
  assertNaN( pow(-0  , -nan) )

  assertNaN( pow(-1  ,  nan) )
  assertNaN( pow(-1  , -nan) )
  assertNaN( pow( nan,   1 ) )
  assertNaN( pow(-nan,   1 ) )
  assertNaN( pow( nan, - 1 ) )
  assertNaN( pow(-nan, - 1 ) )

  assertNaN( pow( inf,  nan) )
  assertNaN( pow(-inf,  nan) )
  assertNaN( pow( inf, -nan) )
  assertNaN( pow(-inf, -nan) )
  assertNaN( pow( nan,  inf) )
  assertNaN( pow(-nan,  inf) )
  assertNaN( pow( nan, -inf) )
  assertNaN( pow(-nan, -inf) )

  assertNaN( pow( nan,  nan) )
  assertNaN( pow(-nan,  nan) )
  assertNaN( pow( nan, -nan) )
  assertNaN( pow(-nan, -nan) )
end

function TestLuaGmath:testRad()
  local r = 0.0174532925199432958 -- pi/180
  for _,v in ipairs(values.deg) do
    assertEquals( rad( v) ,  v*r )
    assertEquals( rad(-v) , -v*r )
  end
  assertEquals( rad(-inf), -inf   )
  assertEquals( rad(-360), -2*pi  )
  assertEquals( rad(-180), -pi    )
  assertEquals( rad(- 90), -pi/2  )
  assertEquals( rad(- 60), -pi/3  )
  assertEquals( rad(- 45), -pi/4  )
  assertEquals( rad(- 30), -pi/6  )
  assertEquals( rad(- 10), -pi/18 )
  assertEquals( rad(   0),  0     )
  assertEquals( rad(  10),  pi/18 )
  assertEquals( rad(  30),  pi/6  )
  assertEquals( rad(  45),  pi/4  )
  assertEquals( rad(  60),  pi/3  )
  assertEquals( rad(  90),  pi/2  )
  assertEquals( rad( 180),  pi    )
  assertEquals( rad( 360),  2*pi  )
  assertEquals( rad( inf),  inf   )

  assertNaN( rad(nan) )
end

function TestLuaGmath:testSin()
  for _,v in ipairs(values.rad) do
    assertAlmostEquals( sin(v)           - -sin(-v)             , 0, eps )
    assertAlmostEquals( sin(v)           -  cos(pi/2-v)         , 0, eps )
    assertAlmostEquals( sin(v)           - (2*sin(v/2)*cos(v/2)), 0, eps )
    assertAlmostEquals( sin(asin( v/pi)) -  v/pi                , 0, eps )
    assertAlmostEquals( sin(asin(-v/pi)) - -v/pi                , 0, eps )
  end
  local r3, r4, r12 = sqrt(3)/2, sqrt(2)/2, sqrt(2)*(sqrt(3)-1)/4
  assertAlmostEquals( sin(    pi   ) -  0  , 0,   eps )
  assertAlmostEquals( sin( 11*pi/12) -  r12, 0, 2*eps )
  assertAlmostEquals( sin(  5*pi/6 ) -  0.5, 0,   eps )
  assertAlmostEquals( sin(  3*pi/4 ) -  r4 , 0,   eps )
  assertAlmostEquals( sin(  2*pi/3 ) -  r3 , 0,   eps )
  assertAlmostEquals( sin(    pi/2 ) -  1  , 0,   eps )
  assertAlmostEquals( sin(    pi/3 ) -  r3 , 0,   eps )
  assertAlmostEquals( sin(    pi/4 ) -  r4 , 0,   eps )
  assertAlmostEquals( sin(    pi/6 ) -  0.5, 0,   eps )
  assertAlmostEquals( sin(    pi/12) -  r12, 0,   eps )
  assertEquals      ( sin(    0    ) -  0  , 0        )
  assertAlmostEquals( sin(-   pi/12) - -r12, 0,   eps )
  assertAlmostEquals( sin(-   pi/6 ) - -0.5, 0,   eps )
  assertAlmostEquals( sin(-   pi/4 ) - -r4 , 0,   eps )
  assertAlmostEquals( sin(-   pi/3 ) - -r3 , 0,   eps )
  assertAlmostEquals( sin(-   pi/2 ) - -1  , 0,   eps )
  assertAlmostEquals( sin(- 2*pi/3 ) - -r3 , 0,   eps )
  assertAlmostEquals( sin(- 3*pi/4 ) - -r4 , 0,   eps )
  assertAlmostEquals( sin(- 5*pi/6 ) - -0.5, 0,   eps )
  assertAlmostEquals( sin(-11*pi/12) - -r12, 0, 2*eps )
  assertAlmostEquals( sin(-   pi   ) - -0  , 0,   eps )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/sin( -0 ) , -inf  ) -- check for -0
  assertEquals( 1/sin(  0 ) ,  inf  ) -- check for +0
  assertNaN( sin(-inf) )
  assertNaN( sin( inf) )
  assertNaN( sin( nan) )
end

function TestLuaGmath:testSinh()
  for _,v in ipairs(values.num) do
    assertEquals( sinh(v), -sinh(-v) )
    if v < 3e-8 then
      assertEquals( sinh(v), v )
    end
    if v > 19.0006 then
      assertEquals( sinh(v), cosh(v) )
    end
    if v < 1e-5 then
      assertAlmostEquals( sinh(v) - v , 0, eps )
    end
    if v > 1e-5 and v < 19.0006 then
      assertAlmostEquals( sinh(v) / (2*sinh(v/2)*cosh(v/2))  - 1, 0,   eps )
      assertAlmostEquals( sinh(v) / (exp(-v)*(exp(2*v)-1)/2) - 1, 0, 2*eps )
      assertAlmostEquals(  exp(v) / (cosh(v) + sinh(v))      - 1, 0,   eps )
    end
  end
  assertEquals( sinh(-711), -inf )
  assertEquals( sinh( 711),  inf )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/sinh(-0  ), -inf ) -- check for -0
  assertEquals( 1/sinh( 0  ),  inf ) -- check for +0
  assertEquals(   sinh(-inf), -inf )
  assertEquals(   sinh( inf),  inf )

  assertNaN( sinh(nan) )
end

function TestLuaGmath:testSqrt()
  for _,v in ipairs(values.num) do
    if v > 0 and v < inf then
      assertAlmostEquals( sqrt(v)*sqrt(v) / v - 1, 0, eps )
    end
  end
  assertNaN( sqrt(-inf) )
  assertNaN( sqrt(-1  ) )
  assertNaN( sqrt(-0.1) )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/sqrt(- 0  ) , -inf ) -- check for -0
  assertEquals( 1/sqrt(  0  ) ,  inf ) -- check for +0
  assertEquals(   sqrt( inf ) ,  inf )
  assertNaN( sqrt(-tiny) )
  assertNaN( sqrt(-inf ) )
  assertNaN( sqrt( nan ) )
end

function TestLuaGmath:testTan()
  for _,v in ipairs(values.rad) do
    assertEquals      (tan( v) - -tan(-v)        , 0     )
    assertAlmostEquals(tan( v) -  sin( v)/cos( v), 0, eps)
    assertAlmostEquals(tan(-v) -  sin(-v)/cos(-v), 0, eps)
  end
  local r3, r6, r12 = sqrt(3), 1/sqrt(3), 2-sqrt(3)
  assertAlmostEquals(  tan(-pi )            , 0,   eps )
  assertAlmostEquals(  tan(-pi+pi/12) -  r12, 0,   eps )
  assertAlmostEquals(  tan(-pi+pi/6 ) -  r6 , 0,   eps )
  assertAlmostEquals(  tan(-pi+pi/4 ) -  1  , 0,   eps )
  assertAlmostEquals(  tan(-pi+pi/3 ) -  r3 , 0, 3*eps )
  assertAlmostEquals(1/tan(-pi/2    )       , 0,   eps )
  assertAlmostEquals(  tan(-pi/3    ) - -r3 , 0, 2*eps )
  assertAlmostEquals(  tan(-pi/4    ) - -1  , 0,   eps )
  assertAlmostEquals(  tan(-pi/6    ) - -r6 , 0,   eps )
  assertAlmostEquals(  tan(-pi/12   ) - -r12, 0,   eps )
  assertEquals      (  tan( 0       ) -  0  , 0        )
  assertAlmostEquals(  tan( pi/12   ) -  r12, 0,   eps )
  assertAlmostEquals(  tan( pi/6    ) -  r6 , 0,   eps )
  assertAlmostEquals(  tan( pi/4    ) -  1  , 0,   eps )
  assertAlmostEquals(  tan( pi/3    ) -  r3 , 0, 2*eps )
  assertAlmostEquals(1/tan( pi/2    )       , 0,   eps )
  assertAlmostEquals(  tan( pi-pi/3 ) - -r3 , 0, 3*eps )
  assertAlmostEquals(  tan( pi-pi/4 ) - -1  , 0,   eps )
  assertAlmostEquals(  tan( pi-pi/6 ) - -r6 , 0,   eps )
  assertAlmostEquals(  tan( pi-pi/12) - -r12, 0,   eps )
  assertAlmostEquals(  tan( pi )            , 0,   eps )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals(        1/tan( -0 ) , -inf  ) -- check for -0
  assertEquals(        1/tan(  0 ) ,  inf  ) -- check for +0
  assertNaN( tan(-inf) )
  assertNaN( tan( inf) )
  assertNaN( tan( nan) )
end

function TestLuaGmath:testTanh()
  for _,v in ipairs(values.num) do
    assertEquals( tanh(v), -tanh(-v) )
    if v < 2e-8 then
      assertEquals( tanh(v), v )
    end
    if v > 19.06155 then
      assertEquals( tanh(v), 1 )
    end
    if v < 8.74e-06 then
      assertAlmostEquals( tanh(v) - v , 0, eps )
    end
    if v > 8.74e-06 and v < 19.06155 then
      assertAlmostEquals( tanh( v) -  sinh( v)/cosh( v), 0, eps )
      assertAlmostEquals( tanh(-v) -  sinh(-v)/cosh(-v), 0, eps )
    end
  end
  assertEquals( tanh(-inf     ), -1 )
  assertEquals( tanh(-19.06155), -1 )
  assertEquals( tanh(  0      ),  0 )
  assertEquals( tanh( 19.06155),  1 )
  assertEquals( tanh( inf)     ,  1 )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/tanh(-0  ), -inf ) -- check for -0
  assertEquals( 1/tanh( 0  ),  inf ) -- check for +0
  assertEquals(   tanh(-inf), -1   )
  assertEquals(   tanh( inf),  1   )
  assertNaN( tanh(nan) )
end

-- functions wo generic

function TestLuaGmath:testAtan2()
  for _,v in ipairs(values.rad2) do
    local x, y = cos(v), sin(v)
    assertAlmostEquals( atan2(y,x) - (pi/2-atan2( x,  y)), 0,   eps )
    assertAlmostEquals( atan2(y,x) - (pi  -atan2( y, -x)), 0, 2*eps )
    assertAlmostEquals( atan2(y,x) -      -atan2(-y,  x) , 0,   eps )
    assertAlmostEquals( atan2(y,x) - (pi  +atan2(-y, -x)), 0, 2*eps )
    if v > 0 then
      assertAlmostEquals( atan2(y,x) / atan (y/x), 1, eps )
    end
  end

  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > 0 or y > 0 then
    assertAlmostEquals( atan2(y,x) - (pi/2-atan2( x,  y)), 0, 2*eps )
    end
    assertAlmostEquals( atan2(y,x) - (pi  -atan2( y, -x)), 0, 2*eps )
    assertAlmostEquals( atan2(y,x) -      -atan2(-y,  x) , 0,   eps )
    assertAlmostEquals( atan2(y,x) - (pi  +atan2(-y, -x)), 0, 2*eps )
  end end

  assertEquals( atan2(    1,    0),  pi/2   )
  assertEquals( atan2( -  1,    0), -pi/2   )
  assertEquals( atan2(  inf,    0),  pi/2   )
  assertEquals( atan2( -inf,    0), -pi/2   )
  assertEquals( atan2(    0,    1),  0      )
  assertEquals( atan2(    1,    1),  pi/4   )
  assertEquals( atan2( -  1,    1), -pi/4   )
  assertEquals( atan2(  inf,    1),  pi/2   )
  assertEquals( atan2( -inf,    1), -pi/2   )
  assertEquals( atan2(    0, -  1),  pi     )
  assertEquals( atan2(    1, -  1),  pi/4*3 )
  assertEquals( atan2( -  1, -  1), -pi/4*3 )
  assertEquals( atan2(  inf, -  1),  pi/2   )
  assertEquals( atan2( -inf, -  1), -pi/2   )
  assertEquals( atan2(    0,  inf),  0      )
  assertEquals( atan2(    1,  inf),  0      )
  assertEquals( atan2( -  1,  inf),  0      )
  assertEquals( atan2(    0, -inf),  pi     )
  assertEquals( atan2(    1, -inf),  pi     )
  assertEquals( atan2( -  1, -inf), -pi     )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals(  atan2(-0 ,-0   ) , -pi   )
  assertEquals(  atan2( 0 ,-0   ) ,  pi   )
  assertEquals(1/atan2(-0 , 0   ) , -inf  ) -- check for -0
  assertEquals(1/atan2( 0 , 0   ) ,  inf  ) -- check for +0
  assertEquals(  atan2(-0 ,-tiny) , -pi   )
  assertEquals(  atan2( 0 ,-tiny) ,  pi   )
  assertEquals(  atan2(-0 ,-1   ) , -pi   )
  assertEquals(  atan2( 0 ,-1   ) ,  pi   )
  assertEquals(  atan2(-0 ,-huge) , -pi   )
  assertEquals(  atan2( 0 ,-huge) ,  pi   )
  assertEquals(1/atan2(-0 , tiny) , -inf  ) -- check for -0
  assertEquals(1/atan2( 0 , tiny) ,  inf  ) -- check for +0
  assertEquals(1/atan2(-0 , 1   ) , -inf  ) -- check for -0
  assertEquals(1/atan2( 0 , 1   ) ,  inf  ) -- check for +0
  assertEquals(1/atan2(-0 , huge) , -inf  ) -- check for -0
  assertEquals(1/atan2( 0 , huge) ,  inf  ) -- check for +0
  assertEquals(  atan2(-tiny, -0) , -pi/2 )
  assertEquals(  atan2(-tiny,  0) , -pi/2 )
  assertEquals(  atan2(-1   , -0) , -pi/2 )
  assertEquals(  atan2(-1   ,  0) , -pi/2 )
  assertEquals(  atan2(-huge, -0) , -pi/2 )
  assertEquals(  atan2(-huge,  0) , -pi/2 )
  assertEquals(  atan2( tiny, -0) ,  pi/2 )
  assertEquals(  atan2( tiny,  0) ,  pi/2 )
  assertEquals(  atan2( 1   , -0) ,  pi/2 )
  assertEquals(  atan2( 1   ,  0) ,  pi/2 )
  assertEquals(  atan2( huge, -0) ,  pi/2 )
  assertEquals(  atan2( huge,  0) ,  pi/2 )

  assertEquals(  atan2(-tiny, -inf) , -pi )
  assertEquals(  atan2(-1   , -inf) , -pi )
  assertEquals(  atan2(-huge, -inf) , -pi )
  assertEquals(  atan2( tiny, -inf) ,  pi )
  assertEquals(  atan2( 1   , -inf) ,  pi )
  assertEquals(  atan2( huge, -inf) ,  pi )
  assertEquals(1/atan2(-tiny,  inf) , -inf ) -- check for -0
  assertEquals(1/atan2(-1   ,  inf) , -inf ) -- check for +0
  assertEquals(1/atan2(-huge,  inf) , -inf ) -- check for -0
  assertEquals(1/atan2( tiny,  inf) ,  inf ) -- check for +0
  assertEquals(1/atan2( 1   ,  inf) ,  inf ) -- check for -0
  assertEquals(1/atan2( huge,  inf) ,  inf ) -- check for +0
  assertEquals(  atan2(-inf, -0   ) , -pi/2 )
  assertEquals(  atan2(-inf, -tiny) , -pi/2 )
  assertEquals(  atan2(-inf, -1   ) , -pi/2 )
  assertEquals(  atan2(-inf, -huge) , -pi/2 )
  assertEquals(  atan2(-inf,  0   ) , -pi/2 )
  assertEquals(  atan2(-inf,  tiny) , -pi/2 )
  assertEquals(  atan2(-inf,  1   ) , -pi/2 )
  assertEquals(  atan2(-inf,  huge) , -pi/2 )
  assertEquals(  atan2( inf, -0   ) ,  pi/2 )
  assertEquals(  atan2( inf, -tiny) ,  pi/2 )
  assertEquals(  atan2( inf, -1   ) ,  pi/2 )
  assertEquals(  atan2( inf, -huge) ,  pi/2 )
  assertEquals(  atan2( inf,  0   ) ,  pi/2 )
  assertEquals(  atan2( inf,  tiny) ,  pi/2 )
  assertEquals(  atan2( inf,  1   ) ,  pi/2 )
  assertEquals(  atan2( inf,  huge) ,  pi/2 )
  assertEquals(  atan2( inf,  -inf) , 3*pi/4 )
  assertEquals(  atan2(-inf,  -inf) ,-3*pi/4 )
  assertEquals(  atan2( inf,   inf) ,   pi/4 )
  assertEquals(  atan2(-inf,   inf) ,-  pi/4 )

  assertNaN( atan2(nan, 0 ) )
  assertNaN( atan2( 0 ,nan) )
  assertNaN( atan2(nan,nan) )
end

function TestLuaGmath:testFMod()
  local e, n, f, r
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x < y then
      assertEquals( fmod( x, y),  x )
      assertEquals( fmod(-x, y), -x )
      assertEquals( fmod( x,-y),  x )
      assertEquals( fmod(-x,-y), -x )
    elseif y/x >= tiny/eps and x < inf and y < inf then
      n = floor(x/y)
      f = fmod(x,y)
      r = x - (n*y + f)
      if r < 0 then r = r+y end
      e = n * eps / 10
      assertTrue( 0 <= f and f < y )
      assertTrue( r < e )
    end
  end end

  assertAlmostEquals( fmod(-5.1, -3  ) - -2.1, 0, 2*eps)
  assertAlmostEquals( fmod(-5.1,  3  ) - -2.1, 0, 2*eps)
  assertAlmostEquals( fmod( 5.1, -3  ) -  2.1, 0, 2*eps)
  assertAlmostEquals( fmod( 5.1,  3  ) -  2.1, 0, 2*eps)

  assertAlmostEquals( fmod(-5.1, -3.1) - -2  , 0, 2*eps)
  assertAlmostEquals( fmod(-5.1,  3.1) - -2  , 0, 2*eps)
  assertAlmostEquals( fmod( 5.1, -3.1) -  2  , 0, 2*eps)
  assertAlmostEquals( fmod( 5.1,  3.1) -  2  , 0, 2*eps)

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/fmod(- 0,  0.5), -inf) -- check for -0
  assertEquals( 1/fmod(  0,  0.5),  inf) -- check for +0
  assertEquals( 1/fmod(- 0, -0.5), -inf) -- check for -0
  assertEquals( 1/fmod(  0, -0.5),  inf) -- check for +0
  assertEquals( 1/fmod(- 0,  1  ), -inf) -- check for -0
  assertEquals( 1/fmod(  0,  1  ),  inf) -- check for +0
  assertEquals( 1/fmod(- 0, -1  ), -inf) -- check for -0
  assertEquals( 1/fmod(  0, -1  ),  inf) -- check for +0
  assertEquals( 1/fmod(- 0,  10 ), -inf) -- check for -0
  assertEquals( 1/fmod(  0,  10 ),  inf) -- check for +0
  assertEquals( 1/fmod(- 0, -10 ), -inf) -- check for -0
  assertEquals( 1/fmod(  0, -10 ),  inf) -- check for +0

  assertEquals(  fmod( 0.5,  inf),  0.5)
  assertEquals(  fmod(-0.5,  inf), -0.5)
  assertEquals(  fmod( 0.5, -inf),  0.5)
  assertEquals(  fmod(-0.5, -inf), -0.5)
  assertEquals(  fmod( 1  ,  inf),  1  )
  assertEquals(  fmod(-1  ,  inf), -1  )
  assertEquals(  fmod( 1  , -inf),  1  )
  assertEquals(  fmod(-1  , -inf), -1  )
  assertEquals(  fmod( 10 ,  inf),  10 )
  assertEquals(  fmod(-10 ,  inf), -10 )
  assertEquals(  fmod( 10 , -inf),  10 )
  assertEquals(  fmod(-10 , -inf), -10 )

  assertNaN( fmod( inf,  0.5) )
  assertNaN( fmod( inf, -0.5) )
  assertNaN( fmod(-inf,  0.5) )
  assertNaN( fmod(-inf, -0.5) )
  assertNaN( fmod( inf,  1  ) )
  assertNaN( fmod( inf, -1  ) )
  assertNaN( fmod(-inf,  1  ) )
  assertNaN( fmod(-inf, -1  ) )
  assertNaN( fmod( inf,  10 ) )
  assertNaN( fmod( inf, -10 ) )
  assertNaN( fmod(-inf,  10 ) )
  assertNaN( fmod(-inf, -10 ) )

  assertNaN( fmod( 0.5,  0  ) )
  assertNaN( fmod(-0.5,  0  ) )
  assertNaN( fmod( 0.5, -0  ) )
  assertNaN( fmod(-0.5, -0  ) )
  assertNaN( fmod( 1  ,  0  ) )
  assertNaN( fmod(-1  ,  0  ) )
  assertNaN( fmod( 1  , -0  ) )
  assertNaN( fmod(-1  , -0  ) )
  assertNaN( fmod( 10 ,  0  ) )
  assertNaN( fmod(-10 ,  0  ) )
  assertNaN( fmod( 10 , -0  ) )
  assertNaN( fmod(-10 , -0  ) )

  assertNaN( fmod( inf,  inf) )
  assertNaN( fmod(-inf,  inf) )
  assertNaN( fmod( inf, -inf) )
  assertNaN( fmod(-inf, -inf) )

  assertNaN( fmod( nan,  nan) )
  assertNaN( fmod(-nan,  nan) )
  assertNaN( fmod( nan, -nan) )
  assertNaN( fmod(-nan, -nan) )
end

function TestLuaGmath:testLdexp()
  for i,v in ipairs(values.num) do
    assertEquals( ldexp(v,  i), v*2^ i )
    assertEquals( ldexp(v, -i), v*2^-i )
    assertEquals( ldexp(v,  0), v )
    assertEquals( ldexp(0,  i), 0 )
  end
  for i,v in ipairs(values.rad) do
    assertEquals( ldexp(v,  i), v*2^ i )
    assertEquals( ldexp(v, -i), v*2^-i )
    assertEquals( ldexp(v,  0), v )
    assertEquals( ldexp(0,  i), 0 )
  end

  assertEquals( ldexp(-inf,   0), -inf )
  assertEquals( ldexp( 3  , 1.9),    6 )
  assertEquals( ldexp( 3  , 2.1),   12 )
  assertEquals( ldexp( inf,   0),  inf )

  assertNaN( ldexp(nan,   0) )
  assertNaN( ldexp(nan,   1) )
  assertNaN( ldexp(nan, nan) )
end

function TestLuaGmath:testFrexp()
  assertEquals( {frexp(0)}, {0,0} )
  assertEquals( {frexp(1)}, {0.5,1} )

  for i=-100,100 do
    assertEquals( ldexp(frexp(2^i)), 2^i )
  end
  for x=-100,100,0.1 do
    assertEquals( ldexp(frexp(x)), x )
  end

  assertEquals( {frexp(- inf)}, {-inf  ,     0} )
  assertEquals( {frexp(- 0.2)}, {-0.8  , -   2} )
  assertEquals( {frexp( tiny)}, { 0.5  , -1021} )
  assertEquals( {frexp(  eps)}, { 0.5  , -  51} )
  assertEquals( {frexp(  0.1)}, { 0.8  , -   3} )
  assertEquals( {frexp(  0.7)}, { 0.7  ,     0} )
  assertEquals( {frexp(  1  )}, { 0.5  ,     1} )
  assertEquals( {frexp(  1.1)}, { 0.55 ,     1} )
  assertEquals( {frexp(  1.7)}, { 0.85  ,    1} )
  assertEquals( {frexp(  2.1)}, { 0.525,     2} )
  assertEquals( {frexp(  inf)}, { inf  ,     0} )

  local f,e
  f,e = frexp(1-eps)
  assertEquals( 1 , 1 )
  assertEquals( 0 , 0 )
  f,e = frexp(1+eps)
  assertEquals( 1 , 1 )
  assertEquals( 0 , 0 )
  f,e = frexp(huge)
  assertAlmostEquals( f - 1    , 0, eps)
  assertEquals      ( e - 1024 , 0     )

  assertNaN( frexp(nan,   0) )
  assertNaN( frexp(nan,   1) )
  assertNaN( frexp(nan, nan) )
end

function TestLuaGmath:testRandom()
  for i=1,1000 do
    assertTrue( random()    >= 0   )
    assertTrue( random()    <  1   )
    assertTrue( random(100) >= 1   )
    assertTrue( random(100) <= 100 )
    assertTrue( random(-1,1) >= -1 )
    assertTrue( random(-1,1) <=  1 )
    assertTrue( random(-1,2^52) >= -1    )
    assertTrue( random(-1,2^52) <=  2^52 )
  end

  assertNaN( random(nan,  0 ) )
  assertNaN( random( 0 , nan) )
  assertNaN( random(nan, nan) )
end

function TestLuaGmath:testRandomseed()
  local val  = {}
  local oldVal = {}
  for j=1,10 do
    randomseed( j )
    for i=1,500 do
      val[i] = random(0,2^52)
      assertTrue ( val[i] >= 0    )
      assertTrue ( val[i] <= 2^52 )
      assertFalse( val[i] == oldVal[i] )
      oldVal[i] = val[i]
    end
  end
end

-- extra generic functions

function TestLuaGmath:testAngle()
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    assertEquals( angle( x,  y), atan2( y,  x) )
    assertEquals( angle( x, -y), atan2(-y,  x) )
    assertEquals( angle(-x,  y), atan2( y, -x) )
    assertEquals( angle(-x, -y), atan2(-y, -x) )
  end end

  for _,x in ipairs(values.rad) do
  for _,y in ipairs(values.rad) do
    x, y = x/pi, y/pi
    assertEquals( angle( x,  y), atan2( y,  x) )
    assertEquals( angle( x, -y), atan2(-y,  x) )
    assertEquals( angle(-x,  y), atan2( y, -x) )
    assertEquals( angle(-x, -y), atan2(-y, -x) )
  end end

  assertEquals( angle(    0,    0),  0      )
  assertEquals( angle(    0,    1),  pi/2   )
  assertEquals( angle(    0, -  1), -pi/2   )
  assertEquals( angle(    0,  inf),  pi/2   )
  assertEquals( angle(    0, -inf), -pi/2   )
  assertEquals( angle(    1,    0),  0      )
  assertEquals( angle(    1,    1),  pi/4   )
  assertEquals( angle(    1, -  1), -pi/4   )
  assertEquals( angle(    1,  inf),  pi/2   )
  assertEquals( angle(    1, -inf), -pi/2   )
  assertEquals( angle( -  1,    0),  pi     )
  assertEquals( angle( -  1,    1),  pi/4*3 )
  assertEquals( angle( -  1, -  1), -pi/4*3 )
  assertEquals( angle( -  1,  inf),  pi/2   )
  assertEquals( angle( -  1, -inf), -pi/2   )
  assertEquals( angle(  inf,    0),  0      )
  assertEquals( angle(  inf,    1),  0      )
  assertEquals( angle(  inf, -  1),  0      )
  assertEquals( angle(  inf,  inf),  pi/4   )
  assertEquals( angle(  inf, -inf), -pi/4   )
  assertEquals( angle( -inf,    0),  pi     )
  assertEquals( angle( -inf,    1),  pi     )
  assertEquals( angle( -inf, -  1), -pi     )
  assertEquals( angle( -inf,  inf),  pi/4*3 )
  assertEquals( angle( -inf, -inf), -pi/4*3 )

  assertNaN( angle(nan, 0 ) )
  assertNaN( angle( 0 ,nan) )
  assertNaN( angle(nan,nan) )
end

function TestLuaGmath:testFrac()
  for _,v in ipairs(values.num) do
    if v == inf then break end -- pb, see below
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
--  assertEquals( frac(  inf) ,     0 ) -- get nan, pb with modf and jit
  assertEquals( frac(-   0) , -   0 )
  assertEquals( frac(-tiny) , -tiny )
  assertEquals( frac(- 0.1) , - 0.1 )
  assertEquals( frac(- 0.5) , - 0.5 )
  assertEquals( frac(- 0.7) , - 0.7 )
  assertEquals( frac(-   1) , -   0 )
  assertEquals( frac(- 1.5) , - 0.5 )
  assertEquals( frac(- 1.7) , - 0.7 )
  assertEquals( frac(-huge) , -   0 )
--  assertTrue  ( frac(- inf) ,     0 ) -- get nan, pb with modf and jit

  assertAlmostEquals( frac( 1.1)-0.1, 0, eps/2 )
  assertAlmostEquals( frac(-1.1)+0.1, 0, eps/2 )

  assertNaN( frac(nan) )
end

function TestLuaGmath:testRound()
  assertEquals( round( tiny) ,     0 )
  assertEquals( round(  0.1) ,     0 )
  assertEquals( round(  0.5) ,     1 )
  assertEquals( round(  0.7) ,     1 )
  assertEquals( round(    1) ,     1 )
  assertEquals( round(  1.1) ,     1 )
  assertEquals( round(  1.5) ,     2 )
  assertEquals( round(  1.7) ,     2 )
  assertEquals( round( huge) ,  huge )
  assertEquals( round(-tiny) , -   0 )
  assertEquals( round(- 0.1) , -   0 )
  assertEquals( round(- 0.5) , -   1 )
  assertEquals( round(- 0.7) , -   1 )
  assertEquals( round(-   1) , -   1 )
  assertEquals( round(- 1.1) , -   1 )
  assertEquals( round(- 1.5) , -   2 )
  assertEquals( round(- 1.7) , -   2 )
  assertEquals( round(-huge) , -huge )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/round(-  0 ) , -inf ) -- check for -0
  assertEquals( 1/round(   0 ) ,  inf ) -- check for +0
  assertEquals(   round(- inf) , -inf )
  assertEquals(   round(  inf) ,  inf )

  assertNaN( round(nan) )
end

function TestLuaGmath:testSign()
  assertEquals( sign(    0) ,  1 )
  assertEquals( sign( tiny) ,  1 )
  assertEquals( sign(  0.1) ,  1 )
  assertEquals( sign(    1) ,  1 )
  assertEquals( sign( huge) ,  1 )
  assertEquals( sign(  inf) ,  1 )
  assertEquals( sign(-   0) ,  1 )
  assertEquals( sign(-tiny) , -1 )
  assertEquals( sign(- 0.1) , -1 )
  assertEquals( sign(-   1) , -1 )
  assertEquals( sign(-huge) , -1 )
  assertEquals( sign(- inf) , -1 )

  assertNaN( sign(nan) )
end

function TestLuaGmath:testSinc()
  for _,v in ipairs(values.num) do
    if v < 1e-7 then
      assertEquals( sinc( v), 1 )
      assertEquals( sinc(-v), 1 )
    elseif v < inf then
      assertEquals( sinc( v), sinc(-v) )
      assertEquals( sinc( v), sin(v) / v )
      assertEquals( sinc(-v), sin(-v)/-v )
    end
  end

  assertNaN( sinc(-inf) )
  assertNaN( sinc( inf) )
  assertNaN( sinc( nan) )
end

function TestLuaGmath:testStep()
  assertEquals( step(    0) , 1 )
  assertEquals( step( tiny) , 1 )
  assertEquals( step(  0.1) , 1 )
  assertEquals( step(    1) , 1 )
  assertEquals( step( huge) , 1 )
  assertEquals( step(  inf) , 1 )
  assertEquals( step(-   0) , 1 )
  assertEquals( step(-tiny) , 0 )
  assertEquals( step(- 0.1) , 0 )
  assertEquals( step(-   1) , 0 )
  assertEquals( step(-huge) , 0 )
  assertEquals( step(- inf) , 0 )

  assertNaN( step(nan) )
end

function TestLuaGmath:testTrunc()
  for _,v in ipairs(values.num) do
    assertEquals( trunc( v+eps), first(modf( v+eps)) )
    assertEquals( trunc( v-eps), first(modf( v-eps)) )
    assertEquals( trunc(-v+eps), first(modf(-v+eps)) )
    assertEquals( trunc(-v-eps), first(modf(-v-eps)) )
    assertEquals( trunc( v+0.1), first(modf( v+0.1)) )
    assertEquals( trunc( v-0.1), first(modf( v-0.1)) )
    assertEquals( trunc(-v+0.1), first(modf(-v+0.1)) )
    assertEquals( trunc(-v-0.1), first(modf(-v-0.1)) )
    assertEquals( trunc( v+0.7), first(modf( v+0.7)) )
    assertEquals( trunc( v-0.7), first(modf( v-0.7)) )
    assertEquals( trunc(-v+0.7), first(modf(-v+0.7)) )
    assertEquals( trunc(-v-0.7), first(modf(-v-0.7)) )
  end

  assertEquals( trunc( tiny) ,     0 )
  assertEquals( trunc(  0.1) ,     0 )
  assertEquals( trunc(  0.5) ,     0 )
  assertEquals( trunc(  0.7) ,     0 )
  assertEquals( trunc(    1) ,     1 )
  assertEquals( trunc(  1.1) ,     1 )
  assertEquals( trunc(  1.5) ,     1 )
  assertEquals( trunc(  1.7) ,     1 )
  assertEquals( trunc( huge) ,  huge )
  assertEquals( trunc(-tiny) , -   0 )
  assertEquals( trunc(- 0.1) , -   0 )
  assertEquals( trunc(- 0.5) , -   0 )
  assertEquals( trunc(- 0.7) , -   0 )
  assertEquals( trunc(-   1) , -   1 )
  assertEquals( trunc(- 1.1) , -   1 )
  assertEquals( trunc(- 1.5) , -   1 )
  assertEquals( trunc(- 1.7) , -   1 )
  assertEquals( trunc(-huge) , -huge )

  -- Check for IEEE:IEC 60559 compliance
  assertEquals( 1/trunc(-  0 ) , -inf ) -- check for -0
  assertEquals( 1/trunc(   0 ) ,  inf ) -- check for +0
  assertEquals(   trunc(- inf) , -inf )
  assertEquals(   trunc(  inf) ,  inf )

  assertNaN( trunc(nan) )
end

-- operators as functions

function TestLuaGmath:testMulOp()
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > tiny and y > tiny and x < huge and y < huge then
      assertEquals      (  mul(x,y), mul(y,x) )
      assertAlmostEquals( (mul(x,y)/y - x)/x, 0, eps )
    end
  end end

  assertEquals( mul(   0,   1),    0 )
  assertEquals( mul(   0,-  1), -  0 )
  assertEquals( mul(-  0,   1), -  0 )
  assertEquals( mul(-  0,-  1),    0 )

  assertEquals( mul(   1, inf),  inf )
  assertEquals( mul(   1,-inf), -inf )
  assertEquals( mul(-  1, inf), -inf )
  assertEquals( mul(-  1,-inf),  inf )

  assertEquals( mul( inf,   1),  inf )
  assertEquals( mul(-inf,   1), -inf )
  assertEquals( mul( inf,-  1), -inf )
  assertEquals( mul(-inf,-  1),  inf )

  assertEquals( mul( inf, inf),  inf )
  assertEquals( mul(-inf, inf), -inf )
  assertEquals( mul( inf,-inf), -inf )
  assertEquals( mul(-inf,-inf),  inf )

  assertNaN( mul(   0, inf) )
  assertNaN( mul(   0,-inf) )
  assertNaN( mul(-  0, inf) )
  assertNaN( mul(-  0,-inf) )

  assertNaN( mul( inf,   0) )
  assertNaN( mul(-inf,   0) )
  assertNaN( mul( inf,-  0) )
  assertNaN( mul(-inf,-  0) )

  assertNaN( mul(  0 , nan) )
  assertNaN( mul( nan,  0 ) )
  assertNaN( mul(  1 , nan) )
  assertNaN( mul( nan,  1 ) )
end

function TestLuaGmath:testDivOp()
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > tiny and y > tiny and x < huge and y < huge then
      assertAlmostEquals( (div(x,y)*y - x)/x, 0, eps )
    end
  end end

  assertEquals( 1/div(   0,   1),  inf ) -- check for +0
  assertEquals( 1/div(   0,-  1), -inf ) -- check for -0
  assertEquals( 1/div(-  0,   1), -inf ) -- check for -0
  assertEquals( 1/div(-  0,-  1),  inf ) -- check for +0

  assertEquals(   div(   1,   0),  inf )
  assertEquals(   div(-  1,   0), -inf )
  assertEquals(   div(   1,-  0), -inf )
  assertEquals(   div(-  1,-  0),  inf )

  assertEquals( 1/div(   0, inf),  inf ) -- check for +0
  assertEquals( 1/div(   0,-inf), -inf ) -- check for -0
  assertEquals( 1/div(-  0, inf), -inf ) -- check for -0
  assertEquals( 1/div(-  0,-inf),  inf ) -- check for +0

  assertEquals(   div( inf,   0),  inf )
  assertEquals(   div(-inf,   0), -inf )
  assertEquals(   div( inf,-  0), -inf )
  assertEquals(   div(-inf,-  0),  inf )

  assertEquals( 1/div(   1, inf),  inf ) -- check for +0
  assertEquals( 1/div(   1,-inf), -inf ) -- check for -0
  assertEquals( 1/div(-  1, inf), -inf ) -- check for -0
  assertEquals( 1/div(-  1,-inf),  inf ) -- check for +0

  assertEquals(   div( inf,   1),  inf )
  assertEquals(   div(-inf,   1), -inf )
  assertEquals(   div( inf,-  1), -inf )
  assertEquals(   div(-inf,-  1),  inf )

  assertNaN( div( inf, inf) )
  assertNaN( div(-inf, inf) )
  assertNaN( div( inf,-inf) )
  assertNaN( div(-inf,-inf) )

  assertNaN( div( 0, 0) )
  assertNaN( div( 0,-0) )
  assertNaN( div(-0, 0) )
  assertNaN( div(-0,-0) )

  assertNaN( div( 0 , nan) )
  assertNaN( div(nan,  0 ) )
  assertNaN( div( 1 , nan) )
  assertNaN( div(nan,  1 ) )
end

function TestLuaGmath:testModOp()
  -- Lua: a % b == a - math.floor(a/b)*b
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if y > 0 and y < inf and x < inf then
      assertEquals( x%y , x - floor(x/y)*y )
    end
  end end

  assertAlmostEquals( mod(-5.1, -3  ) - -2.1, 0, 2*eps)
  assertAlmostEquals( mod(-5.1,  3  ) -  0.9, 0, 2*eps)
  assertAlmostEquals( mod( 5.1, -3  ) - -0.9, 0, 2*eps)
  assertAlmostEquals( mod( 5.1,  3  ) -  2.1, 0, 2*eps)

  assertAlmostEquals( mod(-5.1, -3.1) - -2  , 0, 2*eps)
  assertAlmostEquals( mod(-5.1,  3.1) -  1.1, 0, 2*eps)
  assertAlmostEquals( mod( 5.1, -3.1) - -1.1, 0, 2*eps)
  assertAlmostEquals( mod( 5.1,  3.1) -  2  , 0, 2*eps)

  assertNaN( mod( 1,  inf) )
  assertNaN( mod(-1,  inf) )
  assertNaN( mod( 1, -inf) )
  assertNaN( mod(-1, -inf) )

  assertNaN( mod( inf,  inf) )
  assertNaN( mod(-inf, -inf) )
  assertNaN( mod( inf,  nan) )
  assertNaN( mod(-inf,  nan) )
  assertNaN( mod(   1,    0) )
  assertNaN( mod(   1,  - 0) )
  assertNaN( mod( nan,  nan) )
end

function TestLuaGmath:testPowOp()
  local pow = function(a,b) return a^b end -- see testPow

  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > 1/709.78 and y > 1/709.78 and x < 709.78 and y < 709.78 then
      assertAlmostEquals( log(pow(x,y)) - y*log(x), 0, max(abs(y*log(x)) * eps, 4*eps) )
    end
  end end

  -- Check for IEEE:IEC 60559 compliance
  assertEquals   (pow(  0 , - 11),  inf )
  assertEquals   (pow(- 0 , - 11), -inf )

  assertEquals   (pow(  0 , - .5),  inf )
  assertEquals   (pow(- 0 , - .5),  inf )
  assertEquals   (pow(  0 , -  2),  inf )
  assertEquals   (pow(- 0 , -  2),  inf )
  assertEquals   (pow(  0 , - 10),  inf )
  assertEquals   (pow(- 0 , - 10),  inf )

  assertEquals( 1/pow(  0 ,    1),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,    1), -inf ) -- check for -0
  assertEquals( 1/pow(  0 ,   11),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,   11), -inf ) -- check for -0

  assertEquals( 1/pow(  0 ,  0.5),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,  0.5),  inf ) -- check for +0
  assertEquals( 1/pow(  0 ,    2),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,    2),  inf ) -- check for +0
  assertEquals( 1/pow(  0 ,   10),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,   10),  inf ) -- check for +0
  assertEquals( 1/pow(  0 ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(- 0 ,  inf),  inf ) -- check for +0

  assertEquals   (pow(- 1 ,  inf),   1  )
  assertEquals   (pow(- 1 , -inf),   1  )

  assertEquals   (pow(  1 ,   0 ),   1  )
  assertEquals   (pow(  1 , - 0 ),   1  )
  assertEquals   (pow(  1 ,  0.5),   1  )
  assertEquals   (pow(  1 , -0.5),   1  )
  assertEquals   (pow(  1 ,   1 ),   1  )
  assertEquals   (pow(  1 , - 1 ),   1  )
  assertEquals   (pow(  1 ,  inf),   1  )
  assertEquals   (pow(  1 , -inf),   1  )
  assertEquals   (pow(  1 ,  nan),   1  )
  assertEquals   (pow(  1 , -nan),   1  )

  assertEquals   (pow(  0 ,   0 ),   1  )
  assertEquals   (pow(- 0 ,   0 ),   1  )
  assertEquals   (pow( 0.5,   0 ),   1  )
  assertEquals   (pow(-0.5,   0 ),   1  )
  assertEquals   (pow(  1 ,   0 ),   1  )
  assertEquals   (pow(- 1 ,   0 ),   1  )
  assertEquals   (pow( inf,   0 ),   1  )
  assertEquals   (pow(-inf,   0 ),   1  )
  assertEquals   (pow( nan,   0 ),   1  )
  assertEquals   (pow(-nan,   0 ),   1  )

  assertEquals   (pow(  0 , - 0 ),   1  )
  assertEquals   (pow(- 0 , - 0 ),   1  )
  assertEquals   (pow( 0.5, - 0 ),   1  )
  assertEquals   (pow(-0.5, - 0 ),   1  )
  assertEquals   (pow(  1 , - 0 ),   1  )
  assertEquals   (pow(- 1 , - 0 ),   1  )
  assertEquals   (pow( inf, - 0 ),   1  )
  assertEquals   (pow(-inf, - 0 ),   1  )
  assertEquals   (pow( nan, - 0 ),   1  )
  assertEquals   (pow(-nan, - 0 ),   1  )

  assertNaN( pow(- 1  , 0.5) )
  assertNaN( pow(- 1  ,-0.5) )
  assertNaN( pow(- 1  , 1.5) )
  assertNaN( pow(- 1  ,-1.5) )

  assertEquals   (pow(  0   , -inf),  inf )
  assertEquals   (pow(- 0   , -inf),  inf )
  assertEquals   (pow( 0.5  , -inf),  inf )
  assertEquals   (pow(-0.5  , -inf),  inf )
  assertEquals   (pow( 1-eps, -inf),  inf )
  assertEquals   (pow(-1+eps, -inf),  inf )

  assertEquals( 1/pow( 1+eps, -inf),  inf ) -- check for +0
  assertEquals( 1/pow(-1-eps, -inf),  inf ) -- check for +0
  assertEquals( 1/pow( 1.5  , -inf),  inf ) -- check for +0
  assertEquals( 1/pow(-1.5  , -inf),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , -inf),  inf ) -- check for +0
  assertEquals( 1/pow(-inf  , -inf),  inf ) -- check for +0

  assertEquals( 1/pow(  0   ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(- 0   ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow( 0.5  ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(-0.5  ,  inf),  inf ) -- check for +0
  assertEquals( 1/pow( 1-eps,  inf),  inf ) -- check for +0
  assertEquals( 1/pow(-1+eps,  inf),  inf ) -- check for +0

  assertEquals   (pow( 1+eps,  inf),  inf )
  assertEquals   (pow(-1-eps,  inf),  inf )
  assertEquals   (pow( 1.5  ,  inf),  inf )
  assertEquals   (pow(-1.5  ,  inf),  inf )
  assertEquals   (pow( inf  ,  inf),  inf )
  assertEquals   (pow(-inf  ,  inf),  inf )

  assertEquals( 1/pow(-inf  , -  1), -inf ) -- check for -0
  assertEquals( 1/pow(-inf  , - 11), -inf ) -- check for -0
  assertEquals( 1/pow(-inf  , -0.5),  inf ) -- check for +0
  assertEquals( 1/pow(-inf  , -  2),  inf ) -- check for +0
  assertEquals( 1/pow(-inf  , - 10),  inf ) -- check for +0

  assertEquals  ( pow(-inf  ,    1), -inf )
  assertEquals  ( pow(-inf  ,   11), -inf )
  assertEquals  ( pow(-inf  ,  0.5),  inf )
  assertEquals  ( pow(-inf  ,    2),  inf )
  assertEquals  ( pow(-inf  ,   10),  inf )

  assertEquals( 1/pow( inf  , -0.5),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , -  1),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , -  2),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , - 10),  inf ) -- check for +0
  assertEquals( 1/pow( inf  , - 11),  inf ) -- check for +0

  assertEquals  ( pow( inf  ,  0.5),  inf )
  assertEquals  ( pow( inf  ,    1),  inf )
  assertEquals  ( pow( inf  ,    2),  inf )
  assertEquals  ( pow( inf  ,   10),  inf )
  assertEquals  ( pow( inf  ,   11),  inf )

  assertNaN( pow( 0  ,  nan) )
  assertNaN( pow(-0  ,  nan) )
  assertNaN( pow( 0  , -nan) )
  assertNaN( pow(-0  , -nan) )

  assertNaN( pow(-1  ,  nan) )
  assertNaN( pow(-1  , -nan) )
  assertNaN( pow( nan,   1 ) )
  assertNaN( pow(-nan,   1 ) )
  assertNaN( pow( nan, - 1 ) )
  assertNaN( pow(-nan, - 1 ) )

  assertNaN( pow( inf,  nan) )
  assertNaN( pow(-inf,  nan) )
  assertNaN( pow( inf, -nan) )
  assertNaN( pow(-inf, -nan) )
  assertNaN( pow( nan,  inf) )
  assertNaN( pow(-nan,  inf) )
  assertNaN( pow( nan, -inf) )
  assertNaN( pow(-nan, -inf) )

  assertNaN( pow( nan,  nan) )
  assertNaN( pow(-nan,  nan) )
  assertNaN( pow( nan, -nan) )
  assertNaN( pow(-nan, -nan) )
end

function TestLuaGmath:testPowOp2()
  for _,x in ipairs(values.num) do
  for _,y in ipairs(values.num) do
    if x > 1/709.78 and y > 1/709.78 and x < 709.78 and y < 709.78 then
      assertAlmostEquals( log(x^y) - y*log(x), 0, max(abs(y*log(x)) * eps, 4*eps) )
    end
  end end

  -- Check for IEEE:IEC 60559 compliance
  assertEquals   (   0 ^ - 11,  inf )
  assertEquals   ( - 0 ^ - 11, -inf )

  assertEquals   (   0 ^ - .5,  inf )
  assertEquals   ((- 0)^ - .5,  inf )
  assertEquals   (   0 ^ -  2,  inf )
  assertEquals   ((- 0)^ -  2,  inf )
  assertEquals   (   0 ^ - 10,  inf )
  assertEquals   ((- 0)^ - 10,  inf )

  assertEquals( 1/   0 ^    1,  inf ) -- check for +0
  assertEquals( 1/(- 0)^    1, -inf ) -- check for -0
  assertEquals( 1/   0 ^   11,  inf ) -- check for +0
  assertEquals( 1/(- 0)^   11, -inf ) -- check for -0

  assertEquals( 1/   0 ^  0.5,  inf ) -- check for +0
  assertEquals( 1/(- 0)^  0.5,  inf ) -- check for +0
  assertEquals( 1/   0 ^    2,  inf ) -- check for +0
  assertEquals( 1/(- 0)^    2,  inf ) -- check for +0
  assertEquals( 1/   0 ^   10,  inf ) -- check for +0
  assertEquals( 1/(- 0)^   10,  inf ) -- check for +0
  assertEquals( 1/   0 ^  inf,  inf ) -- check for +0
  assertEquals( 1/(- 0)^  inf,  inf ) -- check for +0

  assertEquals   ((- 1)^  inf,   1  )
  assertEquals   ((- 1)^ -inf,   1  )

  assertEquals   (   1 ^   0 ,   1  )
  assertEquals   (   1 ^ - 0 ,   1  )
  assertEquals   (   1 ^  0.5,   1  )
  assertEquals   (   1 ^ -0.5,   1  )
  assertEquals   (   1 ^   1 ,   1  )
  assertEquals   (   1 ^ - 1 ,   1  )
  assertEquals   (   1 ^  inf,   1  )
  assertEquals   (   1 ^ -inf,   1  )
  assertEquals   (   1 ^  nan,   1  )
  assertEquals   (   1 ^ -nan,   1  )

  assertEquals   (   0  ^  0 ,   1  )
  assertEquals   ((- 0 )^  0 ,   1  )
  assertEquals   (  0.5 ^  0 ,   1  )
  assertEquals   ((-0.5)^  0 ,   1  )
  assertEquals   (   1  ^  0 ,   1  )
  assertEquals   ((- 1 )^  0 ,   1  )
  assertEquals   (  inf ^  0 ,   1  )
  assertEquals   ((-inf)^  0 ,   1  )
  assertEquals   (  nan ^  0 ,   1  )
  assertEquals   ((-nan)^  0 ,   1  )

  assertEquals   (   0  ^- 0 ,   1  )
  assertEquals   ((- 0 )^- 0 ,   1  )
  assertEquals   (  0.5 ^- 0 ,   1  )
  assertEquals   ((-0.5)^- 0 ,   1  )
  assertEquals   (   1  ^- 0 ,   1  )
  assertEquals   ((- 1 )^- 0 ,   1  )
  assertEquals   (  inf ^- 0 ,   1  )
  assertEquals   ((-inf)^- 0 ,   1  )
  assertEquals   (  nan ^- 0 ,   1  )
  assertEquals   ((-nan)^- 0 ,   1  )

  assertNaN( (- 1)^ 0.5 )
  assertNaN( (- 1)^-0.5 )
  assertNaN( (- 1)^ 1.5 )
  assertNaN( (- 1)^-1.5 )

  assertEquals   (   0    ^ -inf,  inf )
  assertEquals   ((- 0   )^ -inf,  inf )
  assertEquals   (  0.5   ^ -inf,  inf )
  assertEquals   ((-0.5  )^ -inf,  inf )
  assertEquals   (( 1-eps)^ -inf,  inf )
  assertEquals   ((-1+eps)^ -inf,  inf )

  assertEquals(1/(( 1+eps)^ -inf), inf ) -- check for +0
  assertEquals(1/((-1-eps)^ -inf), inf ) -- check for +0
  assertEquals(1/( 1.5    ^ -inf), inf ) -- check for +0
  assertEquals(1/((-1.5  )^ -inf), inf ) -- check for +0
  assertEquals(1/( inf    ^ -inf), inf ) -- check for +0
  assertEquals(1/((-inf  )^ -inf), inf ) -- check for +0

  assertEquals(1/(     0  ^  inf), inf ) -- check for +0
  assertEquals(1/((-   0 )^  inf), inf ) -- check for +0
  assertEquals(1/(    0.5 ^  inf), inf ) -- check for +0
  assertEquals(1/((-  0.5)^  inf), inf ) -- check for +0
  assertEquals(1/(( 1-eps)^  inf), inf ) -- check for +0
  assertEquals(1/((-1+eps)^  inf), inf ) -- check for +0

  assertEquals   (( 1+eps)^  inf,  inf )
  assertEquals   ((-1-eps)^  inf,  inf )
  assertEquals   (  1.5   ^  inf,  inf )
  assertEquals   ((-1.5  )^  inf,  inf )
  assertEquals   (  inf   ^  inf,  inf )
  assertEquals   ((-inf  )^  inf,  inf )

  assertEquals( 1/((-inf) ^ -  1), -inf ) -- check for -0
  assertEquals( 1/((-inf) ^ - 11), -inf ) -- check for -0
  assertEquals( 1/((-inf) ^ -0.5),  inf ) -- check for +0
  assertEquals( 1/((-inf) ^ -  2),  inf ) -- check for +0
  assertEquals( 1/((-inf) ^ - 10),  inf ) -- check for +0

  assertEquals  (  (-inf) ^    1 , -inf )
  assertEquals  (  (-inf) ^   11 , -inf )
  assertEquals  (  (-inf) ^  0.5 ,  inf )
  assertEquals  (  (-inf) ^    2 ,  inf )
  assertEquals  (  (-inf) ^   10 ,  inf )

  assertEquals( 1/(  inf  ^ -0.5),  inf ) -- check for +0
  assertEquals( 1/(  inf  ^ -  1),  inf ) -- check for +0
  assertEquals( 1/(  inf  ^ -  2),  inf ) -- check for +0
  assertEquals( 1/(  inf  ^ - 10),  inf ) -- check for +0
  assertEquals( 1/(  inf  ^ - 11),  inf ) -- check for +0

  assertEquals  (    inf  ^  0.5 ,  inf )
  assertEquals  (    inf  ^    1 ,  inf )
  assertEquals  (    inf  ^    2 ,  inf )
  assertEquals  (    inf  ^   10 ,  inf )
  assertEquals  (    inf  ^   11 ,  inf )

  assertNaN(   0   ^  nan )
  assertNaN( (-0  )^  nan )
  assertNaN(   0   ^ -nan )
  assertNaN( (-0  )^ -nan )

  assertNaN( (-1  )^  nan )
  assertNaN( (-1  )^ -nan )
  assertNaN(   nan ^   1  )
  assertNaN( (-nan)^   1  )
  assertNaN(   nan ^ - 1  )
  assertNaN( (-nan)^ - 1  )

  assertNaN(   inf ^  nan )
  assertNaN( (-inf)^  nan )
  assertNaN(   inf ^ -nan )
  assertNaN( (-inf)^ -nan )
  assertNaN(   nan ^  inf )
  assertNaN( (-nan)^  inf )
  assertNaN(   nan ^ -inf )
  assertNaN( (-nan)^ -inf )

  assertNaN(   nan ^  nan )
  assertNaN( (-nan)^  nan )
  assertNaN(   nan ^ -nan )
  assertNaN( (-nan)^ -nan )
end

-- generic complex functions

function TestLuaGmath:testCarg()
  assertEquals( carg(    0) ,  0 )
  assertEquals( carg( tiny) ,  0 )
  assertEquals( carg(  0.1) ,  0 )
  assertEquals( carg(    1) ,  0 )
  assertEquals( carg( huge) ,  0 )
  assertEquals( carg(  inf) ,  0 )
  assertEquals( carg(-   0) ,  0 )
  assertEquals( carg(-tiny) , pi )
  assertEquals( carg(- 0.1) , pi )
  assertEquals( carg(-   1) , pi )
  assertEquals( carg(-huge) , pi )
  assertEquals( carg(- inf) , pi )

  assertNaN( carg(nan) )
end

function TestLuaGmath:testReal()
  assertEquals( real(    0) ,     0 )
  assertEquals( real( tiny) ,  tiny )
  assertEquals( real(  0.1) ,   0.1 )
  assertEquals( real(    1) ,     1 )
  assertEquals( real( huge) ,  huge )
  assertEquals( real(  inf) ,   inf )
  assertEquals( real(-   0) , -   0 )
  assertEquals( real(-tiny) , -tiny )
  assertEquals( real(- 0.1) , - 0.1 )
  assertEquals( real(-   1) , -   1 )
  assertEquals( real(-huge) , -huge )
  assertEquals( real(- inf) , - inf )

  assertNaN( real(nan) )
end

function TestLuaGmath:testImag()
  assertEquals( imag(    0) , 0 )
  assertEquals( imag( tiny) , 0 )
  assertEquals( imag(  0.1) , 0 )
  assertEquals( imag(    1) , 0 )
  assertEquals( imag( huge) , 0 )
  assertEquals( imag(  inf) , 0 )
  assertEquals( imag(-   0) , 0 )
  assertEquals( imag(-tiny) , 0 )
  assertEquals( imag(- 0.1) , 0 )
  assertEquals( imag(-   1) , 0 )
  assertEquals( imag(-huge) , 0 )
  assertEquals( imag(- inf) , 0 )
  assertEquals( imag(  nan) , 0 )
end

function TestLuaGmath:testConj()
  assertEquals( conj(    0) ,     0 )
  assertEquals( conj( tiny) ,  tiny )
  assertEquals( conj(  0.1) ,   0.1 )
  assertEquals( conj(    1) ,     1 )
  assertEquals( conj( huge) ,  huge )
  assertEquals( conj(  inf) ,   inf )
  assertEquals( conj(-   0) , -   0 )
  assertEquals( conj(-tiny) , -tiny )
  assertEquals( conj(- 0.1) , - 0.1 )
  assertEquals( conj(-   1) , -   1 )
  assertEquals( conj(-huge) , -huge )
  assertEquals( conj(- inf) , - inf )

  assertNaN( conj(nan) )
end

function TestLuaGmath:testRect()
  assertEquals( rect(    0) ,     0 )
  assertEquals( rect( tiny) ,  tiny )
  assertEquals( rect(  0.1) ,   0.1 )
  assertEquals( rect(    1) ,     1 )
  assertEquals( rect( huge) ,  huge )
  assertEquals( rect(  inf) ,   inf )
  assertEquals( rect(-   0) ,     0 )
  assertEquals( rect(-tiny) ,  tiny )
  assertEquals( rect(- 0.1) ,   0.1 )
  assertEquals( rect(-   1) ,     1 )
  assertEquals( rect(-huge) ,  huge )
  assertEquals( rect(- inf) ,   inf )

  assertNaN( rect(nan) )
end

function TestLuaGmath:testPolar()
  assertEquals( polar(    0) ,    0 )
  assertEquals( polar( tiny) , tiny )
  assertEquals( polar(  0.1) ,  0.1 )
  assertEquals( polar(    1) ,    1 )
  assertEquals( polar( huge) , huge )
  assertEquals( polar(  inf) ,  inf )
  assertEquals( polar(-   0) ,    0 )
  assertEquals( polar(-tiny) , tiny )
  assertEquals( polar(- 0.1) ,  0.1 )
  assertEquals( polar(-   1) ,    1 )
  assertEquals( polar(-huge) , huge )
  assertEquals( polar(- inf) ,  inf )

  assertNaN( polar(nan) )
end

-- delegation --

function TestLuaGmath:testDelegation()
  local mock, mock2, mmock = {}, {}, {}
  setmetatable(mock , mmock)
  setmetatable(mock2, mmock)

  mock.abs    = function() return 'mock.abs   ' end       assertEquals(abs   (mock), 'mock.abs   ')
  mock.acos   = function() return 'mock.acos  ' end       assertEquals(acos  (mock), 'mock.acos  ')
  mock.asin   = function() return 'mock.asin  ' end       assertEquals(asin  (mock), 'mock.asin  ')
  mock.atan   = function() return 'mock.atan  ' end       assertEquals(atan  (mock), 'mock.atan  ')
  mock.ceil   = function() return 'mock.ceil  ' end       assertEquals(ceil  (mock), 'mock.ceil  ')
  mock.cos    = function() return 'mock.cos   ' end       assertEquals(cos   (mock), 'mock.cos   ')
  mock.cosh   = function() return 'mock.cosh  ' end       assertEquals(cosh  (mock), 'mock.cosh  ')
  mock.deg    = function() return 'mock.deg   ' end       assertEquals(deg   (mock), 'mock.deg   ')
  mock.exp    = function() return 'mock.exp   ' end       assertEquals(exp   (mock), 'mock.exp   ')
  mock.floor  = function() return 'mock.floor ' end       assertEquals(floor (mock), 'mock.floor ')
  mock.log    = function() return 'mock.log   ' end       assertEquals(log   (mock), 'mock.log   ')
  mock.log10  = function() return 'mock.log10 ' end       assertEquals(log10 (mock), 'mock.log10 ')
  mock.rad    = function() return 'mock.rad   ' end       assertEquals(rad   (mock), 'mock.rad   ')
  mock.sin    = function() return 'mock.sin   ' end       assertEquals(sin   (mock), 'mock.sin   ')
  mock.sinh   = function() return 'mock.sinh  ' end       assertEquals(sinh  (mock), 'mock.sinh  ')
  mock.sqrt   = function() return 'mock.sqrt  ' end       assertEquals(sqrt  (mock), 'mock.sqrt  ')
  mock.tan    = function() return 'mock.tan   ' end       assertEquals(tan   (mock), 'mock.tan   ')
  mock.tanh   = function() return 'mock.tanh  ' end       assertEquals(tanh  (mock), 'mock.tanh  ')
  mock.angle  = function() return 'mock.angle ' end assertEquals(angle(mock, mock2), 'mock.angle ')
  mock.max    = function() return 'mock.max   ' end       assertEquals(max   (mock), 'mock.max   ')
  mock.min    = function() return 'mock.min   ' end       assertEquals(min   (mock), 'mock.min   ')
  mock.sign   = function() return 'mock.sign  ' end       assertEquals(sign  (mock), 'mock.sign  ')
  mock.step   = function() return 'mock.step  ' end       assertEquals(step  (mock), 'mock.step  ')
  mock.sinc   = function() return 'mock.sinc  ' end       assertEquals(sinc  (mock), 'mock.sinc  ')
  mock.frac   = function() return 'mock.frac  ' end       assertEquals(frac  (mock), 'mock.frac  ')
  mock.trunc  = function() return 'mock.trunc ' end       assertEquals(trunc (mock), 'mock.trunc ')
  mock.round  = function() return 'mock.round ' end       assertEquals(round (mock), 'mock.round ')
  mock.carg   = function() return 'mock.carg  ' end       assertEquals(carg  (mock), 'mock.carg  ')
  mock.real   = function() return 'mock.real  ' end       assertEquals(real  (mock), 'mock.real  ')
  mock.imag   = function() return 'mock.imag  ' end       assertEquals(imag  (mock), 'mock.imag  ')
  mock.conj   = function() return 'mock.conj  ' end       assertEquals(conj  (mock), 'mock.conj  ')
  mock.rect   = function() return 'mock.rect  ' end       assertEquals(rect  (mock), 'mock.rect  ')
  mock.polar  = function() return 'mock.polar ' end       assertEquals(polar (mock), 'mock.polar ')

  local set = function(a,k) a[k] = (a[k] or 0) + 1 end
  local get = function(a,k) return a[k] end
  local inc = function(a,k) set(a,k) return get(a,k) end

  mmock.__unm = function(a  ) return inc(a, 'unm') end
  mmock.__add = function(a,b) return inc(a, 'add') end
  mmock.__sub = function(a,b) return inc(a, 'sub') end
  mmock.__mul = function(a,b) return inc(a, 'mul') end
  mmock.__div = function(a,b) return inc(a, 'div') end
  mmock.__mod = function(a,b) return inc(a, 'mod') end
  mmock.__pow = function(a,b) return inc(a, 'pow') end
  mmock.__eq  = function(a,b) return inc(a, 'eq')  end
  mmock.__lt  = function(a,b) return inc(a, 'lt')  end
  mmock.__le  = function(a,b) return inc(a, 'le')  end

  assertEquals(     -  mock , get(mock, 'unm'))
  assertEquals(mock +  mock2, get(mock, 'add'))
  assertEquals(mock -  mock2, get(mock, 'sub'))
  assertEquals(mock *  mock2, get(mock, 'mul'))
  assertEquals(mock /  mock2, get(mock, 'div'))
  assertEquals(mock %  mock2, get(mock, 'mod'))
  assertEquals(mock ^  mock2, get(mock, 'pow'))
  assertEquals(mock == mock2, true)
  assertEquals(mock <  mock2, true)
  assertEquals(mock <= mock2, true)

  assertEquals(mock ~= mock2, false)
  assertEquals(mock >  mock2, true)
  assertEquals(mock >= mock2, true)
  assertEquals(get(mock, 'unm'), 1)
  assertEquals(get(mock, 'add'), 1)
  assertEquals(get(mock, 'sub'), 1)
  assertEquals(get(mock, 'mul'), 1)
  assertEquals(get(mock, 'div'), 1)
  assertEquals(get(mock, 'mod'), 1)
  assertEquals(get(mock, 'pow'), 1)
  assertEquals(get(mock , 'eq'), 2)
  assertEquals(get(mock , 'lt'), 1)
  assertEquals(get(mock , 'le'), 1)
  assertEquals(get(mock2, 'lt'), 1)
  assertEquals(get(mock2, 'le'), 1)
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
