-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function test(parameters)
  for i=1,5 do
    if (not parameters.x)   -- Trying to put BC_COVERG between test and jump
      and parameters.x > i  -- instructions. Which is undefined behavior.
    then
      parameters.x = i
    end
  end
  return parameters
end
local parameters = {x = 0}
local _ = test(parameters)

