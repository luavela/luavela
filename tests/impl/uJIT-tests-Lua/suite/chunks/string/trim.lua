-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function test_trim(str, expected)
    local actual = ujit.string.trim(str)
    if actual ~= expected then
        error(string.format('\nTest failed: trim("%s")\n\tExpected: "%s"\n\tActual: "%s"',
                str, expected, actual), 2)
    end
end

test_trim("", "")
test_trim(" ", "")
test_trim("    \t   \n   \v\f   ", "")
test_trim("foo", "foo")
test_trim("foo bar", "foo bar")
test_trim("     foo", "foo")
test_trim("foo     ", "foo")
test_trim(" \t\n\v  \f\r  foo   bar  \t\v \f  ", "foo   bar")
test_trim("  foo\0bar  ", "foo\0bar")
test_trim(" \t\n\0\0\0foo\0bar\0\0\v \n  ", "\0\0\0foo\0bar\0\0")

-- test calling with extra args (they should be ignored)
assert(ujit.string.trim("   ", 42, "hello") == "")
assert(ujit.string.trim("    foo     ", 42, "hello") == "foo")
