codes = true
read_globals = {
    -- Test.More
    'plan',
    'done_testing',
    'skip_all',
    'BAIL_OUT',
    'ok',
    'nok',
    'is',
    'isnt',
    'like',
    'unlike',
    'cmp_ok',
    'type_ok',
    'subtest',
    'pass',
    'fail',
    'require_ok',
    'eq_array',
    'is_deeply',
    'error_is',
    'error_like',
    'lives_ok',
    'diag',
    'note',
    'skip',
    'todo_skip',
    'skip_rest',
    'todo',
    -- LuaVela
    'ujit',
}

files['test_lua/000-sanity.t'].ignore = { '111', '113' }
files['test_lua/001-if.t'].ignore = { '111', '113', '511' }
files['test_lua/002-table.t'].ignore = { '111', '113' }
files['test_lua/014-fornum.t'].ignore = { '512' }
files['test_lua/201-assign.t'].ignore = { '411/my_i', '531', '532' }
files['test_lua/211-scope.t'].ignore = { '421' }
files['test_lua/231-metatable.t'].ignore = { '421', '431' }
files['test_lua/308-io.t'].ignore = { '512' }
files['test_lua/320-stdin.t'].ignore = { '631' }

files['test_lua/102-function.t'].globals = { 'print' }
files['test_lua/108-userdata.t'].globals = { 'io' }
files['test_lua/200-examples.t'].globals = { 'factorial' }
files['test_lua/201-assign.t'].globals = { 'b' }
files['test_lua/211-scope.t'].globals = { 'x' }
files['test_lua/231-metatable.t'].globals = { 'new_a' }
files['test_lua/301-basic.t'].globals = { 'norm', 'twice', 'foo', 'bar', 'baz', 'i', 'X', 'a', 'g', 'save' }
files['test_lua/303-package.t'].globals = { 'complex', 'cplx', 'a', 'm', 'mod', 'modz' }
files['test_lua/304-string.t'].globals = { 'name', 'status' }
files['test_lua/402-ffi.t'].globals = { 'ffi' }

