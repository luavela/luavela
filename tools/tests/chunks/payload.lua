-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local fname_stub = arg[1]

assert(type(fname_stub) == 'string', 'Please specify file stub')
assert(ujit.profile.available())
assert(ujit.profile.init())
local started = ujit.profile.start(50, 'leaf', fname_stub)
assert(started == true)

local function foo(i)
	return "value" .. i
end

local function unfoo()
	return nil
end

local t = {}
for i = 1, 1e6 do
	t["key" .. i] = foo(i)
	if i > 1e5 then
		t["key" .. (i - 1000)] = unfoo()
	end
end

ujit.profile.stop()
ujit.profile.terminate()
