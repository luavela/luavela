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

=head1 Lua tables

=head2 Synopsis

    % prove 221-table.t

=head2 Description

See section "Tables" in "Programming in Lua".

=cut

--]]

require'tap'

plan(25)

do
    local a = {}
    local k = 'x'
    a[k] = 10
    a[20] = 'great'
    is(a['x'], 10)
    k = 20
    is(a[k], 'great')
    a['x'] = a ['x'] + 1
    is(a['x'], 11)
end

do
    local a = {}
    a['x'] = 10
    local b = a
    is(b['x'], 10)
    b['x'] = 20
    is(a['x'], 20)
    a = nil
    b = nil
end

do
    local a = {}
    for i=1,1000 do a[i] = i*2 end
    is(a[9], 18)
    a['x'] = 10
    is(a['x'], 10)
    is(a['y'], nil)
end

do
    local a = {}
    local x = 'y'
    a[x] = 10
    is(a[x], 10)
    is(a.x, nil)
    is(a.y, 10)
end

do
    local i = 10; local j = '10'; local k = '+10'
    local a = {}
    a[i] = "one value"
    a[j] = "another value"
    a[k] = "yet another value"
    is(a[j], "another value")
    is(a[k], "yet another value")
    is(a[tonumber(j)], "one value")
    is(a[tonumber(k)], "one value")
end

do
    local t = { {'a','b','c'}, 10 }
    is(t[2], 10)
    is(t[1][3], 'c')
    t[1][1] = 'A'
    is(table.concat(t[1],','), 'A,b,c')
end

do
    local tt = { {'a','b','c'}, 10 }
    is(tt[2], 10)
    is(tt[1][3], 'c')
    tt[1][1] = 'A'
    is(table.concat(tt[1],','), 'A,b,c')
end

do
    local a = {}
    error_like(function () a() end,
               "^[^:]+:%d+: attempt to call")
end

do
    local tt = { {'a','b','c'}, 10 }
    is((tt)[2], 10)
    is((tt[1])[3], 'c');
    (tt)[1][2] = 'B'
    (tt[1])[3] = 'C'
    is(table.concat(tt[1],','), 'a,B,C')
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
