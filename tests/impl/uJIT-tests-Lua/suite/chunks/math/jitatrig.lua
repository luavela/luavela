-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local chunks_dir = arg[1] or '.'
local test_function = assert(dofile(chunks_dir .. '/test_function.lua'))

jit.opt.start("hotloop=1")

local pinf = math.huge
local ninf = -math.huge
local nan = ujit.math.nan

local inputs = { 0, 0.5, "0.5", -0.5, 1, -1, pinf, ninf, nan }

-- math.asin
local outputs_asin = { 0, 0.5236, 0.5236, -0.5236, 1.5708, -1.570, nan, nan, nan }
test_function(inputs, outputs_asin, math.asin, "math.asin")

-- math.acos
local outputs_acos = { 1.5708, 1.0472, 1.0472, 2.0944, 0, 3.1416, nan, nan, nan }
test_function(inputs, outputs_acos, math.acos, "math.acos")

-- math.atan
local outputs_atan = { 0, 0.46365, 0.46365, -0.46365, 0.7854, -0.7854, 1.5708, -1.5708, nan }
test_function(inputs, outputs_atan, math.atan, "math.atan")
