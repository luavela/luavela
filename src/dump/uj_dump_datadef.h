/*
 * Data definitions for C-level BC / IR / mcode dumpers.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_DUMP_DATADEF_H
#define _UJ_DUMP_DATADEF_H

/* Byte code instruction mnemonics. */
LJ_DATA const char *const dump_bc_names[];
/* IR instruction mnemonics. */
LJ_DATA const char *const dump_ir_names[];
/* Fast function names. */
LJ_DATA const char *const dump_ff_names[];

/* IR type mnemonics. */
LJ_DATA const char *const dump_ir_types[];
/* Function names for CALL* instructions. */
LJ_DATA const char *const dump_ir_call_names[];
/* Field names for FLOAD. */
LJ_DATA const char *const dump_ir_field_names[];
/* Function names for floating-point math calls. */
LJ_DATA const char *const dump_ir_fpm_names[];

/* Tracing error descriptions. */
LJ_DATA const char *const dump_trace_errors[];
/* Trace link type names. */
LJ_DATA const char *const dump_trace_lt_names[];

/* Platform-specific: General-purpose registers. */
LJ_DATA const char *const dump_reg_gpr_names[];
/* Platform-specific: Floating-point registers. */
LJ_DATA const char *const dump_reg_fpr_names[];
LJ_DATA const char *const dump_progress_state_names[];

#endif /* !_UJ_DUMP_DATADEF_H */
