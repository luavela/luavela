-- Tests string.find implementation.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function eq_array(got, expected)
  assert(type(got) == "table")
  assert(type(expected) == "table")
  assert(#got == #expected)
  for i = 1, #expected do
    assert(expected[i] == got[i])
  end
end

local function is_nil(got)
  assert(got == nil)
end

-- basic usage
eq_array({string.find("Where's Wally?", "Wally")}, {9, 13})
eq_array({string.find("Whe\0re's Wal\0ly?", "Wal\0ly")}, {10, 15})
is_nil(string.find("Where's Wally?", "Waldo"))

-- with `init` argument
eq_array({string.find("Where's Wally?", "Wally", nil)}, {9, 13})
is_nil(string.find("Where's Wally?", "Wally", -2))
eq_array({string.find("Where's Wally?", "Wally", -7)}, {9, 13})
eq_array({string.find("Where's Wally?", "Wally", -256)}, {9, 13})
eq_array({string.find("Where's Wally?", "Wally", 2)}, {9, 13})
is_nil(string.find("Where's Wally?", "Wally", 10))

-- empty pattern cases
is_nil(string.find("Where's Wally?", "", 256)) -- lua5.1 returns 15, 14 here
eq_array({string.find("Where's Wally?", "")} , {1, 0})
eq_array({string.find("Where's Wally?", "", 3)} , {3, 2})

-- pattern matching and `plain` argument
eq_array({string.find("Where's W%ally?", "W%ally", nil, true)}, {9, 14})
is_nil(string.find("Where's W%ally?", "W%ally"))
is_nil(string.find("Where's W%ally?", "W%ally", nil, nil))
is_nil(string.find("Where's W%ally?", "W%ally", nil, false))
-- any non-falsy counts at 4th parameter (according to vanilla lua5.1)
eq_array({string.find("Where's W%ally?", "W%ally", -7, {})}, {9, 14})

-- pattern matching with captures
eq_array({string.find("Where's Wally?", "W(%a+)y")}, {9, 13, "all"})
eq_array({string.find("Where's Wally?", "W(%a+)y", 7)}, {9, 13, "all"})
