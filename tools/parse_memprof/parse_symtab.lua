-- Parser of uJIT's symtab binary stream.
-- The format spec can be found in src/profile/uj_symtab.h.
--
-- Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local bit = require 'bit'

local band          = bit.band
local string_format = string.format

local UJS_MAGIC           = 'ujs'
local UJS_CURRENT_VERSION = 2
local UJS_EPILOGUE_HEADER = 0x80
local UJS_SYMTYPE_MASK    = 0x03

local SYMTAB_LFUNC = 0

local M = {}

-- Parse a single entry in a symtab: lfunc symbol
local function parse_sym_lfunc(reader, symtab)
	local sym_addr  = reader:read_uleb128()
	local sym_chunk = reader:read_string()
	local sym_line  = reader:read_uleb128()

	symtab[sym_addr] = {
		name = string_format('%s:%d', sym_chunk, sym_line),
	}
end

local parsers = {
	[SYMTAB_LFUNC] = parse_sym_lfunc,
}

function M.parse(reader)
	local symtab   = {}
	local magic    = reader:read_octets(3)
	local version  = reader:read_octets(1)

	local _
	_ = reader:read_octets(3) -- dummy-consume reserved bytes
	_ = reader:read_uleb128() -- dummy-consume vm-id

	if magic ~= UJS_MAGIC then
		error('Bad UJS format prologue: ' .. magic)
	end

	if string.byte(version) ~= UJS_CURRENT_VERSION then
		error(string_format(
		     'UJS format version mismatch: the tool expects %d, but your data is %d',
		     UJS_CURRENT_VERSION,
		     string.byte(version)
		))

	end

	while not reader:eof() do
		local header   = reader:read_octet()
		local is_final = band(header, UJS_EPILOGUE_HEADER) ~= 0

		if is_final then
			break
		end

		local sym_type = band(header, UJS_SYMTYPE_MASK)
		if parsers[sym_type] then
			parsers[sym_type](reader, symtab)
		end
	end

	return symtab
end

function M.demangle(symtab, loc)
	local addr = loc.addr

	if addr == 0 then
		return 'INTERNAL'
	end

	if symtab[addr] then
		return string_format('%s, line %d', symtab[addr].name, loc.line)
	end

	return string_format('CFUNC %#x', addr)
end

return M
