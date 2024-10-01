do --- trace should not return to lower frame before checking rindex return value
	local function foo(call_rindex, tab)
		if call_rindex == false then return nil end
		return ujit.table.rindex(tab, "k1", "k2")
	end

	local function bar(tab)
		local ret = foo(true, tab)
		-- compiled cycle just to link trace from foo() here
		-- otherwise it will leave bar()
		for _ = 1, 5 do end
		-- stop further recordings
		print("nyi bytecode")
		return ret
	end

	foo(false, {})
	foo(false, {})
	foo(false, {})
	foo(false, {})
	foo(false, {})
	foo(false, {}) -- record only nil branch

	local tab = {k1 = {k2 = "str1"}}
	bar(tab)
	-- side trace which records rindex and will stop in bar()
	bar(tab)
	-- trace should exit correctly to start of foo() without unwinding to bar() frame
	assert(bar({k1 = {k2 = 42}}) == 42)
end

do --- rindex return value check must not be broken
	local function foo(call_rindex, tab)
		if call_rindex == false then return nil end
		return ujit.table.rindex(tab, "k1", "k2")
	end

	local function bar(tab)
		foo(true, tab)
		for _ = 1, 5 do end
		print("nyi bytecode")
	end

	foo(false, {})
	foo(false, {})
	foo(false, {})
	foo(false, {})
	foo(false, {})
	foo(false, {})

	local tab = {k1 = {k2 = {}}}
	bar(tab)
	bar(tab)
	bar(tab) -- should exit normally
end

do --- ensure correct HOTCNT handling (i.e. applying LJ_POST_FFSPECRET while J->pc-1 points to HOTCNT)
	local function foo(tab)
		return ujit.table.rindex(tab, "k1", "k2")
	end

	local function bar(tab)
		for _ = 1, 5 do
			foo(tab)
		end
	end

	bar({k1 = {k2 = false}})
end
