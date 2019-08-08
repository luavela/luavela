--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--
-- Copyright (C) 2009-2018, Perrad Francois
--
-- This code is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--

local f = function () return 1 end

error_like(function () return ~f end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value",
           "~f")

error_like(function () f = print; return ~f end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value")

error_like(function () return f // 3 end,
           "^[^:]+:%d+: attempt to perform arithmetic on",
           "f // 3")

error_like(function () f = print; return f // 3 end,
           "^[^:]+:%d+: attempt to perform arithmetic on")

error_like(function () return f & 7 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value",
           "f & 7")

error_like(function () f = print; return f & 7 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value")

error_like(function () return f | 1 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value",
           "f | 1")

error_like(function () f = print; return f | 1 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value")

error_like(function () return f ~ 4 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value",
           "f ~ 4")

error_like(function () f = print; return f ~ 4 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value")

error_like(function () return f >> 5 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value",
           "f >> 5")

error_like(function () f = print; return f >> 5 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value")

error_like(function () return f << 2 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value",
           "f << 2")

error_like(function () f = print; return f << 2 end,
           "^[^:]+:%d+: attempt to perform bitwise operation on a function value")

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
