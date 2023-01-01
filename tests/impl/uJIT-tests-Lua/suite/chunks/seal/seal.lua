-- This is a part of uJIT's testing suite.
-- Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
-- Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

local tests = {}

local function assert_sealed_modification_error(...)
  local status, errmsg = pcall(...)
  assert(status == false)
  assert(string.match(errmsg, "attempt to modify .+ object$"))
end

local function assert_sealing_nyi_type(obj)
  local status, errmsg = pcall(ujit.seal, obj)
  assert(status == false)
  assert(string.match(errmsg, "attempt to seal"))
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

function tests.sealing_types()
    -- Values below are immutable per se, a no-op behaviour expected:
    assert(ujit.seal()      == nil)
    assert(ujit.seal(nil)   == nil)
    assert(ujit.seal(false) == false)
    assert(ujit.seal(true)  == true)
    assert(ujit.seal(4.2)   == 4.2)
    assert(ujit.seal(1/0)   == 1/0)
    assert(ujit.seal("")    == "")
    assert(ujit.seal("xxx") == "xxx")
    -- a function without upvalues:
    local foo1 = function (x, y)
        return x + y
    end
    assert(ujit.seal(foo1) == foo1)
    assert(type(pairs) == "function" and ujit.seal(pairs) == pairs)
end

function tests.sealing_nyi_types()
  -- userdata
  assert(type(io.stdout) == "userdata")
  assert_sealing_nyi_type(io.stdout)

  -- coroutine
  local co = coroutine.create(function() end)
  assert(type(co) == "thread")
  assert_sealing_nyi_type(co)

  -- function with upvalues
  local upvalue = 'foo'
  local fn_uv = function (x)
    return upvalue + x
  end
  assert_sealing_nyi_type(fn_uv)

  -- cdata
  local has_ffi, ffi = pcall(require, "ffi")
  if has_ffi then
    local cdata = ffi.new("uint64_t")
    assert(type(cdata) == "cdata")
    assert_sealing_nyi_type(cdata)
  end
end

function tests.sealing_atomic()
  -- We attempt to seal t1 recursively. At the inner-most level, an error will be
  -- thrown. If sealing is atomic, t1's state will be kept the same as it was
  -- before the sealing attempt. In our case, the t1 will still be writable
  -- at any nesting level.
  local t1 = {
    nested = {
      nested = {
        nested = {
          oops = coroutine.create(function() end),
        }
      }
    }
  }
  assert_sealing_nyi_type(t1)
  assert_mutable(t1, 3, "thread")
end

function tests.sealing_recursive()
  local function modify_table(tbl, key, value) tbl[key] = value end
  local tbl = {}
  tbl.tbl = tbl
  ujit.seal(tbl)
  assert(tbl.tbl == tbl)
  assert_sealed_modification_error(modify_table, tbl, "tbl", "tbl")
end

function tests.sealing_unmarking()
  -- NB! This test relies on the order in which table elements are traversed.
  -- If we encounter an error in-between marking immutable (iteration 2),
  -- we must be able to unmark the *entire* object.
  local t = {
                -- Iteration# Marked? Success? Unmarked?
    {},         --         1     YES      YES       YES
    io.stdout,  --         2     YES       NO        NO
    {},         --         3      NO      N/A       YES
  }
  local status = pcall(ujit.seal, t)
  assert(status == false)
end

-- A sealed object t1 gets referenced from a non-sealed object t2, which cannot
-- be made sealed. After that, t1 and t2 should preserve their states:
-- t1 should still be sealed, and t2 should still be non-sealed.
function tests.sealing_consistency()
  local function modify_table(tbl, key, value) tbl[key] = value end
  local t1 = ujit.seal({})
  local t2 = {
    nested = {
      nested = {
        t1   = t1,
        oops = io.stdout,
      },
    },
  }
  assert_sealed_modification_error(modify_table, t1, "foo", "bar")
  assert_sealing_nyi_type(t2)
  assert_mutable(t2, 2, "userdata")
  assert_sealed_modification_error(modify_table, t1, "foo", "bar")
end

function tests.error_on_direct_modification()
  local function modify_table(tbl, key, value) tbl[key] = value end
  local tbl = ujit.seal({5, 3, 14, foo = 'bar'})
  assert_sealed_modification_error(modify_table, tbl, 2, 8) -- rewrite array
  assert_sealed_modification_error(modify_table, tbl, 4, 4) -- add to array
  assert_sealed_modification_error(modify_table, tbl, 20000, 2) -- add integer to hash
  assert_sealed_modification_error(modify_table, tbl, 'foo', 'baz') -- rewrite hash
  assert_sealed_modification_error(modify_table, tbl, 'baz', 'lol') -- add to hash
end

function tests.error_on_setmetatable()
  local tbl = ujit.seal({5, 3, 14})
  assert_sealed_modification_error(setmetatable, tbl, {})
end

function tests.error_on_debug_setmetatable()
  local tbl = ujit.seal({5, 3, 14})
  assert_sealed_modification_error(debug.setmetatable, tbl, {})
end

function tests.error_on_table_sort()
  local tbl = ujit.seal({5, 3, 14})
  assert_sealed_modification_error(table.sort, tbl)
end

function tests.error_on_table_insert()
  local tbl = ujit.seal({5, 3, 14})
  assert_sealed_modification_error(table.insert, tbl, 'foo') -- push_back
  assert_sealed_modification_error(table.insert, tbl, 2, 'foo') -- insert
end

function tests.error_on_table_remove()
  local tbl = ujit.seal({5, 3, 14})
  assert_sealed_modification_error(table.remove, tbl, 2) -- remove
  assert_sealed_modification_error(table.remove, tbl, 3) -- pop_back
end

local function assert_weak_table(generator, sealed)
  local tbl = generator(sealed)
  assert(next(tbl) ~= nil)
  collectgarbage()
  if sealed then
    assert(next(tbl) ~= nil)
  else
    assert(next(tbl) == nil)
  end
end

-- A k-weak table referencing a sealed object should
-- preserve corresponding key-value pair over GC cycle.
function tests.weak_keys()
  local function create_kweak(do_seal)
    local tbl   = {}
    local kweak = setmetatable({ [tbl] = "foo" }, {__mode = 'k'})
    if do_seal then
      ujit.seal(kweak) -- implicitly calls GC, so tbl must be reachable
    end
    return kweak
  end
  assert_weak_table(create_kweak, true)
  assert_weak_table(create_kweak, false)
end

-- A v-weak table referencing a sealed object should
-- preserve corresponding key-value pair over GC cycle.
function tests.weak_values()
  local function create_vweak(do_seal)
    local tbl   = {}
    local vweak = setmetatable({ foo = tbl }, {__mode = 'v'})
    if do_seal then
      ujit.seal(vweak) -- implicitly calls GC, so tbl must be reachable
    end
    return vweak
  end
  assert_weak_table(create_vweak, true)
  assert_weak_table(create_vweak, false)
end

-- If an object is sealed, it is immutable
function tests.immutable()
    local t = ujit.seal({
        foo = {
            bar = {
                baz = "qux"
            }
        }
    })
    ujit.immutable(t) -- no-op here, but absolutely no harm
end

for k, v in pairs(tests) do
  if (type(k) == 'string' and type(v) == 'function')
  then
    v()
  end
end
