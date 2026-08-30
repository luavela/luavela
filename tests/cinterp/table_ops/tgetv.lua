-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- argument is not a table
	local foo = function()
		local str = 'not a table'
		debug.setmetatable(str, {__index = function(t, k) return 42 end})
		local k = 'key'
		return str[k]
	end

	assert(cinterpcall(foo) == 42)
end

do -- cases when key is string
	do -- value is present, no __index
		local foo = function()
			local tab = {k1 = 'test'}
			local k = 'k1'
			return tab[k]
		end

		assert(cinterpcall(foo) == 'test')
	end

	do -- key is present, has __index (ignored)
		local foo = function()
			local tab = {k1 = 'test'}
			setmetatable(tab, {__index = function(t, k) return 'override' end})
			local k = 'k1'
			return tab[k]
		end

		assert(cinterpcall(foo) == 'test')
	end

	do -- key is present and nil, no __index
		local foo = function()
			local tab = {k1 = nil}
			local k = 'k1'
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- key is present and nil, has __index
		local foo = function()
			local tab = {k1 = nil}
			setmetatable(tab, {__index = function(t, k) return 'override' end})
			local k = 'k1'
			return tab[k]
		end

		assert(cinterpcall(foo) == 'override')
	end

	do -- no key, no __index
		local foo = function()
			local tab = {k1 = nil}
			local k = 'k2'
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- no key, has __index
		local foo = function()
			local tab = {k1 = nil}
			setmetatable(tab, {__index = function(t, k) return false end})
			local k = 'k2'
			return tab[k]
		end

		assert(cinterpcall(foo) == false)
	end
end

do -- key is a number
	do -- not integer key, no __index
		local foo = function()
			local tab = {1, 2, 3}
			local k = 2.5
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- not integer key, has __index
		local foo = function()
			local tab = {1, 2, 3}
			setmetatable(tab, {__index = function(t, k) return k + 0.5 end})
			local k = 2.5
			return tab[k]
		end

		assert(cinterpcall(foo) == 3)
	end

	do -- integer key, index exceeds array part, no __index
		local foo = function()
			local tab = {11, 22, 33} -- array size = 5
			local k = 100
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- integer key, index exceeds array part, has __index
		local foo = function()
			local tab = {11, 22, 33} -- array size = 5
			setmetatable(tab, {__index = function(t, k) return true end})
			local k = 100
			return tab[k]
		end

		assert(cinterpcall(foo) == true)
	end

	do -- integer key, fits in array part, not nil, no __index
		local foo = function()
			local tab = {11, 22, 33} -- array size = 5
			local k = 3
			return tab[k]
		end

		assert(cinterpcall(foo) == 33)
	end

	do -- integer key, fits in array part, not nil, has __index
		local foo = function()
			local tab = {11, 22, 33} -- array size = 5
			setmetatable(tab, {__index = function(t, k) return true end})
			local k = 1
			return tab[k]
		end

		assert(cinterpcall(foo) == 11)
	end

	do -- integer key, fits in array part, nil, no __index (var 1)
		local foo = function()
			local tab = {nil, 22, 33}
			local k = 4
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- integer key, fits in array part, nil, has __index (var 1)
		local foo = function()
			local tab = {nil, 22, 33}
			setmetatable(tab, {__index = function(t, k) return 42 end})
			local k = 4
			return tab[k]
		end

		assert(cinterpcall(foo) == 42)
	end

	do -- integer key, fits in array part, nil, no __index (var 2)
		local foo = function()
			local tab = {nil, 22, 33}
			local k = 1
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- integer key, fits in array part, nil, has __index (var 2)
		local foo = function()
			local tab = {nil, 22, 33}
			setmetatable(tab, {__index = function(t, k) return 2024 end})
			local k = 1
			return tab[k]
		end

		assert(cinterpcall(foo) == 2024)
	end

	do -- 0 index, no metatable
		local foo = function()
			local tab = {11, 22, 33}
			local k = 0.0
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
	end

	do -- 0 index, with metatable
		local foo = function()
			local tab = {11, 22, 33}
			setmetatable(tab, {__index = function(t, k) return 2024 end})
			local k = 0.0
			return tab[k]
		end

		assert(cinterpcall(foo) == 2024)
	end

	do -- set 0.0 idx, read -0.0 idx
		local foo = function()
			local tab = {11, 22, 33}
			local k1 = 0.0
			tab[k1] = 42
			local k2 = -0.0
			return tab[k2]
		end

		assert(cinterpcall(foo) == 42)
	end

	do -- set -0.0 idx, read 0.0 idx
		local foo = function()
			local tab = {11, 22, 33}
			local k1 = -0.0
			tab[k1] = 42
			local k2 = 0.0
			return tab[k2]
		end

		assert(cinterpcall(foo) == 42)
	end
end

do -- key is not string or number, no __index
		local foo = function()
			local tab = {k1 = 'test'}
			local k = {}
			return tab[k]
		end

		assert(cinterpcall(foo) == nil)
end

do -- key is not string or number, has __index
		local foo = function()
			local tab = {k1 = 'test'}
			setmetatable(tab, {__index = function(t, k) return 2024 end})
			local k = {}
			return tab[k]
		end

		assert(cinterpcall(foo) == 2024)
end
