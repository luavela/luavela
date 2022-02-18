-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local function shallowcopy(t)
	local result = {}
	for k, v in pairs(t) do
		result[k] = v
	end
	return result
end

local function keys(set)
	if not set or not next(set) then return {} end

	local list = {}
	for k, _ in pairs(set) do
		table.insert(list, k)
	end
	return list
end

local function values(set)
	if not set or not next(set) then return {} end

	local list = {}
	for _, v in pairs(set) do
		table.insert(list, v)
	end
	return list
end

local function toset(list)
	if not list or #list == 0 then return {} end
	local set = {}
	for _, v in ipairs(list) do
		set[v] = true
	end
	return set
end

local function size(t)
	local count = 0
	for _, _ in pairs(t) do
		count = count + 1
	end
	return count
end

return {
	shallowcopy = shallowcopy,
	keys = keys,
	values = values,
	toset = toset,
	size = size
}
