-- This performance test is mostly aimed at assessing implicit tostring
-- conversion performance in case of integer arguments. However, as the payload
-- uses concatenation heavily, its performance might also be assessed.
--
-- Apart of the implicit integer-to-string conversion (done when concatenating
-- b, c and d), there is rather heavy collateral workload. Concatentation falls
-- back three times, numerous new strings are created on each iteration (not
-- allowing to re-use strings created on previous iterations).
--
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local result = ''

local function construct_string(a, b, c, d)
    return a .. "_" .. b .. "_" .. c .. "_" .. d
end

for i = 1,10000000 do
  result = construct_string('12345', i-1, i, i+1)
end

print(result)
