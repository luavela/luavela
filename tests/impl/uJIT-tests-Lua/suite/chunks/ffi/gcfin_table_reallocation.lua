-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local ffi = require('ffi')
pcall(jit.off)

ffi.cdef([[
	struct old {int a;};
	struct new {int a;};
]])

local anchor = 0
local function create_fin(i)
	return function()
		anchor = anchor + i
	end
end

-- Start GC and collect the existing garbage.
collectgarbage('collect')
-- Make GC collect as aggressive as possible.
collectgarbage('setstepmul', 100)

-- Create and mark GCcdata object as dead.
-- As a result, ctype_state->finalizer->hmask is 1 (i.e. 2 fields):
-- * __mode = "k";
-- * finalizer for old GCcdata.
local old_type = ffi.metatype('struct old', { __gc = create_fin(1) })
local _ = old_type(1)
_ = nil

-- Reset the GC related counters.
ujit.getmetrics()
-- Finish all GCSpropagate steps and GCSatomic step.
while ujit.getmetrics().gc_steps_sweepstring == 0 do
	collectgarbage('step')
end

-- Freeze GC at GCSsweepstring phase.
collectgarbage('stop')
-- Create another GCcdata object.
-- As a result, ctype_state->finalizer->hmask is 3 (i.e. 4 fields):
-- * __mode = "k";
-- * finalizer for the old (and dead) GCcdata;
-- * finalizer for the new GCcdata.
-- Hence, reallocation is required
local new_type = ffi.metatype('struct new', { __gc = create_fin(1) })
-- Reallocation occurs and the dead GCcdata is copied to the new hpart.
_ = new_type(1)
