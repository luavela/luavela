-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- a .. b
	local concat_meta = function()
		local mt = {
			__concat = function(a, b)
				return { s = a.s .. b.s }
			end
		}

		local a = { s = "hello" }
		local b = { s = "world" }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a .. b
		return res
	end

	local ret = cinterpcall(concat_meta)
	assert(ret.s == ("hello" .. "world"))
end

do -- a .. b .. c .. d
	local concat_meta_multiple = function()
		local mt = {
			__concat = function(a, b)
				return { s = a.s .. b.s }
			end
		}

		local a = { s = "hello" }
		local b = { s = "world" }
		local c = { s = "okay" }
		local d = { s = "bye" }

		setmetatable(a, mt)
		setmetatable(b, mt)
		setmetatable(c, mt)
		setmetatable(d, mt)

		local res = a .. b .. c .. d
		return res
	end

	local ret = cinterpcall(concat_meta_multiple)
	assert(ret.s == ("hello" .. "world" .. "okay" .. "bye"))
end

-- complex metatable which handles one of the arguments being a string
local mt_complex = {}
mt_complex.__concat = function(a, b)
	if type(a) == "string" then
		local res = { s = a .. b.s }
		setmetatable(res, mt_complex)
		return res
	elseif type(b) == "string" then
		local res = { s = a.s .. b }
		setmetatable(res, mt_complex)
		return res
	end

	local res = { s = a.s .. b.s }
	setmetatable(res, mt_complex)
	return res
end

do -- a .. b .. c .. d, (b, c - string, a, d - table with mt)
	local concat_meta_multiple_strings = function()
		local a = { s = "hello" }
		local b = "world"
		local c = "okay"
		local d = { s = "bye" }

		setmetatable(a, mt_complex)
		setmetatable(d, mt_complex)

		local res = a .. b .. c .. d
		return res
	end

	local ret = cinterpcall(concat_meta_multiple_strings)
	assert(ret.s == ("hello" .. "world" .. "okay" .. "bye"))
end

do -- a .. b .. c .. d, (a, b, c - string, d - table with mt)
	local concat_meta_multiple_strings3 = function()
		local a = "hello"
		local b = "world"
		local c = "okay"
		local d = { s = "bye" }

		setmetatable(d, mt_complex)

		local res = a .. b .. c .. d
		return res
	end

	local ret = cinterpcall(concat_meta_multiple_strings3)
	assert(ret.s == ("hello" .. "world" .. "okay" .. "bye"))
end

do -- check correctness of cont_cat continuation handler logic
	local cont_cat_handler_test = function()
		local val, unused = setmetatable({}, {__concat =
			function(unused1, unused2)
				return "some_string"
			end
		})

		return "prefix_"..val..val..val
	end

	assert(cinterpcall(cont_cat_handler_test) == "prefix_some_string")
end
