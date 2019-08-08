-- This is a part of uJIT's testing suite.
-- Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

jit.off()

local tests = {}

local function assert_immutable_nyi_type(obj)
    local status, errmsg = pcall(ujit.immutable, obj)
    assert(status == false)
    assert(string.match(errmsg, "attempt to make immutable"))
end

local function assert_modification_error(...)
    local status, errmsg = pcall(...)
    assert(status == false)
    assert(string.match(errmsg, "attempt to modify .+ object$"))
end

function tests.immutable_types()
    -- Values below are immutable per se, a no-op behaviour expected:
    assert(ujit.immutable()      == nil)
    assert(ujit.immutable(nil)   == nil)
    assert(ujit.immutable(false) == false)
    assert(ujit.immutable(true)  == true)
    assert(ujit.immutable(4.2)   == 4.2)
    assert(ujit.immutable(1/0)   == 1/0)
    assert(ujit.immutable("")    == "")
    assert(ujit.immutable("xxx") == "xxx")
    assert(ujit.immutable(io.stdout) == io.stdout)
    assert(type(io.stdout) == "userdata")
    -- any function is a no-op:
    local upvalue = 0
    local foo1 = function (x, y)
        return x + y
    end
    local foo2 = function (x, y)
        upvalue = upvalue + 1
        return upvalue > 100 and x + y or x - y
    end
    assert(ujit.immutable(foo1) == foo1)
    assert(ujit.immutable(foo2) == foo2)
    assert(type(pairs) == "function" and ujit.immutable(pairs) == pairs)
end

function tests.immutable_nyi_types()
    -- coroutine
    local co = coroutine.create(function() end)
    assert(type(co) == "thread")
    assert_immutable_nyi_type(co)

    -- cdata
    local has_ffi, ffi = pcall(require, "ffi")
    if has_ffi then
        local cdata = ffi.new("uint64_t")
        assert(type(cdata) == "cdata")
        assert_immutable_nyi_type(cdata)
    end
end

function tests.immutable_tables_basic()
    local mutator_tsets = function (t) t.k = "v" end
    local mutator_tsetb = function (t) t[1] = "v" end
    local mutator_tsetv = function (t) local k = "k"; t[k] = "v" end
    local mutator_raw   = function (t) rawset(t, "k", "v") end

    local t1 = ujit.immutable{}
    assert_modification_error(mutator_tsets, t1)
    assert_modification_error(mutator_tsetb, t1)
    assert_modification_error(mutator_tsetv, t1)
    assert_modification_error(mutator_raw,   t1)

    local t2 = ujit.immutable{ nested = {} }
    assert_modification_error(mutator_tsets, t2.nested)
    assert_modification_error(mutator_tsetb, t2.nested)
    assert_modification_error(mutator_tsetv, t2.nested)
    assert_modification_error(mutator_raw,   t2.nested)

    local t3 = ujit.immutable{ nested = { nested = {} } }
    assert_modification_error(mutator_tsets, t3.nested.nested)
    assert_modification_error(mutator_tsetb, t3.nested.nested)
    assert_modification_error(mutator_tsetv, t3.nested.nested)
    assert_modification_error(mutator_raw,   t3.nested.nested)

    local t4 = ujit.immutable(ujit.immutable(ujit.immutable{
        [true]  = true,
        [false] = false,
        [42]    = 4.2,
        ["xxx"] = "xxx",
    }))
    assert(type(t4) == "table")
end

function tests.immutable_iterable_pairs()
    local t1 = ujit.immutable{
        foo = "bar",
        baz = 42,
    }
    local t2 = {}
    local n = 0

    assert(t1.foo == "bar")
    assert(t1.baz == 42)

    for k, v in pairs(t1) do
        n = n + 1
        t2[k] = v
    end

    assert(n == 2)
    assert(t2.foo == t1.foo)
    assert(t2.baz == t1.baz)
end

function tests.immutable_iterable_ipairs()
    local t1 = ujit.immutable{10, 20, 30}
    local t2 = {}

    assert(t1[1] == 10)
    assert(t1[2] == 20)
    assert(t1[3] == 30)

    for k, v in ipairs(t1) do
        t2[k] = v
    end

    assert(#t1 == #t2)
    assert(t2[1] == t1[1])
    assert(t2[2] == t1[2])
    assert(t2[3] == t1[3])
end

function tests.immutable_iterable_next()
    local k, v
    local t = {}

    k, v = next(ujit.immutable{foo = "bar"})
    assert(k == "foo")
    assert(v == "bar")

    k, v = next(ujit.immutable{[1] = "foo"})
    assert(k == 1)
    assert(v == "foo")

    k, v = next(ujit.immutable{[t] = t})
    assert(k == t)
    assert(v == t)
end

function tests.immutable_recursive()
    local function mutator(tbl, key, value) tbl[key] = value end
    local t1 = {}
    t1.t1 = t1
    ujit.immutable(t1)
    assert(t1.t1 == t1)
    assert_modification_error(mutator, t1, "t1", "t1")
end

local function assert_mutable(t, exp_level, exp_oops_type)
    local t1 = t
    local level = 0
    while t1.nested ~= nil do
        t1.foo = "bar"
        t1 = t1.nested
        level = level + 1
    end
    assert(level == exp_level)
    assert(type(t1.oops) == exp_oops_type)
end

function tests.immutable_tables_nyi_types()
    local t1 = {
        nested = {
            nested = {
                nested = {
                    nested = {
                        oops = coroutine.create(function() end),
                    },
                },
            },
        },
    }
    assert_immutable_nyi_type(t1)
    assert_mutable(t1, 4, "thread")
end

function tests.immutable_unmarking()
    -- NB! This test relies on the order in which table elements are traversed.
    -- If we encounter an error in-between marking immutable (iteration 2),
    -- we must be able to unmark the *entire* object.
    local c = coroutine.create(function() end)
    local t = {
             -- Iteration# Marked? Success? Unmarked?
        {},  --         1     YES      YES       YES
         c,  --         2     YES       NO        NO
        {},  --         3      NO      N/A       YES
    }
    local status = pcall(ujit.immutable, t)
    assert(status == false)
end

-- An immutable object t1 gets referenced from a mutable object t2, which cannot
-- be made immutable. After that, t1 and t2 should preserve their states:
-- t1 should still be immutable, and t2 should still be mutable.
function tests.immutable_consistency()
    local mutator = function (t, k, v) t[k] = v end
    local t1 = ujit.immutable{}
    local t2 = {
        nested = {
            nested = {
                t1   = t1,
                oops = coroutine.create(function() end),
            },
        },
    }
    assert_modification_error(mutator, t1, "foo", "bar")
    assert_immutable_nyi_type(t2)
    assert_mutable(t2, 2, "thread")
    assert_modification_error(mutator, t1, "foo", "bar")
end

function tests.ok_copying_immutable()
    local t1 = ujit.immutable{
        k = "v",
    }
    local t2 = {}
    for k, _ in pairs(t1) do
        t2[k] = t1[k]
    end
    assert(t2.k == "v")
    t2.k = "v"
    t2.K = "V"
end

function tests.metatable_immutability()
    local mutator = function (t, k, v) t[k] = v end
    local mt = {
        __index = function(_, k)
            return k
        end,
        __newindex = function (_, _, _)
            error("Unreachable")
        end
    }
    local t1 = ujit.immutable(setmetatable({}, mt))
    assert(t1.foo   == "foo")
    assert(t1[true] == true)
    -- table itself is immutable:
    assert_modification_error(mutator, t1, "k", "v")
    -- metatable is immutable, too:
    assert_modification_error(mutator, mt, "k", "v")
end

function tests.error_on_setmetatable()
    local mt = {}
    assert_modification_error(setmetatable, ujit.immutable{}, mt)
end

function tests.error_on_debug_setmetatable()
    local t1 = ujit.immutable{}
    assert_modification_error(debug.setmetatable, t1, {})
end

function tests.error_on_table_sort()
    local t1 = ujit.immutable{5, 3, 14}
    assert_modification_error(table.sort, t1)
end

function tests.error_on_table_insert()
    local t1 = ujit.immutable{5, 3, 14}
    assert_modification_error(table.insert, t1, 'foo') -- push_back
    assert_modification_error(table.insert, t1, 2, 'foo') -- insert
end

function tests.error_on_table_remove()
    local t1 = ujit.immutable{5, 3, 14}
    assert_modification_error(table.remove, t1, 2) -- remove
    assert_modification_error(table.remove, t1, 3) -- pop_back
end

-- A k-weak table referencing an immutable object should not
-- preserve corresponding key-value pair over GC cycle.
function tests.sweeping_weak_keys()
    local kweak = setmetatable({ [{}] = ujit.immutable{} }, {__mode = 'k'})

    assert(next(kweak) ~= nil)
    collectgarbage()
    assert(next(kweak) == nil)
end

-- A v-weak table referencing an immutable object should not
-- preserve corresponding key-value pair over GC cycle.
function tests.sweeping_weak_values()
    local vweak = setmetatable({ x = ujit.immutable{y = 42} }, {__mode = 'v'})

    assert(vweak.x.y == 42)
    collectgarbage()
    assert(vweak.x == nil)
end

-- If a table is made immutable, it will be sealed.
function tests.seal()
    local t = ujit.immutable{
        foo = {
            bar = {
                baz = "qux"
            }
        }
    }
    ujit.seal(t)
end

for k, v in pairs(tests) do
    if (type(k) == 'string' and type(v) == 'function')
    then
        v()
    end
end
