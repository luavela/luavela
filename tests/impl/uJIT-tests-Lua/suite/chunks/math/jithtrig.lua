-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local chunks_dir = arg[1] or '.'
local test_function = assert(dofile(chunks_dir .. '/test_function.lua'))

jit.opt.start("hotloop=1")

local pinf = math.huge
local ninf = -math.huge
local nan = ujit.math.nan

local inputs = { 0, 0.5, "0.5", -0.5, 1, -1, pinf, ninf, nan }

-- math.sinh
local outputs_sinh = { 0, 0.5211, 0.5211, -0.5211, 1.1752, -1.1752, pinf, ninf, nan }
test_function(inputs, outputs_sinh, math.sinh, "math.sinh")

-- math.cosh
local outputs_cosh = { 1, 1.1276, 1.1276, 1.1276, 1.5431, 1.5431, pinf, pinf, nan }
test_function(inputs, outputs_cosh, math.cosh, "math.cosh")

local outputs_tanh = { 0, 0.4621, 0.4621, -0.4621, 0.7616, -0.7616, 1, -1, nan }
test_function(inputs, outputs_tanh, math.tanh, "math.tanh")
