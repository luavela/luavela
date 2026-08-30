-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

package.path = 'tests/cinterp/?.lua;' .. package.path
local tu = require 'testutil'

local cinterpcall = ujit.debug.cinterpcall

do -- string.len("abc")
	local string_len_string = function()
		local str = "abc"
		return string.len(str)
	end

	local ret = cinterpcall(string_len_string)
	assert(ret == 3)
end

do -- string.byte("abc")
	local string_byte_string = function()
		local str = "abc"
		return string.byte(str)
	end

	local ret = cinterpcall(string_byte_string)
	assert(ret == 97)
end

do -- string.byte("á")
	-- "á" is "0xC3 0xA1" in UTF-8.
	-- string.byte will return the first byte, 0xC3
	local string_byte_utf8 = function()
		return string.byte("á")
	end

	local ret = cinterpcall(string_byte_utf8)
	assert(ret == 0xC3)
end

do -- string.byte("東京")
	-- "東" is "0xE6 0x9D 0xB1" in UTF-8.
	-- string.byte will return the first byte, 0xE6
	local string_byte_utf8_complex = function()
		return string.byte("東京")
	end

	local ret = cinterpcall(string_byte_utf8_complex)
	assert(ret == 0xE6)
end

do -- string.byte("")
	local string_byte_empty_string = function()
		local str = ""
		return string.byte(str)
	end

	local ret = cinterpcall(string_byte_empty_string)
	assert(ret == nil)
end

do -- string.byte("abc", 1, 3) - calls C fallback actually
	local string_byte_fallback = function()
		local str = "abc"
		local ret = {string.byte(str, 1, 3)}
		return ret
	end

	local ret = cinterpcall(string_byte_fallback)
	assert(#ret == 3)
	assert(ret[1] == 97)
	assert(ret[2] == 98)
	assert(ret[3] == 99)
end

do -- string.byte(number)
	local string_byte_number = function()
		return string.byte(4) -- gets implicitly converted to string "4"
	end

	local ret = cinterpcall(string_byte_number)
	assert(ret == 52)
end

do -- string.char(number)
	local string_char_number = function()
		return string.char(80)
	end

	local ret = cinterpcall(string_char_number)
	assert(ret == "P")
end

do -- string.char(number, number) - C fallback
	local string_char_number = function()
		return string.char(80, 81)
	end

	local ret = cinterpcall(string_char_number)
	assert(ret == "PQ")
end

do -- string.char() - OK, returns empty string
	local string_char_no_value = function()
		return string.char()
	end

	local ret = cinterpcall(string_char_no_value)
	assert(ret == "")
end

do -- string.sub(string, i, j), j > i
	local string_sub_two_args = function()
		return string.sub("hello", 2, 5)
	end

	local ret = cinterpcall(string_sub_two_args)
	assert(ret == "ello")
end

do -- string.sub(string, i, j), i > j - should return an empty string
	local string_sub_start_above_end = function()
		return string.sub("hello", 3, 1)
	end

	local ret = cinterpcall(string_sub_start_above_end)
	assert(ret == "")
end

do -- string.sub(string, i, j), j is negative
	local string_sub_negative_end = function()
		return string.sub("hello", 2, -2)
	end

	local ret = cinterpcall(string_sub_negative_end)
	assert(ret == "ell")
end

do -- string.sub(string, i, j), j == string length
	local string_sub_negative_end = function()
		return string.sub("hello", 2, 5)
	end

	local ret = cinterpcall(string_sub_negative_end)
	assert(ret == "ello")
end

do -- string.sub(string, i, j), i == 0 -> i = 1
	local string_sub_zero_start = function()
		return string.sub("hello", 0, 4)
	end

	local ret = cinterpcall(string_sub_zero_start)
	assert(ret == "hell")
end

do -- string.sub(string, i, j), i < 0 -> return empty string
	local string_sub_below_zero_start = function()
		return string.sub("hello", -1, 4)
	end

	local ret = cinterpcall(string_sub_below_zero_start)
	assert(ret == "")
end

do -- string.sub(string, i), -> j is set to -1
	local string_sub_no_end_arg = function()
		return string.sub("hello", 2)
	end

	local ret = cinterpcall(string_sub_no_end_arg)
	assert(ret == "ello")
end

do -- string.sub(string, i), i < 0
	local string_sub_no_end_arg_out_of_bounds = function()
		return string.sub("hello", -30)
	end

	local ret = cinterpcall(string_sub_no_end_arg_out_of_bounds)
	assert(ret == "hello")
end

do -- string.sub(string, i), i > len(string)
	local string_sub_no_end_arg_out_of_bounds2 = function()
		return string.sub("hello", 30)
	end

	local ret = cinterpcall(string_sub_no_end_arg_out_of_bounds2)
	assert(ret == "")
end

do -- string.sub(string, i), -> i < 0
	local string_sub_no_end_arg_start_negative = function()
		return string.sub("hello", -2)
	end

	local ret = cinterpcall(string_sub_no_end_arg_start_negative)
	assert(ret == "lo")
end

do -- string.sub(string, i), -> i < 0, j > 0
	local string_sub_start_negative_end_positive = function()
		return string.sub("hello", -4, 3)
	end

	local ret = cinterpcall(string_sub_start_negative_end_positive)
	assert(ret == "el")
end

do -- string.sub(string, i), -> i == 0 (=> i = 1), j > 0
	local string_sub_start_zero_end_positive = function()
		return string.sub("hello", 0, 3)
	end

	local ret = cinterpcall(string_sub_start_zero_end_positive)
	assert(ret == "hel")
end

do -- string.sub(string, i), -> i < 0, j < 0
	local string_sub_start_negative_end_negative = function()
		return string.sub("hello", -4, -2)
	end

	local ret = cinterpcall(string_sub_start_negative_end_negative)
	assert(ret == "ell")
end

do -- string.sub(string, i, j, extra args), -> extra args are ignored
	local string_sub_extra_args = function()
		return string.sub("hello", 2, 4, 5)
	end

	local ret = cinterpcall(string_sub_extra_args)
	assert(ret == "ell")
end

do -- string.rep(string, 2), -> string has >1 chars - C fallback
	local string_rep_no_end_arg = function()
		return string.rep("ha", 3)
	end

	local ret = cinterpcall(string_rep_no_end_arg)
	assert(ret == "hahaha")
end

do -- string.rep(string, 2), -> string has >1 chars - fallback
	local string_rep_simple = function()
		return string.rep("ha", 3)
	end

	local ret = cinterpcall(string_rep_simple)
	assert(ret == "hahaha")
end

do -- string.rep(string, 0), -> count == 0 -> empty string
	local string_rep_zero = function()
		return string.rep("ha", 0)
	end

	local ret = cinterpcall(string_rep_zero)
	assert(ret == "")
end

do -- string.rep(string, 0), -> count < 0 -> empty string
	local string_rep_negative = function()
		return string.rep("ha", -2)
	end

	local ret = cinterpcall(string_rep_negative)
	assert(ret == "")
end

do -- string.rep(string, n), -> input string is empty
	local string_rep_empty = function()
		return string.rep("", 5)
	end

	local ret = cinterpcall(string_rep_empty)
	assert(ret == "")
end

do -- string.rep(string, 0), -> count < 0 -> empty string
	local string_rep_negative = function()
		return string.rep("A", 5)
	end

	local ret = cinterpcall(string_rep_negative)
	assert(ret == "AAAAA")
end

do -- string.reverse(string), non-empty string
	local string_reverse = function()
		return string.reverse("Hello")
	end

	local ret = cinterpcall(string_reverse)
	assert(ret == "olleH")
end

do -- string.reverse(string), #string == 1
	local string_reverse_one_char = function()
		return string.reverse("H")
	end

	local ret = cinterpcall(string_reverse_one_char)
	assert(ret == "H")
end

do -- string.reverse(""), empty string
	local string_reverse_empty = function()
		return string.reverse("")
	end

	local ret = cinterpcall(string_reverse_empty)
	assert(ret == "")
end

do -- string.lower("HELLO42") == "hello42"
	local string_lower_all_uppercase = function()
		return string.lower("HELLO42")
	end

	local ret = cinterpcall(string_lower_all_uppercase)
	assert(ret == "hello42")
end

do -- string.lower("hello42") == "hello"
	local string_all_lowercase = function()
		return string.lower("hello42")
	end

	local ret = cinterpcall(string_all_lowercase)
	assert(ret == "hello42")
end

do -- string.lower("HeLLo42") == "hello42"
	local string_lower_mixed_case = function()
		return string.lower("HELLO42")
	end

	local ret = cinterpcall(string_lower_mixed_case)
	assert(ret == "hello42")
end

do -- string.lower("") == ""
	local string_lower_empty = function()
		return string.lower("")
	end

	local ret = cinterpcall(string_lower_empty)
	assert(ret == "")
end

do -- string.upper("hello42") == "HELLO42"
	local string_upper_all_lowercase = function()
		return string.upper("hello42")
	end

	local ret = cinterpcall(string_upper_all_lowercase)
	assert(ret == "HELLO42")
end

do -- string.upper("HELLO42") == "HELLO42"
	local string_all_uppercase = function()
		return string.upper("HELLO42")
	end

	local ret = cinterpcall(string_all_uppercase)
	assert(ret == "HELLO42")
end

do -- string.upper("HeLLo42") == "HELLO42"
	local string_upper_mixed_case = function()
		return string.upper("HELLO42")
	end

	local ret = cinterpcall(string_upper_mixed_case)
	assert(ret == "HELLO42")
end

do -- string.upper("") == ""
	local string_upper_empty = function()
		return string.upper("")
	end

	local ret = cinterpcall(string_upper_empty)
	assert(ret == "")
end
