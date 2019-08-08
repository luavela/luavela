#! /usr/bin/lua
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2014-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

--[[

=head1 Lua UTF-8 support Library

=head2 Synopsis

    % prove 305-utf8.t

=head2 Description

Tests Lua UTF-8 Library

This library was introduced in Lua 5.3.

See section "UTF-8 support" in "Reference Manual"
L<https://www.lua.org/manual/5.3/manual.html#6.5>.

=cut

--]]

require 'tap'

local has_utf8 = _VERSION >= 'Lua 5.3'

if not utf8 then
    plan(1)
    nok(has_utf8, "no has_utf8")
else
    plan(69)
    dofile'lexico53/utf8.t'
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
