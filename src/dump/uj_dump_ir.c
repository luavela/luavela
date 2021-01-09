/*
 * C-level trace IR dumper. Dumps IR itself as well as interleaved snapshots.
 * Port of LuaJIT's original IR dumper dump.lua with some modifications
 * and reasonable simplifications (marked with NB! throughout the code).
 * Copyright (C) 2020-2021 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "jit/lj_ircall.h"

#include "uj_str.h"
#include "uj_proto.h"

#include "utils/str.h"

#include "dump/uj_dump_datadef.h"
#include "dump/uj_dump_utils.h"

/*
 * NB! The original dump_ir from dump.lua is parametrized with two flags,
 * dumpsnap (dump IR's intervleaved snapshots) and dumpreg (print host register
 * names). We have them as compile-time flags to avoid polluting interfaces.
 */
#define DUMP_INTERLEAVED_SNAPSHOTS 1
#define DUMP_ALLOCATED_HOST_REGISTERS 1
/* max 40 chars + \0 terminator */
#define IR_KVALUE_BUF_SIZE 41
/* worst case: "<20 chars>\127"~ (27 chars) */
#define IR_KVALUE_FMT_KSTR_THRESHOLD 21

/* Unbias a value that is guaranteed to be an IRMref. */
static uint32_t dump_unbias_ref(IRRef ref)
{
	return (uint32_t)(ref - REF_BIAS);
}

/* Unbias instruction's op1 of it is a IRMref or do nothing otherwise. */
static LJ_AINLINE uint32_t dump_unbias_op1(const IRIns *ins)
{
	IRMode mode = irm_op1(lj_ir_mode[ins->o]);

	return (uint32_t)(ins->op1) - (mode == IRMref ? REF_BIAS : 0);
}

/* Unbias instruction's op2 of it is a IRMref or do nothing otherwise. */
static LJ_AINLINE uint32_t dump_unbias_op2(const IRIns *ins)
{
	IRMode mode = irm_op2(lj_ir_mode[ins->o]);

	return (uint32_t)(ins->op2) - (mode == IRMref ? REF_BIAS : 0);
}

/* Dump IR constant which can be mapped to interpreter-level TValue's. */
static void dump_ir_k_native_lua(FILE *out, const IRIns *ins)
{
	const GCobj *ko = ins->o == IR_KGC ? ir_kgc(ins) : NULL;
	char kvalue[IR_KVALUE_BUF_SIZE] = {0};

	switch (irt_type(ins->t)) {
	case IRT_NIL: {
		fprintf(out, "nil");
		break;
	}
	case IRT_FALSE: {
		fprintf(out, "false");
		break;
	}
	case IRT_TRUE: {
		fprintf(out, "true");
		break;
	}
	case IRT_LIGHTUD: {
		fprintf(out, "ERROR: lightud is not expected here\n");
		lua_assert(0);
		break;
	}
	case IRT_STR: {
		uj_dump_format_kstr(kvalue, strdata(ir_kstr(ins)),
				    IR_KVALUE_FMT_KSTR_THRESHOLD);
		break;
	}
	case IRT_P32: { /* NB! corresponding tag: LJ_TUPVAL */
		fprintf(out, "upvalue: %p", (void *)ko);
		break;
	}
	case IRT_THREAD: {
		/*
		 * NB! Accroding to the original dumper's logic,
		 * equivalent of tostring(...) should be dumped.
		 */
		fprintf(out, "thread: %p", (void *)ko);
		break;
	}
	case IRT_PROTO: {
		uj_proto_fprintloc(out, ir_kproto(ins), 0);
		break;
	}
	case IRT_FUNC: {
		uj_dump_func_description(out, ir_kfunc(ins), 0);
		break;
	}
	case IRT_P64: { /* NB! corresponding tag: LJ_TTRACE */
		fprintf(out, "trace: %p", (void *)ko);
		break;
	}
	case IRT_CDATA: {
		fprintf(out, "cdata: %p", (void *)ko);
		break;
	}
	case IRT_TAB: {
		fprintf(out, "{%p}", (void *)ko);
		break;
	}
	case IRT_UDATA: {
		fprintf(out, "userdata: %p", (void *)ko);
		break;
	}
	case IRT_NUM: {
		double value = numV(ir_knum(ins));

		uj_cstr_fromnum(kvalue, value);
		break;
	}
	default: {
		fprintf(out, "ERROR: Uknown IR operand type\n");
		lua_assert(0);
	}
	}

	if (kvalue[0])
		fprintf(out, "%s", kvalue);
}

/* Dump any IR constant. */
static void dump_ir_k(FILE *out, const GCtrace *trace, IRRef ref)
{
	const IRIns *ir = &trace->ir[ref];
	IRRef1 slot = REF_DROP;

	lua_assert(irref_isk(ref));
	lua_assert(ref >= trace->nk);

	if (ir->o == IR_KSLOT) {
		slot = ir->op2;
		ir = &trace->ir[ir->op1];
	}

	switch (ir->o) {
	case IR_KPRI:
	case IR_KGC:
	case IR_KNUM: {
		dump_ir_k_native_lua(out, ir);
		break;
	}
	case IR_KPTR:
	case IR_KKPTR:
	case IR_KNULL: {
		if (ir->ptr != NULL)
			fprintf(out, "[%p]", ir->ptr);
		else
			fprintf(out, "[NULL]");
		break;
	}
	case IR_KINT: {
		int32_t value = (int32_t)ir->i;

		fprintf(out, "%s%d", value >= 0 ? "+" : "", value);
		break;
	}
#if LJ_HASFFI
	case IR_KINT64: {
		int64_t value = (int64_t)rawV(ir_kint64(ir));

		fprintf(out, "%s%" PRId64, value >= 0 ? "+" : "", value);
		break;
	}
#endif /* LJ_HASFFI */
	default: {
		fprintf(out, "ERROR: Unknown IR K-instruction\n");
		lua_assert(0);
	}
	}

	if (slot != REF_DROP)
		fprintf(out, " @%u", slot);
}

/* Convert IR reference to instruction number. */
static void dump_ir_insno(FILE *out, IRRef ref)
{
	lua_assert(!irref_isk(ref));
	fprintf(out, "%04u", dump_unbias_ref(ref));
}

#if DUMP_ALLOCATED_HOST_REGISTERS

/* Dump register name to out. */
static void dump_reg_name(FILE *out, uint8_t r)
{
	if (r < RID_NUM_GPR) {
		fprintf(out, "%-6s", dump_reg_gpr_names[r]);
		return;
	}

	r -= RID_NUM_GPR;
	if (r < RID_NUM_FPR) {
		fprintf(out, "%-6s", dump_reg_fpr_names[r]);
		return;
	}

	/*
	 * Some crap, probably we dump IR of an aborted trace where proper
	 * register allocation had no chance to happen.
	 */
	fprintf(out, "???   ");
}

#endif /* DUMP_ALLOCATED_HOST_REGISTERS */

/* Dump register name or stack slot for a rid/sp location. */
static void dump_host_alloc_hints(FILE *out, const IRIns *ins, IRRef ref)
{
#if DUMP_ALLOCATED_HOST_REGISTERS
	uint8_t r = ins->r;
	uint8_t s = ins->s;

	if (r == RID_SINK || r == RID_SUNK) {
		if (!ra_hasspill(s) || isfarsunkstore(s))
			fprintf(out, " {sink");
		else
			fprintf(out, " {%04u", dump_unbias_ref(ref) - s);
		return;
	}

	if (ra_hasspill(s)) {
		/* Has a spill stack slot => dump its offset. */
		char stack_offset[6];

		memset(stack_offset, 0, 6);
		sprintf(stack_offset, "[%x]", s << 2);
		fprintf(out, "%-6s", stack_offset);
		return;
	}

	if (!ra_hasreg(r)) {
		fprintf(out, "      ");
		return;
	}

	/* Has allocated register => dump its name. */
	dump_reg_name(out, r);
#else /* DUMP_ALLOCATED_HOST_REGISTERS */
	UNUSED(ins);
	UNUSED(ref);
	fprintf(out, "      ");
#endif /* DUMP_ALLOCATED_HOST_REGISTERS */
}

/* Dump CALL* function ref and return optional ctype reference. */
static IRRef1 dump_ir_func_call(FILE *out, const GCtrace *trace, IRRef ref)
{
	IRRef1 ctype_k = REF_DROP;

	if (!irref_isk(ref)) {
		const IRIns *ins = &trace->ir[ref];

		if (!((int)irt_type(ins->t) & IRT_TYPE)) {
			/* nil type means CARG(func, ctype) */
			ref = ins->op1;
			ctype_k = ins->op2;
		}
	}

	if (irref_isk(ref)) {
		/*
		 * NB! Dump callable object. For a generic GCobj, simply dump
		 * its address, otherwise (exotic unlikely cases of callable
		 * primitives and numbers[?]), dump opcode of the constant.
		 */
		const IRIns *ins = &trace->ir[ref];

		if (LJ_LIKELY(ins->o == IR_KGC))
			fprintf(out, "[%p]", (void *)ir_kgc(ins));
		else
			fprintf(out, "[%s]", dump_ir_names[ins->o]);
	} else {
		fprintf(out, "%04u (", dump_unbias_ref(ref));
	}

	return ctype_k;
}

/* Recursively gather CALL* args and dump them. */
static void dump_ir_func_args(FILE *out, const GCtrace *trace, IRRef ref)
{
	IRRef op2;

	if (irref_isk(ref)) {
		dump_ir_k(out, trace, ref);
		return;
	}
	const IRIns *ins = &trace->ir[ref];

	if (ins->o != IR_CARG) {
		fprintf(out, "%04u", dump_unbias_ref(ref));
		return;
	}

	dump_ir_func_args(out, trace, ins->op1);

	op2 = ins->op2;

	if (irref_isk(op2)) {
		fprintf(out, " ");
		dump_ir_k(out, trace, op2);
	} else {
		fprintf(out, " %04u", dump_unbias_op2(ins));
	}
}

/* Dump flags (op2) for SLOAD IR instruction. */
static void dump_sload_flags(FILE *out, uint16_t flags)
{
	char sload_flags[7] = {0};
	char *write_pos = (char *)sload_flags;

	if (flags & IRSLOAD_PARENT)
		*(write_pos++) = 'P';

	if (flags & IRSLOAD_FRAME)
		*(write_pos++) = 'F';

	if (flags & IRSLOAD_TYPECHECK)
		*(write_pos++) = 'T';

	if (flags & IRSLOAD_CONVERT)
		*(write_pos++) = 'C';

	if (flags & IRSLOAD_READONLY)
		*(write_pos++) = 'R';

	if (flags & IRSLOAD_INHERIT)
		*(write_pos++) = 'I';

	fprintf(out, "%s", sload_flags);
}

/* Dump flags (op2) for XLOAD IR instruction. */
static void dump_xload_flags(FILE *out, uint16_t flags)
{
	char xload_flags[4] = {0};
	char *write_pos = (char *)xload_flags;

	if (flags & IRXLOAD_READONLY)
		*(write_pos++) = 'R';
	if (flags & IRXLOAD_VOLATILE)
		*(write_pos++) = 'V';
	if (flags & IRXLOAD_UNALIGNED)
		*(write_pos++) = 'U';

	fprintf(out, "%s", xload_flags);
}

/* Dump conversion mode (stored in op2) for CONV IR instruction. */
static void dump_conv_mode(FILE *out, uint16_t mode)
{
	uint8_t src_type = (uint8_t)(mode & IRCONV_SRCMASK);
	uint8_t dst_type = (uint8_t)((mode & IRCONV_DSTMASK) >> IRCONV_DSH);

	fprintf(out, "%s.%s", dump_ir_types[dst_type], dump_ir_types[src_type]);

	if (mode & IRCONV_TRUNC)
		fprintf(out, " trunc");
	else if (mode & IRCONV_SEXT)
		fprintf(out, " sext");

	switch (mode & IRCONV_CONVMASK) {
	case IRCONV_INDEX: {
		fprintf(out, " index");
		break;
	}
	case IRCONV_CHECK: {
		fprintf(out, " check");
		break;
	}
	default: {
		break;
	}
	}
}

/* Dump string buffer header mode */
static void dump_bufhdr_mode(FILE *out, uint16_t mode)
{
	if (mode == IRBUFHDR_RESET)
		fprintf(out, "RESET");
	else if (mode == IRBUFHDR_APPEND)
		fprintf(out, "APPEND");
	else
		lua_assert(0); /* Unreachable */
}

static void dump_literal_operand(FILE *out, IROp1 opcode, uint16_t op)
{
	fprintf(out, "  ");

	switch (opcode) {
	case IR_SLOAD: {
		dump_sload_flags(out, op);
		break;
	}
	case IR_XLOAD: {
		dump_xload_flags(out, op);
		break;
	}
	case IR_CONV: {
		dump_conv_mode(out, op);
		break;
	}
	case IR_FLOAD:
	case IR_FREF: {
		char formatted_irfield_name[80];

		to_lower(formatted_irfield_name, dump_ir_field_names[op]);
		replace_underscores(formatted_irfield_name);
		fprintf(out, "%s", formatted_irfield_name);
		break;
	}
	case IR_FPMATH: {
		char formatted_irfpm_name[80];

		to_lower(formatted_irfpm_name, dump_ir_fpm_names[op]);
		fprintf(out, "%s", formatted_irfpm_name);
		break;
	}
	case IR_UREFO:
	case IR_UREFC: {
		fprintf(out, "#%-3d", op >> 8);
		break;
	}
	case IR_BUFHDR:
		dump_bufhdr_mode(out, op);
		break;
	default: {
		fprintf(out, "#%-3d", op);
		break;
	}
	}
}

static void dump_ir_op2(FILE *out, const GCtrace *trace, const IRIns *ins,
			IRRef op2, IRMode mode2, IROp1 opcode)
{
	/* Dump IR instruction operand 2. */
	if (mode2 == IRMlit) {
		dump_literal_operand(out, opcode, (uint16_t)op2);
	} else if (mode2 == IRMref && irref_isk(op2)) {
		fprintf(out, "  ");
		dump_ir_k(out, trace, op2);
	} else {
		fprintf(out, "  %04u", dump_unbias_op2(ins));
	}
}

/* Dump single IR instruction. */
static void dump_ir_ins(FILE *out, const GCtrace *trace, IRRef ref)
{
	const IRIns *ins = &trace->ir[ref];
	IROp1 opcode = ins->o;
	IRType1 type = ins->t;
	IRRef1 op1 = ins->op1;
	IRRef1 op2 = ins->op2;
	uint8_t r = ins->r;
	uint8_t mode = lj_ir_mode[ins->o];
	IRMode mode1 = irm_op1(mode);
	IRMode mode2 = irm_op2(mode);

	if (opcode == IR_LOOP) {
		dump_ir_insno(out, ref);
		fprintf(out, " ------------ LOOP ------------\n");
		return;
	}

	if (opcode == IR_NOP || opcode == IR_CARG)
		return;

#if !DUMP_ALLOCATED_HOST_REGISTERS
	if (opcode == IR_RENAME)
		return;
#endif /* !DUMP_ALLOCATED_HOST_REGISTERS */

	dump_ir_insno(out, ref);
	fprintf(out, " ");

	dump_host_alloc_hints(out, ins, ref);

	/* Print ins flags: */
	if (r == RID_SINK || r == RID_SUNK)
		fprintf(out, "}");
	else
		fprintf(out, "%s", irt_isguard(type) ? ">" : " ");

	fprintf(out, "%s %s %-6s ", irt_isphi(type) ? "+" : " ",
		dump_ir_types[irt_type(type)], dump_ir_names[opcode]);

	int isircall = opcode == IR_CALLN || opcode == IR_CALLL ||
		       opcode == IR_CALLS || opcode == IR_CALLXS;

	if (isircall) {
		IRRef1 ctype_k = REF_DROP;

		if (mode2 == IRMlit)
			fprintf(out, "%-10s  (", dump_ir_call_names[op2]);
		else
			ctype_k = dump_ir_func_call(out, trace, op2);

		if (op1 != REF_DROP)
			dump_ir_func_args(out, trace, op1);

		fprintf(out, ")");

		if (ctype_k != REF_DROP) {
			fprintf(out, " ctype ");
			dump_ir_k(out, trace, ctype_k);
		}
	} else if (opcode == IR_CNEW && op2 == REF_DROP) {
		/*
		 * NB! The branch below corresponds to the following original
		 * code elseif op == "CNEW  " and op2 == -1 then
		 *    out:write(formatk(tr, op1))
		 * It looks like it should be investigated for correctness.
		 */
		dump_ir_k(out, trace, op1);
	} else if (mode1 != IRMnone) {
		/*
		 * Dump IR instruction operand 1.
		 * NB! Looks like IRMlit case is not handled properly for
		 * op1 in the original dump.lua. Probably will have to fix this.
		 */
		if (mode1 == IRMref && irref_isk(op1)) {
			dump_ir_k(out, trace, op1);
		} else {
			fprintf(out, mode1 == IRMref ? "%04u" : "#%-3u",
				dump_unbias_op1(ins));
		}

		/* Dump IR instruction operand 2. */
		if (mode2 != IRMnone)
			dump_ir_op2(out, trace, ins, op2, mode2, opcode);
	}

	if (ins->hints) {
		fprintf(out, " ;");

		if (ir_hashint(ins, IRH_TAB_METAIDX))
			fprintf(out, " TAB_METAIDX");

		if (ir_hashint(ins, IRH_TAB_SETMETA))
			fprintf(out, " TAB_SETMETA");

		if (ir_hashint(ins, IRH_TAB_NOMETAGUARD))
			fprintf(out, " TAB_NOMETAGUARD");

		if (ir_hashint(ins, IRH_MOVTV))
			fprintf(out, " MOVTV");

		if (ir_hashint(ins, IRH_LOAD_KEEPGUARD))
			fprintf(out, " LOAD_KEEPGUARD");

		if (ir_hashint(ins, IRH_MARK))
			fprintf(out, " MARK");
	}

	fprintf(out, "\n");
}

/* Dump single snapshot. */
static void dump_snapshot(FILE *out, const GCtrace *trace, const SnapShot *snap,
			  SnapNo sn)
{
#if DUMP_INTERLEAVED_SNAPSHOTS
	/*
	 * NB! SNAP_SOFTFPNUM is no longer supported,
	 * so this part is excluded from dumping.
	 */
	uint8_t slot;
	uint8_t num_entries = snap->nent;
	uint8_t i = 0;

	SnapEntry *entries = &trace->snapmap[snap->mapofs];

	fprintf(out, "....              SNAP   #%-3u [ ", (uint32_t)sn);
	for (slot = 0; slot < snap->nslots; slot++) {
		SnapEntry entry = entries[i];
		IRRef ref;

		if (i == num_entries || snap_slot(entry) != slot) {
			fprintf(out, "---- ");
			continue;
		}
		ref = snap_ref(entry);

		if (irref_isk(ref))
			dump_ir_k(out, trace, ref);
		else
			dump_ir_insno(out, ref);
		fprintf(out, snap_isframe(entry) ? "|" : " ");
		i++;
	}
	fprintf(out, "]\n");
#else /* DUMP_INTERLEAVED_SNAPSHOTS */
	UNUSED(out);
	UNUSED(trace);
	UNUSED(snap);
	UNUSED(sn);
#endif /* DUMP_INTERLEAVED_SNAPSHOTS */
}

/*
 * NB! The original dump_ir from dump.lua produces several types of output
 * (plain text, coloured ANSI, HTML). We support plain text only.
 */
void uj_dump_ir(FILE *out, const GCtrace *trace)
{
	IRRef ref;
	SnapNo sn = 0;

#if DUMP_INTERLEAVED_SNAPSHOTS
	SnapShot *snap = &trace->snap[sn];
#else /* DUMP_INTERLEAVED_SNAPSHOTS */
	SnapShot *snap = NULL;
#endif /* DUMP_INTERLEAVED_SNAPSHOTS */

	fprintf(out, "---- TRACE %d IR\n", trace->traceno);

	for (ref = REF_FIRST; ref < trace->nins; ref++) {
		if (snap && ref >= snap->ref) {
			dump_snapshot(out, trace, snap, sn);
			snap = (++sn < trace->nsnap ? &trace->snap[sn] : NULL);
		}
		dump_ir_ins(out, trace, ref);
	}

	if (snap)
		dump_snapshot(out, trace, snap, sn);
}
