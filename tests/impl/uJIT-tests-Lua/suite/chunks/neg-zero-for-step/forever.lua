-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

-- Both stock Lua 5.1 and LuaJIT/uJIT produce a forever loop:
local x = 0
for i = 1, 0, -0 do
    x = x + 1
    if x == 2000 then
        print(i)
        break
    end
end
print(x)

