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

=head1 Lua functions

=head2 Synopsis

    % prove 212-function.t

=head2 Description

See section "Function Definitions" in "Reference Manual"
L<https://www.lua.org/manual/5.1/manual.html#2.5.9>,
L<https://www.lua.org/manual/5.2/manual.html#3.4.10>,
L<https://www.lua.org/manual/5.3/manual.html#3.4.11>

See section "Functions" in "Programming in Lua".

=cut

--]]

require'tap'
local loadstring = loadstring or load

plan(68)

do --[[ add ]]
    local function add (a)
        local sum = 0
        for i,v in ipairs(a) do
            sum = sum + v
        end
        return sum
    end

    local t = { 10, 20, 30, 40 }
    is(add(t), 100, "add")
end

do --[[ f ]]
    local function f(a, b) return a or b end

    is(f(3), 3, "f")
    is(f(3, 4), 3)
    is(f(3, 4, 5), 3)
end

do --[[ incCount ]]
    local count = 0

    local function incCount (n)
        n = n or 1
        count = count + n
    end

    is(count, 0, "inCount")
    incCount()
    is(count, 1)
    incCount(2)
    is(count, 3)
    incCount(1)
    is(count, 4)
end

do --[[ maximum ]]
    local function maximum (a)
        local mi = 1                -- maximum index
        local m = a[mi]             -- maximum value
        for i,val in ipairs(a) do
            if val > m then
                mi = i
                m = val
            end
        end
        return m, mi
    end

    local m, mi = maximum({8,10,23,12,5})
    is(m, 23, "maximum")
    is(mi, 3)
end

do --[[ call by value ]]
    local function f (n)
        n = n - 1
        return n
    end

    local a = 12
    is(a, 12, "call by value")
    local b = f(a)
    is(b, 11)
    is(a, 12)
    local c = f(12)
    is(c, 11)
    is(a, 12)
end

do --[[ call by ref ]]
    local function f (t)
        t[#t+1] = 'end'
        return t
    end

    local a = { 'a', 'b', 'c' }
    is(table.concat(a, ','), 'a,b,c', "call by ref")
    local b = f(a)
    is(table.concat(b, ','), 'a,b,c,end')
    is(table.concat(a, ','), 'a,b,c,end')
end

do --[[ var args ]]
    local function g1(a, b, ...)
        local arg = {...}
        is(a, 3, "vararg")
        is(b, nil)
        is(#arg, 0)
        is(arg[1], nil)
    end
    g1(3)

    local function g2(a, b, ...)
        local arg = {...}
        is(a, 3)
        is(b, 4)
        is(#arg, 0)
        is(arg[1], nil)
    end
    g2(3, 4)

    local function g3(a, b, ...)
        local arg = {...}
        is(a, 3)
        is(b, 4)
        is(#arg, 2)
        is(arg[1], 5)
        is(arg[2], 8)
    end
    g3(3, 4, 5, 8)
end

do --[[ var args ]]
    local function g1(a, b, ...)
        local c, d, e = ...
        is(a, 3, "var args")
        is(b, nil)
        is(c, nil)
        is(d, nil)
        is(e, nil)
    end
    g1(3)

    local function g2(a, b, ...)
        local c, d, e = ...
        is(a, 3)
        is(b, 4)
        is(c, nil)
        is(d, nil)
        is(e, nil)
    end
    g2(3, 4)

    local function g3(a, b, ...)
        local c, d, e = ...
        is(a, 3)
        is(b, 4)
        is(c, 5)
        is(d, 8)
        is(e, nil)
    end
    g3(3, 4, 5, 8)
end

do --[[ var args ]]
    local function g1(a, b, ...)
        is(#{a, b, ...}, 1, "varargs")
    end
    g1(3)

    local function g2(a, b, ...)
        is(#{a, b, ...}, 2)
    end
    g2(3, 4)

    local function g3(a, b, ...)
        is(#{a, b, ...}, 4)
    end
    g3(3, 4, 5, 8)
end

do --[[ var args ]]
    local function f() return 1, 2 end
    local function g() return 'a', f() end
    local function h() return f(), 'b' end
    local function k() return 'c', (f()) end

    local x, y = f()
    is(x, 1, "var args")
    is(y, 2)
    local z
    x, y, z = g()
    is(x, 'a')
    is(y, 1)
    is(z, 2)
    x, y = h()
    is(x, 1)
    is(y, 'b')
    x, y, z = k()
    is(x, 'c')
    is(y, 1)
    is(z, nil)
end

do --[[ invalid var args ]]
    local f, msg = loadstring [[
function f ()
    print(...)
end
]]
    like(msg, "^[^:]+:%d+: cannot use '...' outside a vararg function", "invalid var args")
end

do --[[ tail call ]]
    local output = {}
    local function foo (n)
        output[#output+1] = n
        if n > 0 then
            return foo(n -1)
        end
        return 'end', 0
    end

    eq_array({foo(3)}, {'end', 0}, "tail call")
    eq_array(output, {3, 2, 1, 0})
end

do --[[ no tail call ]]
    local output = {}
    local function foo (n)
        output[#output+1] = n
        if n > 0 then
            return (foo(n -1))
        end
        return 'end', 0
    end

    is(foo(3), 'end', "no tail call")
    eq_array(output, {3, 2, 1, 0})
end

do --[[ no tail call ]]
    local output = {}
    local function foo (n)
        output[#output+1] = n
        if n > 0 then
            foo(n -1)
        end
    end

    is(foo(3), nil, "no tail call")
    eq_array(output, {3, 2, 1, 0})
end

do --[[ sub name ]]
    local function f () return 1 end
    is(f(), 1, "sub name")

    function f () return 2 end
    is(f(), 2)
end

-- Local Variables:
--   mode: lua
--   lua-indent-level: 4
--   fill-column: 100
-- End:
-- vim: ft=lua expandtab shiftwidth=4:
