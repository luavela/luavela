
--
-- lua-Harness : <https://fperrad.frama.io/lua-Harness/>
--

--[[

        Test Anything Protocol : minimalist version

]]

if pcall(require, 'Test.More') then
    diag 'Test.More loaded'
    return
end

local _G = _G
local os = os
local pcall = pcall
local print = print
local require = require
local tostring = tostring
local type = type

local curr_test = 0
local expected_tests = 0
local todo_upto = 0
local todo_reason

function plan (arg)
    if arg ~= 'no_plan' then
        expected_tests = arg
        print("1.." .. tostring(arg))
    end
end

function done_testing ()
    print("1.." .. tostring(curr_test))
end

function skip_all (reason)
    out = "1..0"
    if reason then
        out = out .. " # SKIP " .. reason
    end
    print(out)
    os.exit(0)
end

function ok (test, name)
    curr_test = curr_test + 1
    local out = ''
    if not test then
        out = "not "
    end
    out = out .. "ok " .. tostring(curr_test)
    if name then
        out = out .. " - " .. name
    end
    if todo_reason and todo_upto >= curr_test then
        out = out .. " # TODO # " .. todo_reason
    end
    print(out)
end

function nok (test, name)
    ok(not test, name)
end

function is (got, expected, name)
    local pass = got == expected
    ok(pass, name)
    if not pass then
        diag("         got: " .. tostring(got))
        diag("    expected: " .. tostring(expected))
    end
end

function isnt (got, not_expected, name)
    local pass = got ~= not_expected
    ok(pass, name)
    if not pass then
        diag("         got: " .. tostring(got))
        diag("    expected: anything else")
    end
end

function like (got, pattern, name)
    local pass = tostring(got):match(pattern)
    ok(pass, name)
    if not pass then
        diag("                  " .. tostring(got))
        diag("    doesn't match '" .. tostring(pattern) .. "'")
    end
end

function type_ok (val, t, name)
    if type(val) == t then
        ok(true, name)
    else
        ok(false, name)
        diag("    " .. tostring(val) .. " isn't a '" .. t .."' it's '" .. type(val) .. "'")
    end
end

function pass (name)
    ok(true, name)
end

function fail (name)
    ok(false, name)
end

function require_ok (mod)
    local r, msg = pcall(require, mod)
    ok(r, "require '" .. mod .. "'")
    if not r then
        diag("    " .. msg)
    end
    return r
end

function eq_array (got, expected, name)
    for i = 1, #expected do
        local v = expected[i]
        local val = got[i]
        if val ~= v then
            ok(false, name)
            diag("    at index: " .. tostring(i))
            diag("         got: " .. tostring(val))
            diag("    expected: " .. tostring(v))
            return
        end
    end
    local extra = #got - #expected
    if extra ~= 0 then
        ok(false, name)
        diag("    " .. tostring(extra) .. " unexpected item(s)")
    else
        ok(true, name)
    end
end

function error_is (code, expected, name)
    local r, msg = pcall(code)
    if r then
        ok(false, name)
        diag("    unexpected success")
        diag("    expected: " .. tostring(pattern))
    else
        is(msg, expected, name)
    end
end

function error_like (code, pattern, name)
    local r, msg = pcall(code)
    if r then
        ok(false, name)
        diag("    unexpected success")
        diag("    expected: " .. tostring(pattern))
    else
        like(msg, pattern, name)
    end
end

function diag (msg)
    print("# " .. msg)
end

function skip (reason, count)
    count = count or 1
    local name = "# skip"
    if reason then
        name = name .. " " ..reason
    end
    for i = 1, count do
        ok(true, name)
    end
end

function skip_rest (reason)
    skip(reason, expected_tests - curr_test)
end

function todo (reason, count)
    count = count or 1
    todo_upto = curr_test + count
    todo_reason = reason
end

--
-- Copyright (c) 2009-2018 Francois Perrad
--
-- This library is licensed under the terms of the MIT/X11 license,
-- like Lua itself.
--
