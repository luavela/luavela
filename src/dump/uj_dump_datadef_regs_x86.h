/*
 * Platform-specific register names: x86_64
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

/* x86_64 general purpose register names. */
LJ_DATADEF const char *const dump_reg_gpr_names[] = {
	"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
	"r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};

/* x86_64 SSE register names. */
LJ_DATADEF const char *const dump_reg_fpr_names[] = {
	"xmm0", "xmm1", "xmm2",	 "xmm3",  "xmm4",  "xmm5",  "xmm6",  "xmm7",
	"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"};
