-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local a = 0

::lable1::
a = a + 1
if (a > 10) then
    goto lable2
end

goto lable1

::lable2::
print(a)
