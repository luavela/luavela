-- An implementation of buffered reading data from an arbitrary binary file.
--
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local assert = assert

local ffi = require 'ffi'
local bit = require 'bit'

local ffi_C  = ffi.C
local band   = bit.band

local BUFFER_SIZE = 10 * 1024 * 1024 -- 10 megabytes

local M = {}

ffi.cdef[[
	void *memcpy(void *, const void *, size_t);

	typedef struct FILE_ FILE;
	FILE *fopen(const char *, const char *);
	size_t fread(void *, size_t, size_t, FILE *);
	int feof(FILE *);
	int fclose(FILE *);
]]

local function _read_stream(reader, n)
	local free_size
	local tail_size = reader._end - reader._pos

	if tail_size >= n then
		-- Enough data to satisfy the request of n bytes...
		return true
	end

	-- ...otherwise carry tail_size bytes from the end of the buffer
	-- to the start and fill up free_size bytes with fresh data.
	-- tail_size < n <= free_size (see assert below) ensures that
	-- we don't copy overlapping memory regions.
	-- reader._pos == 0 means filling buffer for the first time.

	free_size = reader._pos > 0 and reader._pos or n

	assert(n <= free_size, 'Internal buffer is large enough')

	if tail_size ~= 0 then
		ffi_C.memcpy(reader._buf, reader._buf + reader._pos, tail_size)
	end

	local bytes_read = ffi_C.fread(
		reader._buf + tail_size, 1, free_size, reader._file
	)

	reader._pos = 0
	reader._end = tail_size + bytes_read

	return reader._end - reader._pos >= n
end

function M.read_octet(reader)
	if not _read_stream(reader, 1) then
		return nil
	end

	local oct = reader._buf[reader._pos]
	reader._pos = reader._pos + 1
	return oct
end

function M.read_octets(reader, n)
	if not _read_stream(reader, n) then
		return nil
	end

	local octets = ffi.string(reader._buf + reader._pos, n)
	reader._pos = reader._pos + n
	return octets
end

function M.read_uleb128(reader)
	local value = ffi.new('uint64_t', 0)
	local shift = 0

	repeat
		local oct = M.read_octet(reader)

		if oct == nil then
			break
		end

		-- Alas, bit library works only with 32-bit arguments:
		local oct_u64 = ffi.new('uint64_t', band(oct, 0x7f))
		value = value + oct_u64 * (2 ^ shift)
		shift = shift + 7

	until band(oct, 0x80) == 0

	return tonumber(value)
end

function M.read_string(reader)
	local len = M.read_uleb128(reader)
	return M.read_octets(reader, len)
end

function M.eof(reader)
	local sys_feof = ffi_C.feof(reader._file)
	if sys_feof == 0 then
		return false
	end
	-- ...otherwise return true only we have reached the end of the buffer:
	return reader._pos == reader._end
end

function M.new(fname)
	local file = ffi_C.fopen(fname, 'rb')
	if file == nil then
		error(string.format('fopen, errno: %d', ffi.errno()))
	end

	local finalizer = function(f)
		if ffi_C.fclose(f) ~= 0 then
			error(string.format('fclose, errno: %d', ffi.errno()))
		end
		ffi.gc(f, nil)
	end

	local reader = setmetatable({
		_file = ffi.gc(file, finalizer),
		_buf  = ffi.new('uint8_t[?]', BUFFER_SIZE),
		_pos  = 0,
		_end  = 0,
	}, {__index = M})

	_read_stream(reader, BUFFER_SIZE)

	return reader
end

return M
