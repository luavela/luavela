# Building uJIT tests for C modules
# Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

enable_language(C ASM)

find_package(cmocka QUIET)
if (NOT cmocka_FOUND)
    message(WARNING "Package cmocka is not found. "
                    "If you want to run cmocka-based tests, please ensure that "
                    "cmocka version 1.1.1 or higher is installed "
                    "or cmocka_DIR is set.")
endif()


set(UJIT_TEST_TARGETS "")
set(SOURCE_CHECKED_BY_CLANG_TIDY "")

# "macro" is used here instead of function so that UJIT_TEST_TARGETS and
# SOURCE_CHECKED_BY_CLANG_TIDY variables behave like file scope global variables
macro(add_ujit_test_common TEST_NAME)
  set(TEST_TARGET "test_${TEST_NAME}")
  set(TEST_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test_${TEST_NAME}.c")

  add_executable(${TEST_TARGET} ${TEST_SOURCE_FILE})
  target_include_directories(${TEST_TARGET} PRIVATE ${UJIT_SRC_DIR})
  target_link_libraries(${TEST_TARGET} PRIVATE ${CMOCKA_LIBRARY})

  set(UJIT_TEST_TARGETS ${UJIT_TEST_TARGETS} ${TEST_TARGET} PARENT_SCOPE)
  set(SOURCE_CHECKED_BY_CLANG_TIDY
    ${SOURCE_CHECKED_BY_CLANG_TIDY} ${TEST_SOURCE_FILE} PARENT_SCOPE)
endmacro()

function(add_ujit_test_no_libujit TEST_NAME)
    add_ujit_test_common(${TEST_NAME})
endfunction()

function(add_ujit_test TEST_NAME)
    add_ujit_test_common(${TEST_NAME})

    # ${TARGET_C_FLAGS} contains flags and preprocessor definitions used for
    # building the core. Tests that link with libujit and use internal
    # headers should be compiled with exactly these settings.
    set_target_properties(${TEST_TARGET} PROPERTIES
        COMPILE_FLAGS "${TARGET_C_FLAGS}"
    )

    target_link_libraries(${TEST_TARGET} PRIVATE libujit_static)

    # TODO: remove this when libujit will link to dl with PUBLIC
    # in target_link_libraries
    target_link_libraries(${TEST_TARGET} PRIVATE dl)
endfunction()

################################################################################
# Sample to add a test to the suite (see test_example.c for details).
add_ujit_test(  # Use add_ujit_test_no_libujit if link to libujit is not needed
  example  # test name; assumes that test_example.c exists
           # creates test_example target
)
################################################################################

################################################################################
# Tests which don't link to libujit
################################################################################
add_ujit_test_no_libujit(leb128)
add_ujit_test_no_libujit(random)
add_ujit_test_no_libujit(str)

set(UJIT_UTILS_DIR "${UJIT_SRC_DIR}/utils")

add_ujit_test_no_libujit(math_fold)
target_sources(test_math_fold PUBLIC ${UJIT_UTILS_DIR}/uj_math.c)
target_link_libraries(test_math_fold PRIVATE m)

add_ujit_test_no_libujit(strscan)
target_sources(test_strscan PUBLIC ${UJIT_UTILS_DIR}/lj_char.c)

################################################################################
# Tests which link to and use libujit
################################################################################
add_ujit_test(api_args)
add_ujit_test(bottom_dummy)
add_ujit_test(cci_immutable)
add_ujit_test(concat_and_gc)
add_ujit_test(crc)
add_ujit_test(cstr)
add_ujit_test(emit_sse2)
add_ujit_test(errmem)
add_ujit_test(ext_events)
add_ujit_test(gc_traversal_mm)
add_ujit_test(gc_traverse_stack)
add_ujit_test(jitpairs)
add_ujit_test(lua_alloc)
add_ujit_test(lua_concat)
add_ujit_test(lua_coverage)
add_ujit_test(lua_debug)
add_ujit_test(lua_equal)
add_ujit_test(lua_getfield)
add_ujit_test(lua_gettable)
add_ujit_test(lua_hotcnt)
add_ujit_test(lua_iprof)
add_ujit_test(lua_lessthan)
add_ujit_test(lua_setfield)
add_ujit_test(lua_settable)
add_ujit_test(lua_timeout)
add_ujit_test(lua_yield)
add_ujit_test(luae_createstate)
add_ujit_test(luae_deepcopy)
add_ujit_test(luae_immutable)
add_ujit_test(luae_iterate)
add_ujit_test(luae_requiref)
add_ujit_test(luae_seal)
add_ujit_test(luae_table)
add_ujit_test(lual_openlib)
add_ujit_test(profiler_and_timeouts)
add_ujit_test(sbuf)
add_ujit_test(stack_resize)
add_ujit_test(store_num_key)
add_ujit_test(vmstate)

################################################################################
# Tests which link require custom linkage
################################################################################
# TODO: re-enable after freezing during the test is fixed
# add_ujit_test(profiler)
# target_link_libraries(test_profiler PRIVATE pthread)

set(UJIT_C_TEST_TARGETS ${UJIT_TEST_TARGETS} PARENT_SCOPE)

#
# clang-tidy
#

add_clang_tidy_check(clang_tidy_tests
  FILES
    ${SOURCE_CHECKED_BY_CLANG_TIDY}
)
