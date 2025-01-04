-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

--
-- LuaJIT: demonstracni priklad cislo 53.
--
-- Test JITu - volani funkce
--



-- deklarace a inicializace lokalnich promennych
local x = 0
local y = 0



-- funkce, ktera se bude JITovat
local function adder()
    x = x + 1
    y = y + 1
end



-- programova smycka, ktera se neJITuje
for _ = 1,50 do
    adder()
end
print("1")

-- programova smycka, ktera se neJITuje
for _ = 1,50 do
    adder()
end
print("2")

-- programova smycka, ktera se neJITuje
for _ = 1,50 do
    adder()
end
print("3")

print(x,y)



--
-- Finito.
--

