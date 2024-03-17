/*
 * C-level interface for dumping compiler's progress.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_frame.h"
#include "uj_dispatch.h"

#include "dump/uj_dump_iface.h"
#include "dump/uj_dump_datadef.h"
#include "dump/uj_dump_utils.h"

#define FUNC_DESC_BUFFER_SIZE 128

typedef void (*progress_dumper)(FILE *out, const jit_State *J, void *data);

static void dump_progress_trace_record(FILE *out, const jit_State *J,
				       void *data)
{
	UNUSED(data);

	if (J->pt)
		uj_dump_bc_ins(out, J->pt, J->pc, J->framedepth);
	else
		uj_dump_nonlua_bc_ins(out, J->fn, J->framedepth);
}

static void dump_progress_trace_start(FILE *out, const jit_State *J, void *data)
{
	UNUSED(data);

	fprintf(out, "---- TRACE %d %s", J->cur.traceno,
		dump_progress_state_names[JSTATE_TRACE_START]);

	if (J->parent)
		fprintf(out, " %d/%d", J->parent, J->exitno);

	fprintf(out, " ");
	uj_dump_func_description(out, J->fn, proto_bcpos(J->pt, J->pc));
	fprintf(out, "\n");
}

/*
 * NB! By the time the STOP event is reported, trace is already saved inside J,
 * and J->cur cannot be used for dumping purposes. So we pass the new trace as
 * an opaque data payload.
 */
static void dump_progress_trace_stop(FILE *out, const jit_State *J, void *data)
{
	const GCtrace *trace = (const GCtrace *)data;
	TraceNo trace_no = trace->traceno;
	TraceNo link = trace->link;
	uint8_t link_type = trace->linktype;

	uj_dump_ir(out, trace);
	uj_dump_mcode(out, J, trace);

	fprintf(out, "---- TRACE %d %s -> ", trace_no,
		dump_progress_state_names[JSTATE_TRACE_STOP]);

	if (link == trace_no || link == 0)
		fprintf(out, "%s", dump_trace_lt_names[link_type]);
	else if (link_type == LJ_TRLINK_ROOT)
		fprintf(out, "%d", link);
	else
		fprintf(out, "%d %s", link, dump_trace_lt_names[link_type]);
	fprintf(out, "\n\n");
}

/* NB! J->cur is not purged yet, it is fully legit to dump from this value. */
static void dump_progress_trace_abort(FILE *out, const jit_State *J, void *data)
{
	const AbortState *abortstate = (const AbortState *)&(J->abortstate);
	const GCtrace *trace = &(J->cur);
	const TValue *frame;
	const BCIns *pc;
	const GCfunc *fn;
	TraceError error_code;
	const TValue *extra_data;

	uj_dump_ir(out, trace);
	uj_dump_mcode(out, J, trace);

	fprintf(out, "---- TRACE %d %s ", trace->traceno,
		dump_progress_state_names[JSTATE_TRACE_ABORT]);

	/*
	 * Take the state that triggered abort, find first Lua frame in it
	 * (there surely will be one) and compose a function location hint.
	 */
	frame = abortstate->L->base - 1;
	pc = abortstate->pc;

	while (!isluafunc(frame_func(frame))) {
		pc = frame_iscont(frame) ? frame_contpc(frame)
					 : frame_pc(frame);
		pc--;
		frame = frame_prev(frame);
	}
	fn = frame_func(frame);

	uj_dump_func_description(out, fn, proto_bcpos(funcproto(fn), pc));
	fprintf(out, " -- ");

	error_code = *(TraceError *)data;
	extra_data = (const TValue *)&(abortstate->extra_data);

	if (!tagisvalid(extra_data)) {
		fprintf(out, "asynchronous abort\n\n");
		return;
	}

	if (tvisnil(extra_data)) {
		fprintf(out, "%s", dump_trace_errors[error_code]);
	} else if (tvisnum(extra_data)) {
		fprintf(out, dump_trace_errors[error_code],
			(int)numV(extra_data));
	} else if (tvisfunc(extra_data)) {
		/*
		 * NB! Here we have C functions and various built-ins with
		 * rather short names, so even medium-size buffer
		 * should suffice.
		 */
		char desc_buffer[FUNC_DESC_BUFFER_SIZE] = {0};

		uj_dump_format_func_description(desc_buffer, funcV(extra_data),
						0);
		fprintf(out, dump_trace_errors[error_code], desc_buffer);
	} else {
		/*
		 * Other TValue's cannot be used as
		 * error extra info at the moment.
		 */
		lua_assert(0);
	}
	fprintf(out, "\n\n");
}

static void dump_progress_trace_flush(FILE *out, const jit_State *J, void *data)
{
	UNUSED(J);
	UNUSED(data);

	fprintf(out, "---- TRACE flush\n\n");
}

/* Dump all registers from exit state es to out. */
static void dump_regs_x86(FILE *out, const ExitState *es)
{
	int32_t i;

	fprintf(out, "General-purpose registers\n");
	for (i = 0; i < RID_NUM_GPR; i++)
		fprintf(out, "%-3s = %#018llx\n", dump_reg_gpr_names[i],
			(unsigned long long)es->gpr[i]);

	fprintf(out, "Floating-point registers\n");
	for (i = 0; i < RID_NUM_FPR; i++)
		fprintf(out, "%-5s = %+17.14g\n", dump_reg_fpr_names[i],
			(double)es->fpr[i]);
}

static void dump_progress_trace_exit(FILE *out, const jit_State *J, void *data)
{
	fprintf(out, "---- TRACE %d exit %d\n", J->parent, J->exitno);
	dump_regs_x86(out, (ExitState *)data);
	fprintf(out, "\n");
}

static const progress_dumper progress_dumpers[] = {
#define DUMP_PROGRESS_DEF(state, suffix) \
	((progress_dumper)dump_progress_trace_##suffix),
#include "dump/uj_dump_progress.h"
#undef DUMP_PROGRESS_DEF
};

void uj_dump_compiler_progress(int jstate, const jit_State *J, void *data)
{
	FILE *out = (FILE *)J->dump_file;

	if (out == NULL)
		return;

	(progress_dumpers[jstate])(out, J, data);

	fflush(out);
}

int uj_dump_start(const lua_State *L, FILE *out)
{
	jit_State *J = L2J(L);

	if (J->dump_file != NULL)
		return 1; /* Error: Already dumping */

	if (out == NULL)
		return 1; /* Error: Invalid output */

	J->dump_file = out;
	return 0;
}

int uj_dump_stop(const lua_State *L)
{
	jit_State *J = L2J(L);
	FILE *out = J->dump_file;

	if (out == NULL)
		return 1; /* Error: Not dumping at the moment */

	if (out != stdout && out != stderr)
		fclose(out);

	J->dump_file = NULL;
	return 0;
}
