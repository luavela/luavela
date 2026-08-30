#!/usr/bin/python3

# This script verifies that tail-call optimization is properly applied
# by compiler for every bytecode, built-in and some helper functions.
#
# Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

import re
import sys
import argparse
import platform
import itertools

FUNC_HEADER_RE = re.compile(r'^[0-9a-fA-F]{16}\s<[^>]+>:$')
ARM_CALL_TEMPLATE = r'^.*blr?{}.*'
X86_CALL_TEMPLATE = r'^.*call{}.*'

# Each entry contains function name and an array of allowed calls.
# Most functions share the same call set on both x86 and ARM.
# For the few exceptions, ARM-specific calls are stored in the third
# array - see uj_BC_FUNCC for example.
KNOWN_FUNCTIONS = [
    # Bytecodes
    ['uj_BC_ISLT', []],
    ['uj_BC_ISGE', []],
    ['uj_BC_ISLE', []],
    ['uj_BC_ISGT', []],
    ['uj_BC_ISEQV', []],
    ['uj_BC_ISNEV', []],
    ['uj_BC_ISEQS', []],
    ['uj_BC_ISNES', []],
    ['uj_BC_ISEQN', []],
    ['uj_BC_ISNEN', []],
    ['uj_BC_ISEQP', []],
    ['uj_BC_ISNEP', []],
    ['uj_BC_ISTC',  []],
    ['uj_BC_ISFC', []],
    ['uj_BC_IST', []],
    ['uj_BC_ISF', []],
    ['uj_BC_MOV', []],
    ['uj_BC_NOT', []],
    ['uj_BC_UNM', []],
    ['uj_BC_LEN', ['lj_tab_len']],
    ['uj_BC_ADD', []],
    ['uj_BC_SUB', []],
    ['uj_BC_MUL', []],
    ['uj_BC_DIV', []],
    ['uj_BC_MOD', []],
    ['uj_BC_POW', ['pow@plt']],
    ['uj_BC_CAT', ['uj_meta_cat', 'lj_gc_check_fixtop', 'uj_meta_call']],
    ['uj_BC_KSTR', []],
    ['uj_BC_KCDATA', []],
    ['uj_BC_KSHORT', []],
    ['uj_BC_KNUM', []],
    ['uj_BC_KPRI', []],
    ['uj_BC_KNIL', []],
    ['uj_BC_UGET', []],
    ['uj_BC_USETV', ['lj_gc_barrieruv']],
    ['uj_BC_USETS', ['lj_gc_barrieruv']],
    ['uj_BC_USETN', []],
    ['uj_BC_USETP', []],
    ['uj_BC_UCLO', ['uj_upval_close']],
    ['uj_BC_FNEW', ['uj_func_newL_gc']],
    ['uj_BC_TNEW', ['lj_tab_new', 'lj_gc_check_fixtop']],
    ['uj_BC_TDUP', ['lj_gc_check_fixtop', 'lj_tab_dup']],
    ['uj_BC_GGET', []],
    ['uj_BC_GSET', ['uj_err']],
    ['uj_BC_TGETV', []],
    ['uj_BC_TGETS', []],
    ['uj_BC_TGETB', []],
    ['uj_BC_TSETV', ['uj_err']],
    ['uj_BC_TSETS', ['uj_err']],
    ['uj_BC_TSETB', ['uj_err']],
    ['uj_BC_TSETM', ['lj_tab_reasize', 'uj_err']],
    ['uj_BC_CALLM', ['uj_meta_call']],
    ['uj_BC_CALL', ['uj_meta_call']],
    ['uj_BC_CALLMT', ['uj_meta_call']],
    ['uj_BC_CALLT', ['uj_meta_call']],
    ['uj_BC_ITERC', ['uj_meta_call']],
    ['uj_BC_ITERN', []],
    ['uj_BC_VARG', ['uj_state_stack_grow']],
    ['uj_BC_ISNEXT', []],
    ['uj_BC_RETM', []],
    ['uj_BC_RET', []],
    ['uj_BC_RET0', []],
    ['uj_BC_RET1', []],
    ['uj_BC_HOTCNT', []],
    ['uj_BC_COVERG', ['uj_coverage_stream_line']],
    ['uj_BC_FORI', ['uj_meta_for', 'uj_timerint_ticks',
                    'uj_throw_timeout']],
    ['uj_BC_IFORL', ['uj_timerint_ticks', 'uj_throw_timeout']],
    ['uj_BC_IITERL', ['uj_timerint_ticks', 'uj_throw_timeout']],
    ['uj_BC_IITRNL', ['uj_timerint_ticks', 'uj_throw_timeout']],
    ['uj_BC_ILOOP', ['uj_timerint_ticks', 'uj_throw_timeout']],
    ['uj_BC_JMP', []],
    ['uj_BC_IFUNCF', ['uj_state_stack_grow', 'uj_timerint_ticks',
                      'uj_throw_timeout']],
    ['uj_BC_IFUNCV', ['uj_state_stack_grow', 'uj_timerint_ticks',
                      'uj_throw_timeout']],
    ['uj_BC_FUNCC', [r'PTR\s\[r.*\]', 'uj_state_stack_grow',
                     'uj_timerint_ticks', 'uj_throw_timeout'],
                    [r'x\d{1,2}', 'uj_state_stack_grow', 'abort@plt',
                     'uj_timerint_ticks', 'uj_throw_timeout']],
    ['uj_BC_FUNCCW', [r'PTR\s\[r.*\]', 'uj_state_stack_grow',
                      'uj_timerint_ticks', 'uj_throw_timeout'],
                     [r'x\d{1,2}', 'uj_state_stack_grow', 'abort@plt',
                      'uj_timerint_ticks', 'uj_throw_timeout']],
    # Fast functions
    ['uj_ff_assert', ['memmove@plt']],
    ['uj_ff_type', []],
    ['uj_ff_next', ['lj_tab_next']],
    ['uj_ff_pairs', []],
    ['uj_ff_ipairs_aux', ['lj_tab_getinth']],
    ['uj_ff_ipairs', []],
    ['uj_ff_getmetatable', ['lj_tab_getstr']],
    ['uj_ff_setmetatable', []],
    ['uj_ff_rawget', ['lj_tab_get', ]],
    ['uj_ff_tonumber', []],
    ['uj_ff_tostring', ['uj_str_fromnumber', 'lj_gc_step']],
    ['uj_ff_pcall', ['uj_meta_call']],
    ['uj_ff_xpcall', ['uj_meta_call']],
    ['uj_ff_coroutine_yield', ['uj_timerint_ticks', 'uj_throw_timeout'],
                              ['abort@plt', 'uj_timerint_ticks',
                               'uj_throw_timeout']],
    ['uj_ff_coroutine_resume', ['uj_prepare_coroutine_bc_ret_z',
                                'uj_prepare_coroutine_vm_return',
                                'uj_prepare_initial_coroutine_call',
                                'uj_state_stack_grow']],
    ['uj_ff_coroutine_wrap_aux', ['uj_prepare_coroutine_vm_return',
                                  'uj_prepare_coroutine_bc_ret_z',
                                  'lj_ffh_coroutine_wrap_err',
                                  'uj_prepare_initial_coroutine_call',
                                  'uj_state_stack_grow']],
    ['uj_ff_math_fabs', []],
    ['uj_ff_math_floor', []],
    ['uj_ff_math_ceil', []],
    ['uj_ff_math_sqrt', ['sqrt@plt']],
    ['uj_ff_math_log10', ['log10@plt']],
    ['uj_ff_math_exp', ['exp@plt']],
    ['uj_ff_math_sin', ['sin@plt']],
    ['uj_ff_math_cos', ['cos@plt']],
    ['uj_ff_math_tan', ['tan@plt']],
    ['uj_ff_math_asin', ['asin@plt']],
    ['uj_ff_math_acos', ['acos@plt']],
    ['uj_ff_math_atan', ['atan@plt']],
    ['uj_ff_math_sinh', ['sinh@plt']],
    ['uj_ff_math_cosh', ['cosh@plt']],
    ['uj_ff_math_tanh', ['tanh@plt']],
    ['uj_ff_math_frexp', ['frexp@plt']],
    ['uj_ff_math_modf', ['modf@plt']],
    ['uj_ff_math_rad_deg', []],
    ['uj_ff_math_log', ['log@plt']],
    ['uj_ff_math_atan2', ['atan2@plt']],
    ['uj_ff_math_pow', ['pow@plt']],
    ['uj_ff_math_fmod', ['fmod@plt']],
    ['uj_ff_math_ldexp', ['ldexp@plt']],
    ['uj_ff_math_min', ['fmin@plt'], []],
    ['uj_ff_math_max', ['fmax@plt'], []],
    ['uj_ff_bit_tobit', []],
    ['uj_ff_bit_bnot', []],
    ['uj_ff_bit_bswap', []],
    ['uj_ff_bit_lshift', []],
    ['uj_ff_bit_rshift', []],
    ['uj_ff_bit_arshift', []],
    ['uj_ff_bit_rol', []],
    ['uj_ff_bit_ror', []],
    ['uj_ff_bit_band', []],
    ['uj_ff_bit_bor', []],
    ['uj_ff_bit_bxor', []],
    ['uj_ff_string_len', []],
    ['uj_ff_string_byte', []],
    ['uj_ff_string_char', ['lj_gc_step', 'uj_str_new']],
    ['uj_ff_string_sub', ['lj_gc_step', 'uj_str_new']],
    ['uj_ff_string_rep', ['lj_gc_step', 'uj_sbuf_reserve',
                          'memset@plt', 'uj_str_new']],
    ['uj_ff_string_reverse', ['lj_gc_step', 'uj_sbuf_reserve',
                              'uj_str_new']],
    ['uj_ff_string_lower', []],
    ['uj_ff_string_upper', []],
    ['uj_ff_table_getn', ['lj_tab_len']],
    # Helpers
    ['vm_vmeta_tget_handler', ['uj_meta_tget']],
    ['vm_vmeta_tset_handler', ['uj_meta_tset']],
    ['vm_tgets_handler', []],
    ['vm_tsets_handler', ['lj_tab_newkey']],
    ['vm_vmeta_arith_vv', ['uj_meta_arith', 'uj_meta_call']],
    ['vm_fff_res_', []],
    ['uj_vm_call_entry', ['uj_meta_call']],
    ['uj_vm_call', []],
    ['uj_vm_pcall', ['uj_vm_prepare_call']],
    ['uj_vm_cpcall_entry', ['r..', 'uj_meta_call'],
                           [r'x\d{1,2}', 'uj_meta_call', 'abort@plt']],
    ['uj_vm_cpcall', ['uj_vm_prepare_cpcall']],
    ['vm_fff_res_.constprop.0', []],
    ['vm_fff_fallback.isra.0', [r'PTR\s\[r.*\]', 'uj_state_stack_grow',
                                'uj_meta_call'],
                               [r'x\d{1,2}', 'uj_state_stack_grow',
                                'uj_meta_call']],
    ['vm_return', [], ['abort@plt']],
    ['vm_returnp', []],
    ['vm_cont_dispatch', [], ['abort@plt']],
    ['uj_vm_cont_ra', []],
    ['uj_vm_cont_nop', []],
    ['uj_vm_cont_condt', []],
    ['uj_vm_cont_condf', []],
    ['uj_vm_cont_cat', ['uj_meta_cat', 'lj_gc_check_fixtop', 'uj_meta_call']]
]


def verify_call_instructions(cinterp_function, disassembled_calls,
                             call_template, is_arm64, verbose):
    func_name, expected_calls, *arm_specific_calls = cinterp_function
    matched_calls = set()
    num_allowed_disassembled_calls = 0

    if is_arm64 and arm_specific_calls:
        # ARM64 build has different call set for this function
        expected_calls = arm_specific_calls[0]

    for disassembled_call, expected_call in itertools.product(
            disassembled_calls, expected_calls):
        call_re = call_template.format(fr'.*{expected_call}')

        if re.search(call_re, disassembled_call):
            num_allowed_disassembled_calls += 1
            matched_calls.add(expected_call)

    if num_allowed_disassembled_calls != len(disassembled_calls):
        raise RuntimeError(
            f'Failed to match all calls for {func_name}: {disassembled_calls}')

    unmatched_calls = set(expected_calls) - matched_calls
    if unmatched_calls:
        raise RuntimeError(
            f'Missing expected calls in {func_name}: {unmatched_calls}. '
            f'Disassembled calls: {disassembled_calls}')

    if not verbose:
        return

    if len(disassembled_calls) == 0:
        print(f'Checked {func_name}: no calls found')
    else:
        print(f'Checked {func_name}:')
        for disassembled_call in disassembled_calls:
            print(f'  -> {disassembled_call.rstrip()}')


def check_for_known_functions(line, already_checked_funcs):
    for func_info in KNOWN_FUNCTIONS:
        name = func_info[0]

        if not re.match(fr'^[0-9a-fA-F]{{16}}\s<{name}>:$', line):
            continue

        if name in already_checked_funcs:
            raise RuntimeError(f'Symbol \'{name}\' has already been checked')

        return func_info

    return None


def parse_disassembled_listing(call_template, is_arm64, verbose):
    current_func = None
    disassembled_calls = []
    already_checked_funcs = set()
    call_re = re.compile(call_template.format(r'\s+'))

    for line in sys.stdin:
        if current_func is None:
            if not FUNC_HEADER_RE.match(line):
                continue

            current_func = \
                check_for_known_functions(line, already_checked_funcs)
            continue

        if line == '\n':
            # Reached end of function
            verify_call_instructions(current_func, disassembled_calls,
                                     call_template, is_arm64, verbose)
            already_checked_funcs.add(current_func[0])
            disassembled_calls = []
            current_func = None
            continue

        if not call_re.match(line):
            continue

        disassembled_calls.append(line)

    missing = set(func[0] for func in KNOWN_FUNCTIONS) - already_checked_funcs
    if missing:
        raise RuntimeError(
            f'The following mandatory symbols were not found: {missing}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()
    arch = platform.machine().lower()

    match arch:
        case 'x86_64' | 'amd64':
            is_arm64 = False
        case 'aarch64' | 'arm64':
            is_arm64 = True
        case _:
            raise RuntimeError(f'Unsupported arch: {arch}')

    call_template = ARM_CALL_TEMPLATE if is_arm64 else X86_CALL_TEMPLATE
    parse_disassembled_listing(call_template, is_arm64, args.verbose)
