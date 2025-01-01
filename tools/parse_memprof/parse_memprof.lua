-- Parser of uJIT's memprof binary stream.
-- The format spec can be found in src/profile/uj_memprof.c.
--
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local bit    = require 'bit'
local band   = bit.band
local lshift = bit.lshift

local symtab   = require 'parse_symtab'
local demangle = symtab.demangle

local string_format = string.format
local table_unpack = table.unpack

local UJM_MAGIC           = 'ujm'
local UJM_CURRENT_VERSION = 2

local UJM_TIMESTAMP       = 0x40
local UJM_TIMESTAMP_INV   = 0xbf -- (uint8_t)~UJM_TIMESTAMP
local UJM_EPILOGUE_HEADER = 0x80

local AEVENT_ALLOC    = 1
local AEVENT_FREE     = 2
local AEVENT_REALLOC  = 3

local ASOURCE_INT     = lshift(1, 2)
local ASOURCE_LFUNC   = lshift(2, 2)
local ASOURCE_CFUNC   = lshift(3, 2)

local M = {}

local function new_event(loc)
	return {
		loc     = loc,
		num     = 0,
		free    = 0,
		alloc   = 0,
		primary = {},
	}
end

local function link_to_previous(heap, e, oaddr)
	-- memory at oaddr was allocated before we started tracking:
	if heap[oaddr] then
		e.primary[heap[oaddr][2]] = heap[oaddr][3]
	end
end

local function parse_location(reader, asource)
	if asource == ASOURCE_INT then
		return 'f0l0', {
			addr = 0, -- INTERNAL
			line = 0,
		}
	elseif asource == ASOURCE_CFUNC then
		local addr = reader:read_uleb128()
		return string_format('f%#xl%d', addr, 0), {
			addr = addr,
			line = 0,
		}
	elseif asource == ASOURCE_LFUNC then
		local addr = reader:read_uleb128()
		local line = reader:read_uleb128()
		return string_format('f%#xl%d', addr, line), {
			addr = addr,
			line = line,
		}
	end
	error('Unknown asource ' .. asource)
end

-- Currently dumping heap snapshots is "interleaved" with the parsing
-- of the entire memprof. This is somewhat awkward as it creates coupling
-- between retrieving and rendering data. At the same time, this a work-around
-- to reduce memory footprint while running the tool: we report "on the go"
-- and do not keep unused data in memory.
-- TODO: Decouple retrieving data from rendering, keeping memory overhead low.
local function dump_heap_snap(events, symbols, snap_interval)
	local heap = events.heap
	local snap = {}
	local ids  = {}

	events.sec = events.sec + 1
	if events.sec % snap_interval ~= 0 then
		return
	end

	for addr, _ in pairs(heap) do
		local size, id, loc = table_unpack(heap[addr])

		if not snap[id] then
			snap[id] = {size = 0, loc = loc}
			table.insert(ids, id)
		end

		snap[id].size = snap[id].size + size
	end

	table.sort(ids, function (id1, id2)
		return snap[id1].size > snap[id2].size
	end)

	print(string_format(
	      '===== HEAP SNAPSHOT, %d seconds passed', events.sec))
	for i = 1, #ids do
		local id = ids[i]
		print(string_format('%s holds %d bytes',
			demangle(symbols, snap[id].loc), snap[id].size
		))
	end
	print('')
end

local function parse_alloc(reader, asource, events, heap)
	local id, loc = parse_location(reader, asource)

	local naddr = reader:read_uleb128()
	local nsize = reader:read_uleb128()

	if not events[id] then
		events[id] = new_event(loc)
	end
	local e = events[id]
	e.num   = e.num + 1
	e.alloc = e.alloc + nsize

	heap[naddr] = {nsize, id, loc}
end

local function parse_realloc(reader, asource, events, heap)
	local id, loc = parse_location(reader, asource)

	local oaddr = reader:read_uleb128()
	local osize = reader:read_uleb128()
	local naddr = reader:read_uleb128()
	local nsize = reader:read_uleb128()

	if not events[id] then
		events[id] = new_event(loc)
	end
	local e = events[id]
	e.num   = e.num + 1
	e.free  = e.free + osize
	e.alloc = e.alloc + nsize

	link_to_previous(heap, e, oaddr)

	heap[oaddr] = nil
	heap[naddr] = {nsize, id, loc}
end

local function parse_free(reader, asource, events, heap)
	local id, loc = parse_location(reader, asource)

	local oaddr = reader:read_uleb128()
	local osize = reader:read_uleb128()

	if not events[id] then
		events[id] = new_event(loc)
	end
	local e = events[id]
	e.num   = e.num + 1
	e.free  = e.free + osize

	link_to_previous(heap, e, oaddr)

	heap[oaddr] = nil
end

local parsers = {
	[AEVENT_ALLOC]   = {evname =   'alloc', parse = parse_alloc},
	[AEVENT_REALLOC] = {evname = 'realloc', parse = parse_realloc},
	[AEVENT_FREE]    = {evname =    'free', parse = parse_free},
}

local function ev_header_has_timestamp(evh)
	return band(evh, UJM_TIMESTAMP) ~= 0
end

local function ev_header_clear_timestamp(evh)
	return band(evh, UJM_TIMESTAMP_INV)
end

local function ev_header_is_valid(evh)
	return evh <= 0x0f or evh == UJM_EPILOGUE_HEADER
end

local function ev_header_is_epilogue(evh)
	return evh == UJM_EPILOGUE_HEADER
end

-- Splits event header into event type (aka aevent = allocation event) and
-- event source (aka asource = allocation source).
local function ev_header_split(evh)
	return band(evh, 0x3), band(evh, lshift(0x3, 2))
end

local function parse_event(reader, events, symbols, snap_interval)
	local ev_header = reader:read_octet()

	if ev_header_has_timestamp(ev_header) then
		if snap_interval then
			dump_heap_snap(events, symbols, snap_interval)
		end
		ev_header = ev_header_clear_timestamp(ev_header)
	end

	assert(ev_header_is_valid(ev_header), 'Bad ev_header ' .. ev_header)

	if ev_header_is_epilogue(ev_header) then
		return false
	end

	local aevent, asource = ev_header_split(ev_header)
	local parser = parsers[aevent]

	assert(parser, 'Bad aevent ' .. aevent)

	parser.parse(reader, asource, events[parser.evname], events.heap)

	return true
end

function M.parse(reader, symbols, snap_interval)
	local events = {
		alloc   = {},
		realloc = {},
		free    = {},
		heap    = {},

		sec     =  0, -- pseudo-second passed from profiling start
	}

	local magic   = reader:read_octets(3)
	local version = reader:read_octets(1)
	local _       = reader:read_octets(3) -- dummy-consume reserved bytes

	if magic ~= UJM_MAGIC then
		error('Bad UJM format prologue: ' .. magic)
	end

	if string.byte(version) ~= UJM_CURRENT_VERSION then
		error(string_format(
		     'UJM format version mismatch: the tool expects %d, but your data is %d',
		     UJM_CURRENT_VERSION,
		     string.byte(version)
		))
	end

	while parse_event(reader, events, symbols, snap_interval) do
		-- empty body
	end

	return events
end

return M
