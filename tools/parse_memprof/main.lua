-- A tool for parsing and visualisation of uJIT's memprof output.
--
-- TODO:
-- * Think about callgraph memory profiling for complex table reallocations
-- * Nicer output, probably an HTML view
-- * Demangling of C symbols
--
-- Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local bufread = require 'bufread'
local memprof = require 'parse_memprof'
local symtab  = require 'parse_symtab'
local view    = require 'view_plain'

-- Quick and dirty version of https://github.com/mpeterv/argparse
local function arg_parse(arg)
	local res = {}
	local i = 1
	while i <= #arg do
		-- Is it a key (i.e. starts with --, has another symbols)?
		local is_key, key = arg[i]:match('^([-]+)(.+)')
		if is_key then
			local pos = key:find('=')
			if pos then
				res[key:sub(1, pos - 1)] = key:sub(pos + 1)
			else -- try to consume the next arg:
				local nextarg = arg[i + 1] or ''
				local value = nextarg:match('^([^-].*)')
				if value then
					i = i + 1
				end
				res[key] = value or true
			end
		else
			res[#res + 1] = arg[i]
		end
		i = i + 1
	end
	return res
end

local args = arg_parse{...}

if args.help then
	print [[
ujit-parse-memprof - parser of the memory usage profile collected
                     with uJIT's memprof.

SYNOPSIS

ujit-parse-memprof --profile memprof.bin [options]

Supported options are:

  --profile            memprof.bin  Path to uJIT memprof stream [MANDATORY]
  --heap-snap-interval SEC          Report heap state each SEC seconds
  --help                            Show this help and exit
]]
	os.exit(0)
end

if not args.profile then
	error('FATAL: --profile /path/to/memprof.bin is required')
end

local reader  = bufread.new(args.profile)

local symbols = symtab.parse(reader)
local events  = memprof.parse(reader,
			      symbols,
			      tonumber(args['heap-snap-interval']))

print('ALLOCATIONS')
view.render(events.alloc, symbols)
print('')

print('REALLOCATIONS')
view.render(events.realloc, symbols)
print('')

print('DEALLOCATIONS')
view.render(events.free, symbols)
print('')
