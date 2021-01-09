-- Tests coverage output when there are branches
-- to non-first instruction in a line
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function softerror(msg)
  print(msg)
  os.exit()
end

for _, funcName in ipairs(
    {'assertIsNumber', 'assertIsString', 'assertisFunction'}
) do
    local typeExpected = funcName:match("^assertIs([A-Z]%a*)$")
    local _ = typeExpected and typeExpected:lower()
        or softerror("bad function name '"..funcName.."' for type assertion")
end
