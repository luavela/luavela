/*
 * This is a part of uJIT's testing suite.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * All cross-validation is done with https://defuse.ca/online-x86-assembler.htm
 */

#include <string.h>

#include "test_common_lua.h"
#include "lj_def.h"
#include "jit/lj_target_x86.h"
#include "jit/emit/uj_emit_sse2.h"

#define MC_BUFFER_SIZE 16

/* movdqu xmm, [gpr] */
static void test_movxmmrm(void **state)
{
	UNUSED_STATE(state);

	uint8_t mov_xmm0_rax[] = {0xF3, 0x0F, 0x6F, 0x00};
	uint8_t mov_xmm0_rcx[] = {0xF3, 0x0F, 0x6F, 0x01};
	uint8_t mov_xmm0_rdx[] = {0xF3, 0x0F, 0x6F, 0x02};
	uint8_t mov_xmm0_rbx[] = {0xF3, 0x0F, 0x6F, 0x03};
	uint8_t mov_xmm0_rsp[] = {0xF3, 0x0F, 0x6F, 0x04, 0x24};
	uint8_t mov_xmm0_rbp[] = {0xF3, 0x0F, 0x6F, 0x45, 0x00};
	uint8_t mov_xmm0_rsi[] = {0xF3, 0x0F, 0x6F, 0x06};
	uint8_t mov_xmm0_rdi[] = {0xF3, 0x0F, 0x6F, 0x07};
	uint8_t mov_xmm0_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x00};
	uint8_t mov_xmm0_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x01};
	uint8_t mov_xmm0_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x02};
	uint8_t mov_xmm0_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x03};
	uint8_t mov_xmm0_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x04, 0x24};
	uint8_t mov_xmm0_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x45, 0x00};
	uint8_t mov_xmm0_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x06};
	uint8_t mov_xmm0_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x07};

	uint8_t mov_xmm1_rax[] = {0xF3, 0x0F, 0x6F, 0x08};
	uint8_t mov_xmm1_rcx[] = {0xF3, 0x0F, 0x6F, 0x09};
	uint8_t mov_xmm1_rdx[] = {0xF3, 0x0F, 0x6F, 0x0A};
	uint8_t mov_xmm1_rbx[] = {0xF3, 0x0F, 0x6F, 0x0B};
	uint8_t mov_xmm1_rsp[] = {0xF3, 0x0F, 0x6F, 0x0C, 0x24};
	uint8_t mov_xmm1_rbp[] = {0xF3, 0x0F, 0x6F, 0x4D, 0x00};
	uint8_t mov_xmm1_rsi[] = {0xF3, 0x0F, 0x6F, 0x0E};
	uint8_t mov_xmm1_rdi[] = {0xF3, 0x0F, 0x6F, 0x0F};
	uint8_t mov_xmm1_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x08};
	uint8_t mov_xmm1_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x09};
	uint8_t mov_xmm1_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x0A};
	uint8_t mov_xmm1_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x0B};
	uint8_t mov_xmm1_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x0C, 0x24};
	uint8_t mov_xmm1_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x4D, 0x00};
	uint8_t mov_xmm1_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x0E};
	uint8_t mov_xmm1_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x0F};

	uint8_t mov_xmm2_rax[] = {0xF3, 0x0F, 0x6F, 0x10};
	uint8_t mov_xmm2_rcx[] = {0xF3, 0x0F, 0x6F, 0x11};
	uint8_t mov_xmm2_rdx[] = {0xF3, 0x0F, 0x6F, 0x12};
	uint8_t mov_xmm2_rbx[] = {0xF3, 0x0F, 0x6F, 0x13};
	uint8_t mov_xmm2_rsp[] = {0xF3, 0x0F, 0x6F, 0x14, 0x24};
	uint8_t mov_xmm2_rbp[] = {0xF3, 0x0F, 0x6F, 0x55, 0x00};
	uint8_t mov_xmm2_rsi[] = {0xF3, 0x0F, 0x6F, 0x16};
	uint8_t mov_xmm2_rdi[] = {0xF3, 0x0F, 0x6F, 0x17};
	uint8_t mov_xmm2_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x10};
	uint8_t mov_xmm2_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x11};
	uint8_t mov_xmm2_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x12};
	uint8_t mov_xmm2_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x13};
	uint8_t mov_xmm2_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x14, 0x24};
	uint8_t mov_xmm2_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x55, 0x00};
	uint8_t mov_xmm2_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x16};
	uint8_t mov_xmm2_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x17};

	uint8_t mov_xmm3_rax[] = {0xF3, 0x0F, 0x6F, 0x18};
	uint8_t mov_xmm3_rcx[] = {0xF3, 0x0F, 0x6F, 0x19};
	uint8_t mov_xmm3_rdx[] = {0xF3, 0x0F, 0x6F, 0x1A};
	uint8_t mov_xmm3_rbx[] = {0xF3, 0x0F, 0x6F, 0x1B};
	uint8_t mov_xmm3_rsp[] = {0xF3, 0x0F, 0x6F, 0x1C, 0x24};
	uint8_t mov_xmm3_rbp[] = {0xF3, 0x0F, 0x6F, 0x5D, 0x00};
	uint8_t mov_xmm3_rsi[] = {0xF3, 0x0F, 0x6F, 0x1E};
	uint8_t mov_xmm3_rdi[] = {0xF3, 0x0F, 0x6F, 0x1F};
	uint8_t mov_xmm3_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x18};
	uint8_t mov_xmm3_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x19};
	uint8_t mov_xmm3_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x1A};
	uint8_t mov_xmm3_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x1B};
	uint8_t mov_xmm3_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x1C, 0x24};
	uint8_t mov_xmm3_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x5D, 0x00};
	uint8_t mov_xmm3_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x1E};
	uint8_t mov_xmm3_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x1F};

	uint8_t mov_xmm4_rax[] = {0xF3, 0x0F, 0x6F, 0x20};
	uint8_t mov_xmm4_rcx[] = {0xF3, 0x0F, 0x6F, 0x21};
	uint8_t mov_xmm4_rdx[] = {0xF3, 0x0F, 0x6F, 0x22};
	uint8_t mov_xmm4_rbx[] = {0xF3, 0x0F, 0x6F, 0x23};
	uint8_t mov_xmm4_rsp[] = {0xF3, 0x0F, 0x6F, 0x24, 0x24};
	uint8_t mov_xmm4_rbp[] = {0xF3, 0x0F, 0x6F, 0x65, 0x00};
	uint8_t mov_xmm4_rsi[] = {0xF3, 0x0F, 0x6F, 0x26};
	uint8_t mov_xmm4_rdi[] = {0xF3, 0x0F, 0x6F, 0x27};
	uint8_t mov_xmm4_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x20};
	uint8_t mov_xmm4_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x21};
	uint8_t mov_xmm4_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x22};
	uint8_t mov_xmm4_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x23};
	uint8_t mov_xmm4_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x24, 0x24};
	uint8_t mov_xmm4_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x65, 0x00};
	uint8_t mov_xmm4_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x26};
	uint8_t mov_xmm4_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x27};

	uint8_t mov_xmm5_rax[] = {0xF3, 0x0F, 0x6F, 0x28};
	uint8_t mov_xmm5_rcx[] = {0xF3, 0x0F, 0x6F, 0x29};
	uint8_t mov_xmm5_rdx[] = {0xF3, 0x0F, 0x6F, 0x2A};
	uint8_t mov_xmm5_rbx[] = {0xF3, 0x0F, 0x6F, 0x2B};
	uint8_t mov_xmm5_rsp[] = {0xF3, 0x0F, 0x6F, 0x2C, 0x24};
	uint8_t mov_xmm5_rbp[] = {0xF3, 0x0F, 0x6F, 0x6D, 0x00};
	uint8_t mov_xmm5_rsi[] = {0xF3, 0x0F, 0x6F, 0x2E};
	uint8_t mov_xmm5_rdi[] = {0xF3, 0x0F, 0x6F, 0x2F};
	uint8_t mov_xmm5_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x28};
	uint8_t mov_xmm5_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x29};
	uint8_t mov_xmm5_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x2A};
	uint8_t mov_xmm5_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x2B};
	uint8_t mov_xmm5_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x2C, 0x24};
	uint8_t mov_xmm5_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x6D, 0x00};
	uint8_t mov_xmm5_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x2E};
	uint8_t mov_xmm5_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x2F};

	uint8_t mov_xmm6_rax[] = {0xF3, 0x0F, 0x6F, 0x30};
	uint8_t mov_xmm6_rcx[] = {0xF3, 0x0F, 0x6F, 0x31};
	uint8_t mov_xmm6_rdx[] = {0xF3, 0x0F, 0x6F, 0x32};
	uint8_t mov_xmm6_rbx[] = {0xF3, 0x0F, 0x6F, 0x33};
	uint8_t mov_xmm6_rsp[] = {0xF3, 0x0F, 0x6F, 0x34, 0x24};
	uint8_t mov_xmm6_rbp[] = {0xF3, 0x0F, 0x6F, 0x75, 0x00};
	uint8_t mov_xmm6_rsi[] = {0xF3, 0x0F, 0x6F, 0x36};
	uint8_t mov_xmm6_rdi[] = {0xF3, 0x0F, 0x6F, 0x37};
	uint8_t mov_xmm6_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x30};
	uint8_t mov_xmm6_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x31};
	uint8_t mov_xmm6_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x32};
	uint8_t mov_xmm6_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x33};
	uint8_t mov_xmm6_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x34, 0x24};
	uint8_t mov_xmm6_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x75, 0x00};
	uint8_t mov_xmm6_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x36};
	uint8_t mov_xmm6_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x37};

	uint8_t mov_xmm7_rax[] = {0xF3, 0x0F, 0x6F, 0x38};
	uint8_t mov_xmm7_rcx[] = {0xF3, 0x0F, 0x6F, 0x39};
	uint8_t mov_xmm7_rdx[] = {0xF3, 0x0F, 0x6F, 0x3A};
	uint8_t mov_xmm7_rbx[] = {0xF3, 0x0F, 0x6F, 0x3B};
	uint8_t mov_xmm7_rsp[] = {0xF3, 0x0F, 0x6F, 0x3C, 0x24};
	uint8_t mov_xmm7_rbp[] = {0xF3, 0x0F, 0x6F, 0x7D, 0x00};
	uint8_t mov_xmm7_rsi[] = {0xF3, 0x0F, 0x6F, 0x3E};
	uint8_t mov_xmm7_rdi[] = {0xF3, 0x0F, 0x6F, 0x3F};
	uint8_t mov_xmm7_r8[] = {0xF3, 0x41, 0x0F, 0x6F, 0x38};
	uint8_t mov_xmm7_r9[] = {0xF3, 0x41, 0x0F, 0x6F, 0x39};
	uint8_t mov_xmm7_r10[] = {0xF3, 0x41, 0x0F, 0x6F, 0x3A};
	uint8_t mov_xmm7_r11[] = {0xF3, 0x41, 0x0F, 0x6F, 0x3B};
	uint8_t mov_xmm7_r12[] = {0xF3, 0x41, 0x0F, 0x6F, 0x3C, 0x24};
	uint8_t mov_xmm7_r13[] = {0xF3, 0x41, 0x0F, 0x6F, 0x7D, 0x00};
	uint8_t mov_xmm7_r14[] = {0xF3, 0x41, 0x0F, 0x6F, 0x3E};
	uint8_t mov_xmm7_r15[] = {0xF3, 0x41, 0x0F, 0x6F, 0x3F};

	uint8_t mov_xmm8_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x00};
	uint8_t mov_xmm8_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x01};
	uint8_t mov_xmm8_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x02};
	uint8_t mov_xmm8_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x03};
	uint8_t mov_xmm8_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x04, 0x24};
	uint8_t mov_xmm8_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x45, 0x00};
	uint8_t mov_xmm8_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x06};
	uint8_t mov_xmm8_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x07};
	uint8_t mov_xmm8_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x00};
	uint8_t mov_xmm8_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x01};
	uint8_t mov_xmm8_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x02};
	uint8_t mov_xmm8_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x03};
	uint8_t mov_xmm8_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x04, 0x24};
	uint8_t mov_xmm8_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x45, 0x00};
	uint8_t mov_xmm8_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x06};
	uint8_t mov_xmm8_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x07};

	uint8_t mov_xmm9_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x08};
	uint8_t mov_xmm9_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x09};
	uint8_t mov_xmm9_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x0A};
	uint8_t mov_xmm9_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x0B};
	uint8_t mov_xmm9_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x0C, 0x24};
	uint8_t mov_xmm9_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x4D, 0x00};
	uint8_t mov_xmm9_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x0E};
	uint8_t mov_xmm9_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x0F};
	uint8_t mov_xmm9_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x08};
	uint8_t mov_xmm9_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x09};
	uint8_t mov_xmm9_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x0A};
	uint8_t mov_xmm9_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x0B};
	uint8_t mov_xmm9_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x0C, 0x24};
	uint8_t mov_xmm9_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x4D, 0x00};
	uint8_t mov_xmm9_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x0E};
	uint8_t mov_xmm9_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x0F};

	uint8_t mov_xmm10_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x10};
	uint8_t mov_xmm10_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x11};
	uint8_t mov_xmm10_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x12};
	uint8_t mov_xmm10_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x13};
	uint8_t mov_xmm10_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x14, 0x24};
	uint8_t mov_xmm10_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x55, 0x00};
	uint8_t mov_xmm10_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x16};
	uint8_t mov_xmm10_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x17};
	uint8_t mov_xmm10_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x10};
	uint8_t mov_xmm10_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x11};
	uint8_t mov_xmm10_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x12};
	uint8_t mov_xmm10_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x13};
	uint8_t mov_xmm10_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x14, 0x24};
	uint8_t mov_xmm10_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x55, 0x00};
	uint8_t mov_xmm10_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x16};
	uint8_t mov_xmm10_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x17};

	uint8_t mov_xmm11_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x18};
	uint8_t mov_xmm11_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x19};
	uint8_t mov_xmm11_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x1A};
	uint8_t mov_xmm11_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x1B};
	uint8_t mov_xmm11_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x1C, 0x24};
	uint8_t mov_xmm11_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x5D, 0x00};
	uint8_t mov_xmm11_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x1E};
	uint8_t mov_xmm11_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x1F};
	uint8_t mov_xmm11_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x18};
	uint8_t mov_xmm11_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x19};
	uint8_t mov_xmm11_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x1A};
	uint8_t mov_xmm11_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x1B};
	uint8_t mov_xmm11_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x1C, 0x24};
	uint8_t mov_xmm11_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x5D, 0x00};
	uint8_t mov_xmm11_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x1E};
	uint8_t mov_xmm11_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x1F};

	uint8_t mov_xmm12_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x20};
	uint8_t mov_xmm12_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x21};
	uint8_t mov_xmm12_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x22};
	uint8_t mov_xmm12_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x23};
	uint8_t mov_xmm12_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x24, 0x24};
	uint8_t mov_xmm12_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x65, 0x00};
	uint8_t mov_xmm12_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x26};
	uint8_t mov_xmm12_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x27};
	uint8_t mov_xmm12_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x20};
	uint8_t mov_xmm12_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x21};
	uint8_t mov_xmm12_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x22};
	uint8_t mov_xmm12_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x23};
	uint8_t mov_xmm12_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x24, 0x24};
	uint8_t mov_xmm12_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x65, 0x00};
	uint8_t mov_xmm12_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x26};
	uint8_t mov_xmm12_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x27};

	uint8_t mov_xmm13_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x28};
	uint8_t mov_xmm13_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x29};
	uint8_t mov_xmm13_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x2A};
	uint8_t mov_xmm13_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x2B};
	uint8_t mov_xmm13_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x2C, 0x24};
	uint8_t mov_xmm13_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x6D, 0x00};
	uint8_t mov_xmm13_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x2E};
	uint8_t mov_xmm13_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x2F};
	uint8_t mov_xmm13_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x28};
	uint8_t mov_xmm13_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x29};
	uint8_t mov_xmm13_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x2A};
	uint8_t mov_xmm13_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x2B};
	uint8_t mov_xmm13_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x2C, 0x24};
	uint8_t mov_xmm13_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x6D, 0x00};
	uint8_t mov_xmm13_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x2E};
	uint8_t mov_xmm13_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x2F};

	uint8_t mov_xmm14_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x30};
	uint8_t mov_xmm14_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x31};
	uint8_t mov_xmm14_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x32};
	uint8_t mov_xmm14_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x33};
	uint8_t mov_xmm14_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x34, 0x24};
	uint8_t mov_xmm14_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x75, 0x00};
	uint8_t mov_xmm14_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x36};
	uint8_t mov_xmm14_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x37};
	uint8_t mov_xmm14_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x30};
	uint8_t mov_xmm14_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x31};
	uint8_t mov_xmm14_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x32};
	uint8_t mov_xmm14_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x33};
	uint8_t mov_xmm14_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x34, 0x24};
	uint8_t mov_xmm14_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x75, 0x00};
	uint8_t mov_xmm14_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x36};
	uint8_t mov_xmm14_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x37};

	uint8_t mov_xmm15_rax[] = {0xF3, 0x44, 0x0F, 0x6F, 0x38};
	uint8_t mov_xmm15_rcx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x39};
	uint8_t mov_xmm15_rdx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x3A};
	uint8_t mov_xmm15_rbx[] = {0xF3, 0x44, 0x0F, 0x6F, 0x3B};
	uint8_t mov_xmm15_rsp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x3C, 0x24};
	uint8_t mov_xmm15_rbp[] = {0xF3, 0x44, 0x0F, 0x6F, 0x7D, 0x00};
	uint8_t mov_xmm15_rsi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x3E};
	uint8_t mov_xmm15_rdi[] = {0xF3, 0x44, 0x0F, 0x6F, 0x3F};
	uint8_t mov_xmm15_r8[] = {0xF3, 0x45, 0x0F, 0x6F, 0x38};
	uint8_t mov_xmm15_r9[] = {0xF3, 0x45, 0x0F, 0x6F, 0x39};
	uint8_t mov_xmm15_r10[] = {0xF3, 0x45, 0x0F, 0x6F, 0x3A};
	uint8_t mov_xmm15_r11[] = {0xF3, 0x45, 0x0F, 0x6F, 0x3B};
	uint8_t mov_xmm15_r12[] = {0xF3, 0x45, 0x0F, 0x6F, 0x3C, 0x24};
	uint8_t mov_xmm15_r13[] = {0xF3, 0x45, 0x0F, 0x6F, 0x7D, 0x00};
	uint8_t mov_xmm15_r14[] = {0xF3, 0x45, 0x0F, 0x6F, 0x3E};
	uint8_t mov_xmm15_r15[] = {0xF3, 0x45, 0x0F, 0x6F, 0x3F};

	uint8_t mc[MC_BUFFER_SIZE];
	size_t n;

	/* XMM0 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm0_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm0_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm0_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm0_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm0_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm0_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm0_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm0_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM0, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm0_r15, n);

	/* XMM1 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm1_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm1_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm1_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm1_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm1_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm1_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm1_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm1_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM1, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm1_r15, n);

	/* XMM2 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm2_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm2_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm2_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm2_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm2_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm2_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm2_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm2_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM2, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm2_r15, n);

	/* XMM3 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm3_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm3_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm3_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm3_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm3_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm3_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm3_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm3_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM3, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm3_r15, n);

	/* XMM4 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm4_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm4_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm4_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm4_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm4_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm4_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm4_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm4_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM4, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm4_r15, n);

	/* XMM5 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm5_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm5_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm5_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm5_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm5_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm5_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm5_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm5_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM5, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm5_r15, n);

	/* XMM6 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm6_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm6_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm6_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm6_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm6_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm6_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm6_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm6_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM6, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm6_r15, n);

	/* XMM7 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_EAX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm7_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_ECX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm7_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_EDX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm7_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_EBX);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm7_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_ESP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_EBP);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_ESI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm7_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_EDI);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_xmm7_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm7_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm7_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM7, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm7_r15, n);

	/* XMM8 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm8_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm8_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm8_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm8_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM8, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm8_r15, n);

	/* XMM9 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm9_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm9_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm9_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm9_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM9, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm9_r15, n);

	/* XMM10 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm10_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm10_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm10_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm10_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM10, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm10_r15, n);

	/* XMM11 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm11_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm11_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm11_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm11_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM11, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm11_r15, n);

	/* XMM12 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm12_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm12_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm12_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm12_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM12, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm12_r15, n);

	/* XMM13 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm13_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm13_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm13_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm13_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM13, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm13_r15, n);

	/* XMM14 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm14_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm14_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm14_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm14_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM14, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm14_r15, n);

	/* XMM15 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_EAX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_rax, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_ECX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_rcx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_EDX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_rdx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_EBX);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_rbx, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_ESP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm15_rsp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_EBP);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm15_rbp, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_ESI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_rsi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_EDI);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_rdi, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R8D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_r8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R9D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_r9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R10D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_r10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R11D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_r11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R12D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm15_r12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R13D);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_xmm15_r13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R14D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_r14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movxmmrm(mc, RID_XMM15, RID_R15D);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_xmm15_r15, n);
}

/* movdqu [gpr], xmm  */
static void test_movrmxmm(void **state)
{
	UNUSED_STATE(state);

	uint8_t mov_rax_xmm0[] = {0xF3, 0x0F, 0x7F, 0x00};
	uint8_t mov_rcx_xmm0[] = {0xF3, 0x0F, 0x7F, 0x01};
	uint8_t mov_rdx_xmm0[] = {0xF3, 0x0F, 0x7F, 0x02};
	uint8_t mov_rbx_xmm0[] = {0xF3, 0x0F, 0x7F, 0x03};
	uint8_t mov_rsp_xmm0[] = {0xF3, 0x0F, 0x7F, 0x04, 0x24};
	uint8_t mov_rbp_xmm0[] = {0xF3, 0x0F, 0x7F, 0x45, 0x00};
	uint8_t mov_rsi_xmm0[] = {0xF3, 0x0F, 0x7F, 0x06};
	uint8_t mov_rdi_xmm0[] = {0xF3, 0x0F, 0x7F, 0x07};
	uint8_t mov_r8_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x00};
	uint8_t mov_r9_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x01};
	uint8_t mov_r10_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x02};
	uint8_t mov_r11_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x03};
	uint8_t mov_r12_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x04, 0x24};
	uint8_t mov_r13_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x45, 0x00};
	uint8_t mov_r14_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x06};
	uint8_t mov_r15_xmm0[] = {0xF3, 0x41, 0x0F, 0x7F, 0x07};

	uint8_t mov_rax_xmm1[] = {0xF3, 0x0F, 0x7F, 0x08};
	uint8_t mov_rcx_xmm1[] = {0xF3, 0x0F, 0x7F, 0x09};
	uint8_t mov_rdx_xmm1[] = {0xF3, 0x0F, 0x7F, 0x0A};
	uint8_t mov_rbx_xmm1[] = {0xF3, 0x0F, 0x7F, 0x0B};
	uint8_t mov_rsp_xmm1[] = {0xF3, 0x0F, 0x7F, 0x0C, 0x24};
	uint8_t mov_rbp_xmm1[] = {0xF3, 0x0F, 0x7F, 0x4D, 0x00};
	uint8_t mov_rsi_xmm1[] = {0xF3, 0x0F, 0x7F, 0x0E};
	uint8_t mov_rdi_xmm1[] = {0xF3, 0x0F, 0x7F, 0x0F};
	uint8_t mov_r8_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x08};
	uint8_t mov_r9_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x09};
	uint8_t mov_r10_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x0A};
	uint8_t mov_r11_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x0B};
	uint8_t mov_r12_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x0C, 0x24};
	uint8_t mov_r13_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x4D, 0x00};
	uint8_t mov_r14_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x0E};
	uint8_t mov_r15_xmm1[] = {0xF3, 0x41, 0x0F, 0x7F, 0x0F};

	uint8_t mov_rax_xmm2[] = {0xF3, 0x0F, 0x7F, 0x10};
	uint8_t mov_rcx_xmm2[] = {0xF3, 0x0F, 0x7F, 0x11};
	uint8_t mov_rdx_xmm2[] = {0xF3, 0x0F, 0x7F, 0x12};
	uint8_t mov_rbx_xmm2[] = {0xF3, 0x0F, 0x7F, 0x13};
	uint8_t mov_rsp_xmm2[] = {0xF3, 0x0F, 0x7F, 0x14, 0x24};
	uint8_t mov_rbp_xmm2[] = {0xF3, 0x0F, 0x7F, 0x55, 0x00};
	uint8_t mov_rsi_xmm2[] = {0xF3, 0x0F, 0x7F, 0x16};
	uint8_t mov_rdi_xmm2[] = {0xF3, 0x0F, 0x7F, 0x17};
	uint8_t mov_r8_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x10};
	uint8_t mov_r9_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x11};
	uint8_t mov_r10_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x12};
	uint8_t mov_r11_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x13};
	uint8_t mov_r12_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x14, 0x24};
	uint8_t mov_r13_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x55, 0x00};
	uint8_t mov_r14_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x16};
	uint8_t mov_r15_xmm2[] = {0xF3, 0x41, 0x0F, 0x7F, 0x17};

	uint8_t mov_rax_xmm3[] = {0xF3, 0x0F, 0x7F, 0x18};
	uint8_t mov_rcx_xmm3[] = {0xF3, 0x0F, 0x7F, 0x19};
	uint8_t mov_rdx_xmm3[] = {0xF3, 0x0F, 0x7F, 0x1A};
	uint8_t mov_rbx_xmm3[] = {0xF3, 0x0F, 0x7F, 0x1B};
	uint8_t mov_rsp_xmm3[] = {0xF3, 0x0F, 0x7F, 0x1C, 0x24};
	uint8_t mov_rbp_xmm3[] = {0xF3, 0x0F, 0x7F, 0x5D, 0x00};
	uint8_t mov_rsi_xmm3[] = {0xF3, 0x0F, 0x7F, 0x1E};
	uint8_t mov_rdi_xmm3[] = {0xF3, 0x0F, 0x7F, 0x1F};
	uint8_t mov_r8_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x18};
	uint8_t mov_r9_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x19};
	uint8_t mov_r10_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x1A};
	uint8_t mov_r11_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x1B};
	uint8_t mov_r12_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x1C, 0x24};
	uint8_t mov_r13_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x5D, 0x00};
	uint8_t mov_r14_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x1E};
	uint8_t mov_r15_xmm3[] = {0xF3, 0x41, 0x0F, 0x7F, 0x1F};

	uint8_t mov_rax_xmm4[] = {0xF3, 0x0F, 0x7F, 0x20};
	uint8_t mov_rcx_xmm4[] = {0xF3, 0x0F, 0x7F, 0x21};
	uint8_t mov_rdx_xmm4[] = {0xF3, 0x0F, 0x7F, 0x22};
	uint8_t mov_rbx_xmm4[] = {0xF3, 0x0F, 0x7F, 0x23};
	uint8_t mov_rsp_xmm4[] = {0xF3, 0x0F, 0x7F, 0x24, 0x24};
	uint8_t mov_rbp_xmm4[] = {0xF3, 0x0F, 0x7F, 0x65, 0x00};
	uint8_t mov_rsi_xmm4[] = {0xF3, 0x0F, 0x7F, 0x26};
	uint8_t mov_rdi_xmm4[] = {0xF3, 0x0F, 0x7F, 0x27};
	uint8_t mov_r8_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x20};
	uint8_t mov_r9_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x21};
	uint8_t mov_r10_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x22};
	uint8_t mov_r11_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x23};
	uint8_t mov_r12_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x24, 0x24};
	uint8_t mov_r13_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x65, 0x00};
	uint8_t mov_r14_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x26};
	uint8_t mov_r15_xmm4[] = {0xF3, 0x41, 0x0F, 0x7F, 0x27};

	uint8_t mov_rax_xmm5[] = {0xF3, 0x0F, 0x7F, 0x28};
	uint8_t mov_rcx_xmm5[] = {0xF3, 0x0F, 0x7F, 0x29};
	uint8_t mov_rdx_xmm5[] = {0xF3, 0x0F, 0x7F, 0x2A};
	uint8_t mov_rbx_xmm5[] = {0xF3, 0x0F, 0x7F, 0x2B};
	uint8_t mov_rsp_xmm5[] = {0xF3, 0x0F, 0x7F, 0x2C, 0x24};
	uint8_t mov_rbp_xmm5[] = {0xF3, 0x0F, 0x7F, 0x6D, 0x00};
	uint8_t mov_rsi_xmm5[] = {0xF3, 0x0F, 0x7F, 0x2E};
	uint8_t mov_rdi_xmm5[] = {0xF3, 0x0F, 0x7F, 0x2F};
	uint8_t mov_r8_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x28};
	uint8_t mov_r9_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x29};
	uint8_t mov_r10_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x2A};
	uint8_t mov_r11_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x2B};
	uint8_t mov_r12_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x2C, 0x24};
	uint8_t mov_r13_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x6D, 0x00};
	uint8_t mov_r14_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x2E};
	uint8_t mov_r15_xmm5[] = {0xF3, 0x41, 0x0F, 0x7F, 0x2F};

	uint8_t mov_rax_xmm6[] = {0xF3, 0x0F, 0x7F, 0x30};
	uint8_t mov_rcx_xmm6[] = {0xF3, 0x0F, 0x7F, 0x31};
	uint8_t mov_rdx_xmm6[] = {0xF3, 0x0F, 0x7F, 0x32};
	uint8_t mov_rbx_xmm6[] = {0xF3, 0x0F, 0x7F, 0x33};
	uint8_t mov_rsp_xmm6[] = {0xF3, 0x0F, 0x7F, 0x34, 0x24};
	uint8_t mov_rbp_xmm6[] = {0xF3, 0x0F, 0x7F, 0x75, 0x00};
	uint8_t mov_rsi_xmm6[] = {0xF3, 0x0F, 0x7F, 0x36};
	uint8_t mov_rdi_xmm6[] = {0xF3, 0x0F, 0x7F, 0x37};
	uint8_t mov_r8_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x30};
	uint8_t mov_r9_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x31};
	uint8_t mov_r10_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x32};
	uint8_t mov_r11_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x33};
	uint8_t mov_r12_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x34, 0x24};
	uint8_t mov_r13_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x75, 0x00};
	uint8_t mov_r14_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x36};
	uint8_t mov_r15_xmm6[] = {0xF3, 0x41, 0x0F, 0x7F, 0x37};

	uint8_t mov_rax_xmm7[] = {0xF3, 0x0F, 0x7F, 0x38};
	uint8_t mov_rcx_xmm7[] = {0xF3, 0x0F, 0x7F, 0x39};
	uint8_t mov_rdx_xmm7[] = {0xF3, 0x0F, 0x7F, 0x3A};
	uint8_t mov_rbx_xmm7[] = {0xF3, 0x0F, 0x7F, 0x3B};
	uint8_t mov_rsp_xmm7[] = {0xF3, 0x0F, 0x7F, 0x3C, 0x24};
	uint8_t mov_rbp_xmm7[] = {0xF3, 0x0F, 0x7F, 0x7D, 0x00};
	uint8_t mov_rsi_xmm7[] = {0xF3, 0x0F, 0x7F, 0x3E};
	uint8_t mov_rdi_xmm7[] = {0xF3, 0x0F, 0x7F, 0x3F};
	uint8_t mov_r8_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x38};
	uint8_t mov_r9_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x39};
	uint8_t mov_r10_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x3A};
	uint8_t mov_r11_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x3B};
	uint8_t mov_r12_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x3C, 0x24};
	uint8_t mov_r13_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x7D, 0x00};
	uint8_t mov_r14_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x3E};
	uint8_t mov_r15_xmm7[] = {0xF3, 0x41, 0x0F, 0x7F, 0x3F};

	uint8_t mov_rax_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x00};
	uint8_t mov_rcx_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x01};
	uint8_t mov_rdx_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x02};
	uint8_t mov_rbx_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x03};
	uint8_t mov_rsp_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x04, 0x24};
	uint8_t mov_rbp_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x45, 0x00};
	uint8_t mov_rsi_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x06};
	uint8_t mov_rdi_xmm8[] = {0xF3, 0x44, 0x0F, 0x7F, 0x07};
	uint8_t mov_r8_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x00};
	uint8_t mov_r9_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x01};
	uint8_t mov_r10_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x02};
	uint8_t mov_r11_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x03};
	uint8_t mov_r12_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x04, 0x24};
	uint8_t mov_r13_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x45, 0x00};
	uint8_t mov_r14_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x06};
	uint8_t mov_r15_xmm8[] = {0xF3, 0x45, 0x0F, 0x7F, 0x07};

	uint8_t mov_rax_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x08};
	uint8_t mov_rcx_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x09};
	uint8_t mov_rdx_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x0A};
	uint8_t mov_rbx_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x0B};
	uint8_t mov_rsp_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x0C, 0x24};
	uint8_t mov_rbp_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x4D, 0x00};
	uint8_t mov_rsi_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x0E};
	uint8_t mov_rdi_xmm9[] = {0xF3, 0x44, 0x0F, 0x7F, 0x0F};
	uint8_t mov_r8_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x08};
	uint8_t mov_r9_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x09};
	uint8_t mov_r10_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x0A};
	uint8_t mov_r11_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x0B};
	uint8_t mov_r12_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x0C, 0x24};
	uint8_t mov_r13_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x4D, 0x00};
	uint8_t mov_r14_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x0E};
	uint8_t mov_r15_xmm9[] = {0xF3, 0x45, 0x0F, 0x7F, 0x0F};

	uint8_t mov_rax_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x10};
	uint8_t mov_rcx_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x11};
	uint8_t mov_rdx_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x12};
	uint8_t mov_rbx_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x13};
	uint8_t mov_rsp_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x14, 0x24};
	uint8_t mov_rbp_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x55, 0x00};
	uint8_t mov_rsi_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x16};
	uint8_t mov_rdi_xmm10[] = {0xF3, 0x44, 0x0F, 0x7F, 0x17};
	uint8_t mov_r8_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x10};
	uint8_t mov_r9_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x11};
	uint8_t mov_r10_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x12};
	uint8_t mov_r11_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x13};
	uint8_t mov_r12_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x14, 0x24};
	uint8_t mov_r13_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x55, 0x00};
	uint8_t mov_r14_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x16};
	uint8_t mov_r15_xmm10[] = {0xF3, 0x45, 0x0F, 0x7F, 0x17};

	uint8_t mov_rax_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x18};
	uint8_t mov_rcx_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x19};
	uint8_t mov_rdx_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x1A};
	uint8_t mov_rbx_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x1B};
	uint8_t mov_rsp_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x1C, 0x24};
	uint8_t mov_rbp_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x5D, 0x00};
	uint8_t mov_rsi_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x1E};
	uint8_t mov_rdi_xmm11[] = {0xF3, 0x44, 0x0F, 0x7F, 0x1F};
	uint8_t mov_r8_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x18};
	uint8_t mov_r9_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x19};
	uint8_t mov_r10_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x1A};
	uint8_t mov_r11_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x1B};
	uint8_t mov_r12_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x1C, 0x24};
	uint8_t mov_r13_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x5D, 0x00};
	uint8_t mov_r14_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x1E};
	uint8_t mov_r15_xmm11[] = {0xF3, 0x45, 0x0F, 0x7F, 0x1F};

	uint8_t mov_rax_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x20};
	uint8_t mov_rcx_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x21};
	uint8_t mov_rdx_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x22};
	uint8_t mov_rbx_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x23};
	uint8_t mov_rsp_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x24, 0x24};
	uint8_t mov_rbp_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x65, 0x00};
	uint8_t mov_rsi_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x26};
	uint8_t mov_rdi_xmm12[] = {0xF3, 0x44, 0x0F, 0x7F, 0x27};
	uint8_t mov_r8_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x20};
	uint8_t mov_r9_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x21};
	uint8_t mov_r10_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x22};
	uint8_t mov_r11_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x23};
	uint8_t mov_r12_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x24, 0x24};
	uint8_t mov_r13_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x65, 0x00};
	uint8_t mov_r14_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x26};
	uint8_t mov_r15_xmm12[] = {0xF3, 0x45, 0x0F, 0x7F, 0x27};

	uint8_t mov_rax_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x28};
	uint8_t mov_rcx_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x29};
	uint8_t mov_rdx_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x2A};
	uint8_t mov_rbx_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x2B};
	uint8_t mov_rsp_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x2C, 0x24};
	uint8_t mov_rbp_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x6D, 0x00};
	uint8_t mov_rsi_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x2E};
	uint8_t mov_rdi_xmm13[] = {0xF3, 0x44, 0x0F, 0x7F, 0x2F};
	uint8_t mov_r8_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x28};
	uint8_t mov_r9_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x29};
	uint8_t mov_r10_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x2A};
	uint8_t mov_r11_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x2B};
	uint8_t mov_r12_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x2C, 0x24};
	uint8_t mov_r13_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x6D, 0x00};
	uint8_t mov_r14_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x2E};
	uint8_t mov_r15_xmm13[] = {0xF3, 0x45, 0x0F, 0x7F, 0x2F};

	uint8_t mov_rax_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x30};
	uint8_t mov_rcx_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x31};
	uint8_t mov_rdx_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x32};
	uint8_t mov_rbx_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x33};
	uint8_t mov_rsp_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x34, 0x24};
	uint8_t mov_rbp_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x75, 0x00};
	uint8_t mov_rsi_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x36};
	uint8_t mov_rdi_xmm14[] = {0xF3, 0x44, 0x0F, 0x7F, 0x37};
	uint8_t mov_r8_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x30};
	uint8_t mov_r9_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x31};
	uint8_t mov_r10_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x32};
	uint8_t mov_r11_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x33};
	uint8_t mov_r12_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x34, 0x24};
	uint8_t mov_r13_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x75, 0x00};
	uint8_t mov_r14_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x36};
	uint8_t mov_r15_xmm14[] = {0xF3, 0x45, 0x0F, 0x7F, 0x37};

	uint8_t mov_rax_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x38};
	uint8_t mov_rcx_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x39};
	uint8_t mov_rdx_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x3A};
	uint8_t mov_rbx_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x3B};
	uint8_t mov_rsp_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x3C, 0x24};
	uint8_t mov_rbp_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x7D, 0x00};
	uint8_t mov_rsi_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x3E};
	uint8_t mov_rdi_xmm15[] = {0xF3, 0x44, 0x0F, 0x7F, 0x3F};
	uint8_t mov_r8_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x38};
	uint8_t mov_r9_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x39};
	uint8_t mov_r10_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x3A};
	uint8_t mov_r11_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x3B};
	uint8_t mov_r12_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x3C, 0x24};
	uint8_t mov_r13_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x7D, 0x00};
	uint8_t mov_r14_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x3E};
	uint8_t mov_r15_xmm15[] = {0xF3, 0x45, 0x0F, 0x7F, 0x3F};

	uint8_t mc[MC_BUFFER_SIZE];
	size_t n;

	/* XMM0 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM0);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM0);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM0);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM0);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM0);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM0);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM0);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM0);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm0, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM0);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm0, n);

	/* XMM1 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM1);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM1);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM1);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM1);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM1);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM1);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM1);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM1);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm1, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM1);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm1, n);

	/* XMM2 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM2);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM2);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM2);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM2);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM2);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM2);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM2);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM2);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm2, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM2);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm2, n);

	/* XMM3 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM3);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM3);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM3);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM3);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM3);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM3);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM3);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM3);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm3, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM3);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm3, n);

	/* XMM4 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM4);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM4);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM4);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM4);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM4);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM4);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM4);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM4);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm4, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM4);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm4, n);

	/* XMM5 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM5);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM5);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM5);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM5);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM5);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM5);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM5);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM5);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm5, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM5);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm5, n);

	/* XMM6 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM6);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM6);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM6);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM6);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM6);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM6);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM6);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM6);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm6, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM6);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm6, n);

	/* XMM7 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM7);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rax_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM7);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rcx_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM7);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdx_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM7);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rbx_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsp_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbp_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM7);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rsi_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM7);
	assert_true(n == 4);
	assert_memory_equal(mc, mov_rdi_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM7);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM7);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm7, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM7);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm7, n);

	/* XMM8 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM8);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM8);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM8);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM8);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm8, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM8);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm8, n);

	/* XMM9 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM9);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM9);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM9);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM9);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm9, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM9);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm9, n);

	/* XMM10 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM10);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM10);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM10);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM10);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm10, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM10);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm10, n);

	/* XMM11 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM11);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM11);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM11);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM11);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm11, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM11);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm11, n);

	/* XMM12 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM12);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM12);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM12);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM12);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm12, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM12);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm12, n);

	/* XMM13 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM13);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM13);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM13);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM13);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm13, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM13);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm13, n);

	/* XMM14 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM14);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM14);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM14);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM14);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm14, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM14);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm14, n);

	/* XMM15 */

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EAX, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rax_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ECX, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rcx_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDX, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdx_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBX, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rbx_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESP, RID_XMM15);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rsp_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EBP, RID_XMM15);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_rbp_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_ESI, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rsi_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_EDI, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_rdi_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R8D, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r8_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R9D, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r9_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R10D, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r10_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R11D, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r11_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R12D, RID_XMM15);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r12_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R13D, RID_XMM15);
	assert_true(n == 6);
	assert_memory_equal(mc, mov_r13_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R14D, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r14_xmm15, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_movrmxmm(mc, RID_R15D, RID_XMM15);
	assert_true(n == 5);
	assert_memory_equal(mc, mov_r15_xmm15, n);
}

/* movdqu xmm, XMMWORD PTR [rsp +/- ofs] */
static void test_spload(void **state)
{
	UNUSED_STATE(state);

	uint8_t load_xmm0_p08[] = {0xF3, 0x0F, 0x6F, 0x44, 0x24, 0x08};
	uint8_t load_xmm0_n08[] = {0xF3, 0x0F, 0x6F, 0x44, 0x24, 0xF8};
	uint8_t load_xmm8_p08[] = {0xF3, 0x44, 0x0F, 0x6F, 0x44, 0x24, 0x08};
	uint8_t load_xmm8_n08[] = {0xF3, 0x44, 0x0F, 0x6F, 0x44, 0x24, 0xF8};

	uint8_t load_xmm0_p88[] = {0xF3, 0x0F, 0x6F, 0x84, 0x24,
				   0x88, 0x00, 0x00, 0x00};
	uint8_t load_xmm0_n88[] = {0xF3, 0x0F, 0x6F, 0x84, 0x24,
				   0x78, 0xFF, 0xFF, 0xFF};
	uint8_t load_xmm8_p88[] = {0xF3, 0x44, 0x0F, 0x6F, 0x84,
				   0x24, 0x88, 0x00, 0x00, 0x00};
	uint8_t load_xmm8_n88[] = {0xF3, 0x44, 0x0F, 0x6F, 0x84,
				   0x24, 0x78, 0xFF, 0xFF, 0xFF};

	uint8_t mc[MC_BUFFER_SIZE];
	size_t n;

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM0, 0x08);
	assert_true(n == 6);
	assert_memory_equal(mc, load_xmm0_p08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM0, -0x08);
	assert_true(n == 6);
	assert_memory_equal(mc, load_xmm0_n08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM8, 0x08);
	assert_true(n == 7);
	assert_memory_equal(mc, load_xmm8_p08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM8, -0x08);
	assert_true(n == 7);
	assert_memory_equal(mc, load_xmm8_n08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM0, 0x88);
	assert_true(n == 9);
	assert_memory_equal(mc, load_xmm0_p88, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM0, -0x88);
	assert_true(n == 9);
	assert_memory_equal(mc, load_xmm0_n88, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM8, 0x88);
	assert_true(n == 10);
	assert_memory_equal(mc, load_xmm8_p88, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spload_xmm(mc, RID_XMM8, -0x88);
	assert_true(n == 10);
	assert_memory_equal(mc, load_xmm8_n88, n);
}

/* movdqu XMMWORD PTR [rsp +/- ofs], xmm */
static void test_spstore(void **state)
{
	UNUSED_STATE(state);

	uint8_t store_xmm0_p08[] = {0xF3, 0x0F, 0x7F, 0x44, 0x24, 0x08};
	uint8_t store_xmm0_n08[] = {0xF3, 0x0F, 0x7F, 0x44, 0x24, 0xF8};
	uint8_t store_xmm8_p08[] = {0xF3, 0x44, 0x0F, 0x7F, 0x44, 0x24, 0x08};
	uint8_t store_xmm8_n08[] = {0xF3, 0x44, 0x0F, 0x7F, 0x44, 0x24, 0xF8};

	uint8_t store_xmm0_p88[] = {0xF3, 0x0F, 0x7F, 0x84, 0x24,
				    0x88, 0x00, 0x00, 0x00};
	uint8_t store_xmm0_n88[] = {0xF3, 0x0F, 0x7F, 0x84, 0x24,
				    0x78, 0xFF, 0xFF, 0xFF};
	uint8_t store_xmm8_p88[] = {0xF3, 0x44, 0x0F, 0x7F, 0x84,
				    0x24, 0x88, 0x00, 0x00, 0x00};
	uint8_t store_xmm8_n88[] = {0xF3, 0x44, 0x0F, 0x7F, 0x84,
				    0x24, 0x78, 0xFF, 0xFF, 0xFF};

	uint8_t mc[MC_BUFFER_SIZE];
	size_t n;

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM0, 0x08);
	assert_true(n == 6);
	assert_memory_equal(mc, store_xmm0_p08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM0, -0x08);
	assert_true(n == 6);
	assert_memory_equal(mc, store_xmm0_n08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM8, 0x08);
	assert_true(n == 7);
	assert_memory_equal(mc, store_xmm8_p08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM8, -0x08);
	assert_true(n == 7);
	assert_memory_equal(mc, store_xmm8_n08, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM0, 0x88);
	assert_true(n == 9);
	assert_memory_equal(mc, store_xmm0_p88, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM0, -0x88);
	assert_true(n == 9);
	assert_memory_equal(mc, store_xmm0_n88, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM8, 0x88);
	assert_true(n == 10);
	assert_memory_equal(mc, store_xmm8_p88, n);

	memset(mc, 0, MC_BUFFER_SIZE);
	n = uj_emit_spstore_xmm(mc, RID_XMM8, -0x88);
	assert_true(n == 10);
	assert_memory_equal(mc, store_xmm8_n88, n);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_movxmmrm),
		cmocka_unit_test(test_movrmxmm),
		cmocka_unit_test(test_spload),
		cmocka_unit_test(test_spstore),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
