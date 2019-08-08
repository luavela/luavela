-- Tests here are dedicated to verify that ujit library implementation of
-- variuos functions from 'table' module outperforms naive implementation.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local utils = require("utils")

local comparators = utils.create_comparators({
	keys = ujit.table.keys,
	shallowcopy = ujit.table.shallowcopy,
	size = ujit.table.size,
	toset = ujit.table.toset,
	values = ujit.table.values,
})

print("Benchmarking:\n")

for _, ttype in pairs(utils.TABLE_TYPES)
do
	print("-- type: " .. ttype)
	for _, l in pairs({10, 100, 1000, 100000})
	do
		local t = utils.generate_table(l, ttype)
		print("   length: " .. l)
		for _, comp in ipairs(comparators)
		do
			local t_diff, t_ratio = comp.get_diff_and_ratio(t)

			print("    " .. comp.compared_name .. ": diff: " .. t_diff .. " ratio: " .. t_ratio)
			-- TODO: These should be investigated later
			-- assert(t_diff >= 0.0)
			-- assert(t_ratio >= 1.0)
		end
		print()
	end
	print()
end

