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

=head1 Lua table & coercion

=head2 Synopsis

    % prove 106-table.t

=head2 Description

=cut

--]]

require'tap'
local has_op53 = _VERSION >= 'Lua 5.3'

plan'no_plan'

error_like(function () return -{} end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "-{}")

is(# {}, 0, "#{}")
is(# {4,5,6}, 3)

is(not {}, false, "not {}")

error_like(function () return {} + 10 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "{} + 10")

error_like(function () return {} - 2 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "{} - 2")

error_like(function () return {} * 3.14 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "{} * 3.14")

error_like(function () return {} / 7 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "{} / 7")

error_like(function () return {} % 4 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "{} % 4")

error_like(function () return {} ^ 3 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "{} ^ 3")

error_like(function () return {} .. 'end' end,
           "^[^:]+:%d+: attempt to concatenate",
           "{} .. 'end'")

is({} == {}, false, "{} == {}")

local t1 = {}
local t2 = {}
is(t1 == t1, true, "t1 == t1")
is(t1 == t2, false, "t1 == t2")
is(t1 ~= t2, true, "t1 ~= t2")

is({} == 1, false, "{} == 1")

is({} ~= 1, true, "{} ~= 1")

error_like(function () return t1 < t2 end,
           "^[^:]+:%d+: attempt to compare two table values",
           "t1 < t2")

error_like(function () return t1 <= t2 end,
           "^[^:]+:%d+: attempt to compare two table values",
           "t1 <= t2")

error_like(function () return t1 > t2 end,
           "^[^:]+:%d+: attempt to compare two table values",
           "t1 > t2")

error_like(function () return t1 >= t2 end,
           "^[^:]+:%d+: attempt to compare two table values",
           "t1 >= t2")

error_like(function () return {} < 0 end,
           "^[^:]+:%d+: attempt to compare %w+ with %w+",
           "{} < 0")

error_like(function () return {} <= 0 end,
           "^[^:]+:%d+: attempt to compare %w+ with %w+",
           "{} <= 0")

error_like(function () return {} > 0 end,
           "^[^:]+:%d+: attempt to compare %w+ with %w+",
           "{} > 0")

error_like(function () return {} >= 0 end,
           "^[^:]+:%d+: attempt to compare %w+ with %w+",
           "{} >= 0")

local t = {}
is( t[1], nil, "index" )
t[1] = 42
is( t[1], 42, "index" )

error_like(function () t = {}; t[nil] = 42 end,
           "^[^:]+:%d+: table index is nil",
           "table index is nil")

error_like(function () t = {}; t[0/0] = 42 end,
           "^[^:]+:%d+: table index is NaN",
           "table index is NaN")

if has_op53 then
    dofile'lexico53/table.t'
end

done_testing()

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
