-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function get_tokens(str, sep)
    local result = {}
    for token in ujit.string.split(str, sep) do
        table.insert(result, token)
    end
    return result
end

local function arrays_equal(t1, t2)
    if #t1 ~= #t2 then
        return false
    end

    for i, v in ipairs(t1) do
        if t2[i] ~= v then
            return false
        end
    end
    return true
end

local function array_to_string(arr)
    if #arr == 0 then
        return "{}"
    else
        return '{ "' .. table.concat(arr, '", "') .. '" }'
    end
end

local function test(str, sep, expected)
    local tokens = get_tokens(str, sep)
    if not arrays_equal(tokens, expected) then
        local tokens_str = array_to_string(tokens)
        local expected_str = array_to_string(expected)
        error(string.format('\nTest failed: split("%s", "%s")\n\tExpected: %s\n\tActual: %s',
                str, sep, expected_str , tokens_str), 2)
    end
end

test("", ",", { "" })
test("a", ",", { "a" })
test("abc", ",", { "abc" })
test(",,,", ",", { "", "", "", "" })
test("ab,,bc", ",", { "ab", "", "bc" })
test("ab,,,bc", ",", { "ab", "", "", "bc" })
test(",ab,,,bc", ",", { "", "ab", "", "", "bc" })
test("ab,bc,cd", ",", { "ab", "bc", "cd" })

-- separators with length > 1
test("==", "==", { "", "" })
test("ab==bc==cd", "==", { "ab", "bc", "cd" })

-- separators at end and beginning
test("ab==bc==cd==", "==", { "ab", "bc", "cd", "" })
test("ab==bc==cd====", "==", { "ab", "bc", "cd", "", "" })
test("==ab==bc==cd==", "==", { "", "ab", "bc", "cd", "" })
-- separator at the end partly matched
test("==ab==bc==cd=", "==", { "", "ab", "bc", "cd=" })

-- \0 handling -- doesn't work with gmatch implementation
test("a\0b\0c", "\0", { "a", "b", "c" })
test("a\0b\0", "\0", { "a", "b", "" })
test("a\0b,b\0c,c\0d", ",", { "a\0b", "b\0c", "c\0d" })

-- test that empty separator throws an error
local ok, msg = pcall(ujit.string.split, "abc", "")
assert(ok == false)
assert(msg:find("empty separator"))
