-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local cinterpcall = ujit.debug.cinterpcall

do -- a + b
	local add_meta = function()
		local mt = {
			__add = function(a, b)
				return { n = a.n + b.n }
			end
		}

		local a = { n = 7 }
		local b = { n = 3 }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a + b
		return res
	end

	local ret = cinterpcall(add_meta)
	assert(ret.n == (7 + 3))
end

do -- a + b with callable object stored in __add metamethod
	local add_meta_callable = function()
		local callable = setmetatable({}, {
			__call = function(tab, a, b) return a.key + b.key end
		})

		local t = {key = 42}
		setmetatable(t, {
			__add = callable
		})

		return t + t
	end

	assert(cinterpcall(add_meta_callable) == 42 + 42)
end

do -- a - b
	local sub_meta = function()
		local mt = {
			__sub = function(a, b)
				return { n = a.n - b.n }
			end
		}

		local a = { n = 7 }
		local b = { n = 3 }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a - b
		return res
	end

	local ret = cinterpcall(sub_meta)
	assert(ret.n == (7 - 3))
end

do -- a * b
	local mul_meta = function()
		local mt = {
			__mul = function(a, b)
				return { n = a.n * b.n }
			end
		}

		local a = { n = 7 }
		local b = { n = 3 }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a * b
		return res
	end

	local ret = cinterpcall(mul_meta)
	assert(ret.n == (7 * 3))
end

do -- a / b
	local div_meta = function()
		local mt = {
			__div = function(a, b)
				return { n = a.n / b.n }
			end
		}

		local a = { n = 7 }
		local b = { n = 3 }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a / b
		return res
	end

	local ret = cinterpcall(div_meta)
	assert(ret.n == (7 / 3))
end

do -- a % b
	local mod_meta = function()
		local mt = {
			__mod = function(a, b)
				return { n = a.n % b.n }
			end
		}

		local a = { n = 7 }
		local b = { n = 3 }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a % b
		return res
	end

	local ret = cinterpcall(mod_meta)
	assert(ret.n == (7 % 3))
end

do -- a ^ b
	local pow_meta = function()
		local mt = {
			__pow = function(a, b)
				return { n = a.n ^ b.n }
			end
		}

		local a = { n = 7 }
		local b = { n = 3 }

		setmetatable(a, mt)
		setmetatable(b, mt)

		local res = a ^ b
		return res
	end

	local ret = cinterpcall(pow_meta)
	assert(ret.n == (7 ^ 3))
end
