#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2009-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 Lua Input/Output Library

=head2 Synopsis

    % prove 308-io.t

=head2 Description

Tests Lua Input/Output Library

See section "Input and Output Facilities" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#5.7>,
L<https://www.lua.org/manual/5.2/manual.html#6.8>,
L<https://www.lua.org/manual/5.3/manual.html#6.8>

=cut

--]]

require'tap'
local profile = require'profile'
local has_write51 = true -- UJIT: currently we have f:write behaving like in 5.1 (returns true on success)
local has_lines52 = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_read52 = _VERSION >= 'Lua 5.2' or profile.luajit_compat52
local has_read53 = _VERSION >= 'Lua 5.3' or  (jit and jit.version_num >= 20100)

local lua = arg[-3] or arg[-1]

plan'no_plan'

do -- stdin
    like(io.stdin, '^file %(0?[Xx]?%x+%)$', "variable stdin")
end

do -- stdout
    like(io.stdout, '^file %(0?[Xx]?%x+%)$', "variable stdout")
end

do -- stderr
    like(io.stderr, '^file %(0?[Xx]?%x+%)$', "variable stderr")
end

do -- close
    local r, msg = io.close(io.stderr)
    is(r, nil, "close (std)")
    is(msg, "cannot close standard file")
end

do -- flush
    is(io.flush(), true, "function flush")
end

do -- open
    os.remove('file-308.no')
    local f, msg = io.open("file-308.no")
    is(f, nil, "function open")
    is(msg, "file-308.no: No such file or directory")

    os.remove('file-308.txt')
    f = io.open('file-308.txt', 'w')
    f:write("file with text\n")
    f:close()
    f = io.open('file-308.txt')
    like(f, '^file %(0?[Xx]?%x+%)$', "function open")

    is(io.close(f), true, "function close")

    error_like(function () io.close(f) end,
               "^[^:]+:%d+: attempt to use a closed file",
               "function close (closed)")

    if _VERSION == 'Lua 5.1' then
        todo("not with 5.1")
    end
    error_like(function () io.open('file-308.txt', 'baz') end,
               "^[^:]+:%d+: bad argument #2 to 'open' %(invalid mode%)",
               "function open (bad mode)")
end

do -- type
    is(io.type("not a file"), nil, "function type")
    local f = io.open('file-308.txt')
    is(io.type(f), 'file')
    like(tostring(f), '^file %(0?[Xx]?%x+%)$')
    io.close(f)
    is(io.type(f), 'closed file')
    is(tostring(f), 'file (closed)')
end

do -- input
    is(io.stdin, io.input(), "function input")
    is(io.stdin, io.input(nil))
    local f = io.stdin
    like(io.input('file-308.txt'), '^file %(0?[Xx]?%x+%)$')
    is(f, io.input(f))
end

do -- output
    is(io.output(), io.stdout, "function output")
    is(io.output(nil), io.stdout)
    local f = io.stdout
    like(io.output('output.new'), '^file %(0?[Xx]?%x+%)$')
    is(f, io.output(f))
    os.remove('output.new')
end

do -- popen
    local r, f = pcall(io.popen, lua .. [[ -e "print 'standard output'"]])
    if r then
        is(io.type(f), 'file', "popen (read)")
        is(f:read(), "standard output")
        is(io.close(f), true)
    else
        diag("io.popen not supported")
    end

    r, f = pcall(io.popen, lua .. [[ -e "for line in io.lines() do print((line:gsub('e', 'a'))) end"]], 'w')
    if r then
        is(io.type(f), 'file', "popen (write)")
        f:write("# hello\n") -- not tested : hallo
        is(io.close(f), true)
    else
        diag("io.popen not supported")
    end
end

do -- lines
    for line in io.lines('file-308.txt') do
        is(line, "file with text", "function lines(filename)")
    end

    error_like(function () io.lines('file-308.no') end,
               "No such file or directory",
               "function lines(no filename)")
end

do -- tmpfile
    local  f = io.tmpfile()
    is(io.type(f), 'file', "function tmpfile")
    f:write("some text")
    f:close()
end

do -- write
    io.write() -- not tested
    io.write('# text', 12, "\n") -- not tested :  # text12
end

do -- :close
    local r, msg = io.stderr:close()
    is(r, nil, "method close (std)")
    is(msg, "cannot close standard file")

    local f = io.open('file-308.txt')
    is(f:close(), true, "method close")
end

do -- :flush
    is(io.stderr:flush(), true, "method flush")

    local f = io.open('file-308.txt')
    f:close()
    error_like(function () f:flush() end,
               "^[^:]+:%d+: attempt to use a closed file",
               "method flush (closed)")
end

do -- :read & :write
    local f = io.open('file-308.txt')
    f:close()
    error_like(function () f:read() end,
               "^[^:]+:%d+: attempt to use a closed file",
               "method read (closed)")

    f = io.open('file-308.txt')
    local s = f:read()
    is(s:len(), 14, "method read")
    is(s, "file with text")
    s = f:read()
    is(s, nil)
    f:close()

    f = io.open('file-308.txt')
    error_like(function () f:read('*z') end,
               "^[^:]+:%d+: bad argument #1 to 'read' %(invalid %w+%)",
               "method read (invalid)")
    f:close()

    f = io.open('file-308.txt')
    local s1, s2 = f:read('*l', '*l')
    is(s1:len(), 14, "method read *l")
    is(s1, "file with text")
    is(s2, nil)
    f:close()

    if has_read52 then
        f = io.open('file-308.txt')
        s1, s2 = f:read('*L', '*L')
        is(s1:len(), 15, "method read *L")
        is(s1, "file with text\n")
        is(s2, nil)
        f:close()
    else
        diag("no read *L")
    end

    f = io.open('file-308.txt')
    local n1, n2 = f:read('*n', '*n')
    is(n1, nil, "method read *n")
    is(n2, nil)
    f:close()

    f = io.open('file-308.num', 'w')
    f:write('1\n')
    f:write('0xFF\n')
    f:write(string.rep('012', 90) .. '\n')
    f:close()

    f = io.open('file-308.num')
    n1, n2 = f:read('*n', '*n')
    is(n1, 1, "method read *n")
    is(n2, 255, "method read *n")
    local n = f:read('*n')
    if _VERSION < 'Lua 5.3' then
        type_ok(n, 'number')
    else
        is(n, nil, "method read *n too long")
    end
    f:close()

    os.remove('file-308.num') -- clean up

    f = io.open('file-308.txt')
    s = f:read('*a')
    is(s:len(), 15, "method read *a")
    is(s, "file with text\n")
    f:close()

    if has_read53 then
        f = io.open('file-308.txt')
        s = f:read('a')
        is(s:len(), 15, "method read a")
        is(s, "file with text\n")
        f:close()
    else
        diag("* mandatory")
    end

    f = io.open('file-308.txt')
    is(f:read(0), '', "method read number")
    eq_array({f:read(5, 5, 15)}, {'file ', 'with ', "text\n"})
    f:close()
end

do -- :lines
    local f = io.open('file-308.txt')
    for line in f:lines() do
        is(line, "file with text", "method lines")
    end
    is(io.type(f), 'file')
    f:close()
    is(io.type(f), 'closed file')

    if has_lines52 then
        f = io.open('file-308.txt')
        for two_char in f:lines(2) do
            is(two_char, "fi", "method lines (with read option)")
            break
        end
        f:close()
    else
        diag("no lines with option")
    end
end

do -- :seek
    local f = io.open('file-308.txt')
    f:close()

    error_like(function () f:seek('end', 0) end,
               "^[^:]+:%d+: attempt to use a closed file",
               "method seek (closed)")

    f = io.open('file-308.txt')
    error_like(function () f:seek('bad', 0) end,
               "^[^:]+:%d+: bad argument #1 to 'seek' %(invalid option 'bad'%)",
               "method seek (invalid)")

    f = io.open('file-308.bin', 'w')
    f:write('ABCDE')
    f:close()
    f = io.open('file-308.bin')
    is(f:seek('end', 0), 5, "method seek")
    f:close()
    os.remove('file-308.bin') --clean up
end

do -- :setvbuf
    local  f = io.open('file-308.txt')
     is(f:setvbuf('no'), true, "method setvbuf 'no'")

    is(f:setvbuf('full', 4096), true, "method setvbuf 'full'")

    is(f:setvbuf('line', 132), true, "method setvbuf 'line'")
    f:close()
end

os.remove('file-308.txt') -- clean up

do -- :write
    local  f = io.open('file-308.out', 'w')
    f:close()
    error_like(function () f:write('end') end,
               "^[^:]+:%d+: attempt to use a closed file",
               "method write (closed)")

    f = io.open('file-308.out', 'w')
    if has_write51 then
        is(f:write('end'), true, "method write")
    else
        is(f:write('end'), f, "method write")
    end
    f:close()

    os.remove('file-308.out') --clean up
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
