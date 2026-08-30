-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

package.path = 'tests/cinterp/?.lua;' .. package.path

local assert_ends_with = require 'testutil'.assert_ends_with

do -- all loop args are numbers
	local foo  = function()
		local counter = 0
		for i = 1, 10, 1 do
			counter = counter + 1
		end
		return counter
	end

	local ret = cinterpcall(foo)
	assert(ret == 10)
end

do -- init value is string, trigger argument coercion
	local foo  = function()
		local counter = 0
		for i = '1', 10, 1 do
			counter = counter + 1
		end
		return counter
	end

	local ret = cinterpcall(foo)
	assert(ret == 10)
end

do -- stop value is string, trigger argument coercion
	local foo  = function()
		local counter = 0
		for i = 1, '10', 1 do
			counter = counter + 1
		end
		return counter
	end

	local ret = cinterpcall(foo)
	assert(ret == 10)
end

do -- increment value is string, trigger argument coercion
	local foo  = function()
		local counter = 0
		for i = 1, 10, '1' do
			counter = counter + 1
		end
		return counter
	end

	local ret = cinterpcall(foo)
	assert(ret == 10)
end

do -- argument (table) can't be coerced
	local foo = function()
		local counter = 0
		for i = 1, 10, {} do
			counter = counter + 1
		end
		return counter
	end

	local caller = function()
		local ok, msg = pcall(foo)
		return ok, msg
	end

	local ok, msg = cinterpcall(caller)
	assert(ok == false)
	assert_ends_with(msg, "'for' step must be a number")
end
