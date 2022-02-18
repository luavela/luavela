-- Simple human-readable renderer of uJIT's memprof profile.
--
-- Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local symtab = require 'parse_symtab'

local M = {}

function M.render(events, symbols)
	local ids = {}

	for id, _ in pairs(events) do
		table.insert(ids, id)
	end

	table.sort(ids, function(id1, id2)
		return events[id1].num > events[id2].num
	end)

	for i = 1, #ids do
		local event = events[ids[i]]
		print(string.format('%s: %d\t%d\t%d',
			symtab.demangle(symbols, event.loc),
			event.num,
			event.alloc,
			event.free
		))

		local prim_loc = {}
		for _, loc in pairs(event.primary) do
			table.insert(prim_loc, symtab.demangle(symbols, loc))
		end
		if #prim_loc ~= 0 then
			table.sort(prim_loc)
			print('\tOverrides:')
			for j = 1, #prim_loc do
				print(string.format('\t\t%s', prim_loc[j]))
			end
			print('')
		end
	end
end

return M
