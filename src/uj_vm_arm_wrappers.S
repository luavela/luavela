/*
 * Copyright (C) 2020-2026 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

/* See comments in uj_vm_x86_wrappers.S */

#define MAKE_VM_ENTRY_FUNC(func_name, entry_func)    \
    .global #func_name;                              \
#func_name:                                          \
    .cfi_startproc;                                  \
    .cfi_personality 0x1b, uj_dwarf_personality;     \
    sub sp, sp, #512;                                \
    .cfi_def_cfa_offset 512;                         \
    stp x19, x20, [sp, 96];                          \
    stp x21, x22, [sp, 112];                         \
    stp x23, x24, [sp, 128];                         \
    stp x25, x26, [sp, 144];                         \
    stp x27, x28, [sp, 160];                         \
    str x29, [sp, 176];                              \
    stp q8, q9, [sp, 192];                           \
    stp q10, q11, [sp, 224];                         \
    stp q12, q13, [sp, 256];                         \
    stp q14, q15, [sp, 288];                         \
    str lr, [sp, #328]; /* entry_func_return_addr */ \
    .cfi_offset 30, -184; /* 512 - 328 */            \
    /* Store lr of interpreter frame: should point   \
     * right after bl instruction */                 \
    adr x5, . + 16;                                  \
    str x5, [sp, #336]; /* vm_return_addr */         \
    mov x5, sp;                                      \
    bl #entry_func;                                  \
    ldp x19, x20, [sp, 96];                          \
    ldp x21, x22, [sp, 112];                         \
    ldp x23, x24, [sp, 128];                         \
    ldp x25, x26, [sp, 144];                         \
    ldp x27, x28, [sp, 160];                         \
    ldr x29, [sp, 176];                              \
    ldp q8, q9, [sp, 192];                           \
    ldp q10, q11, [sp, 224];                         \
    ldp q12, q13, [sp, 256];                         \
    ldp q14, q15, [sp, 288];                         \
    ldr lr, [sp, 328];                               \
    .cfi_restore 30;                                 \
    add sp, sp, #512;                                \
    .cfi_def_cfa_offset 0;                           \
    ret;                                             \
    .cfi_endproc;

MAKE_VM_ENTRY_FUNC(uj_vm_prepare_call, uj_vm_call_entry)
MAKE_VM_ENTRY_FUNC(uj_vm_prepare_cpcall, uj_vm_cpcall_entry)
MAKE_VM_ENTRY_FUNC(uj_prepare_coroutine_bc_ret_z, uj_coroutine_bc_ret_z)
MAKE_VM_ENTRY_FUNC(uj_prepare_coroutine_vm_return, uj_coroutine_vm_return)
MAKE_VM_ENTRY_FUNC(uj_prepare_initial_coroutine_call, uj_initial_coroutine_call)

/* Landing pad in case an exception is caught by pcall/xpcall */
    .global uj_vm_ff_landing_pad
uj_vm_ff_landing_pad:
    mov x0, sp
    ldr lr, [sp, #336] /* Manually restore lr */
    b uj_vm_unwind_ff_eh

/* Landing pad in case an exception is caught by C API */
    .global uj_vm_c_landing_pad
uj_vm_c_landing_pad:
    mov x1, x0
    mov x0, sp
    ldr lr, [sp, #336] /* Manually restore lr */
    b uj_vm_unwind_c_eh

.section .note.GNU-stack,"",@progbits
