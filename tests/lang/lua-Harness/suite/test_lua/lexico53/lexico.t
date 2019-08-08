--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2015-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

is("\u{41}", "A")
is("\u{20AC}", "\xE2\x82\xAC")
is("\u{20ac}", "\xe2\x82\xac")

do
    local f, msg = load [[a = "A\u{yz}"]]
    like(msg, "^[^:]+:%d+: .- near")

    f, msg = load [[a = "A\u{41"]]
    like(msg, "^[^:]+:%d+: .- near")

    f, msg = load [[a = "A\u{FFFFFFFFFF}"]]
    like(msg, "^[^:]+:%d+: .- near")
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
