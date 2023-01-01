-- Tests for the semantics of ujit.table.rindex
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local rindex = ujit.table.rindex
assert(io.stdin ~= nil)
assert(io.stdout ~= nil)

do --- basic usage #1
	local payload1 = {}
	local payload2 = {value = payload1}
	local t = {
		key1 = payload2,
		key2 = {{2042, payload1}},
		key3 = {key4 = 42},
	}
	assert(rindex(t, "key1") == payload2)
	assert(rindex(t, "key1", "value") == payload1)
	assert(rindex(t, "key2", 1, 1) == 2042)
	assert(rindex(t, "key2", 1, 2) == payload1)

	assert(rindex(t, "KEY1") == nil)
	assert(rindex(t, "key1", "value", "value") == nil)
	assert(rindex(t, "key2", 2) == nil)
	assert(rindex(t, "key3", "key4", "key5") == nil)
end

do --- basic usage #2
	local t = {
		{{a = {b = "q"}}, {A = {B = "Q"}}}
	}
	assert(rindex(t, 1, 1, "a", "b") == "q")
	assert(rindex(t, 1, 2, "A", "B") == "Q")

	assert(rindex(t, 1, 1, "A", "b") == nil)
	assert(rindex(t, 1, 1, "a", "B") == nil)
	assert(rindex(t, 1, 2, "A", "b") == nil)
	assert(rindex(t, 1, 2, "a", "B") == nil)
end

do --- non-trivial cases
	local path = {1, io.stdin, io.stdout, "a", "b"}
	local t = {
		{
			[io.stdin] = {[io.stdout] = {a = {b = "q"}}}
		}
	}
	t[2] = t -- +self-referencing
	t[t] = t -- +more non-trivial keys

	assert(rindex(t, 2) == t)
	assert(rindex(t, t) == t)

	assert(rindex(t, 2, unpack(path)) == "q")
	assert(rindex(t, t, unpack(path)) == "q")

	assert(rindex(t, 2, 2, 2, 2, 2, 2, 2) == t)
	assert(rindex(t, t, t, t, t, t, t, t) == t)

	assert(rindex(t, 2, 2, 2, 2, 2, 2, 2, unpack(path)) == "q")
	assert(rindex(t, t, t, t, t, t, t, t, unpack(path)) == "q")
end

do --- recursively indexing tables with metatables
	local t = setmetatable({a = {b = "q"}}, {
		__index = function(_, _) return {b = "Q"} end,
	})

	assert(rindex(t, "a", "b") == "q")
	assert(rindex(t, "A", "b") == "Q")
end

do --- corner cases
	assert(rindex() == nil)
	assert(rindex(nil) == nil)
	assert(rindex(42) == 42)
	assert(rindex("foo") == "foo")
	assert(rindex(io.stdin) == io.stdin)
	assert(rindex({}) == nil)
end
