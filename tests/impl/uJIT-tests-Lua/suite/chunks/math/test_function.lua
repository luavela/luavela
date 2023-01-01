-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function num_compare(x, y, precision)
    if ujit.math.isfinite(x) then
        return math.abs(x - y) <= precision
    elseif ujit.math.ispinf(x) then
        return ujit.math.ispinf(y)
    elseif ujit.math.isninf(x) then
        return ujit.math.isninf(y)
    elseif ujit.math.isnan(x) then
        return ujit.math.isnan(y)
    end
    assert(false) -- unreachable
end

local function test_function(inputs, outputs, f, f_name)
    for i = 1, 100 do
        local j = i % #inputs + 1
        local output = f(inputs[j])
        local expected = outputs[j]
        if not num_compare(output, expected, 0.001) then
            error(string.format("Expected %s(%f) = %f, got %f",
                f_name, inputs[j], expected, output))
        end
    end
end

return test_function
