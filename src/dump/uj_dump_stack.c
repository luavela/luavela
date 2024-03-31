/*
 * C-level stack dumper for Lua states.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "uj_str.h"
#include "lj_frame.h"

#include "dump/uj_dump_utils.h"

#define SLOT_VALUE_BUFFER_SIZE 40
#define SLOT_VALUE_COPY_THRESHOLD 30

enum {
	FRAME_LIVE, /* Live frame, chain linked
		     * with other frames on the stack
		     */
	FRAME_DEAD /* Dead frame, left between two active frames */
};

static void dump_print_stack_anchor(FILE *out, const lua_State *L,
				    const TValue *slot)
{
	fprintf(out, "%p                [%s%s%s%s] ", (void *)slot,
		slot == L->stack ? "S" : " ", slot == L->base ? "B" : " ",
		slot == L->top ? "T" : " ", slot == L->maxstack ? "M" : " ");
}

static void dump_slot_range(FILE *out, const TValue *hi_slot,
			    const TValue *lo_slot)
{
	size_t range_size = (size_t)(hi_slot - lo_slot) + 1;

	fprintf(out, "%p:%p [    ] %ld slot%s", (void *)hi_slot,
		(void *)lo_slot, range_size, range_size != 1 ? "s" : "");
}

static void dump_top_slots(FILE *out, const lua_State *L)
{
	const TValue *hi_mm_slot = &L->stack[L->stacksize - 1];
	const TValue *lo_mm_slot = L->maxstack + 1;
	const TValue *hi_free_slot;
	const TValue *lo_free_slot;

	dump_slot_range(out, hi_mm_slot, lo_mm_slot);
	fprintf(out, ": Red zone\n");

	dump_print_stack_anchor(out, L, L->maxstack);
	fprintf(out, "\n");

	hi_free_slot = L->maxstack - 1;
	lo_free_slot = L->top + 1;

	dump_slot_range(out, hi_free_slot, lo_free_slot);
	fprintf(out, ": Free stack slots\n");

	dump_print_stack_anchor(out, L, L->top);
	fprintf(out, "\n");
}

static void dump_aux_cont_slot(FILE *out, const lua_State *L,
			       const TValue *slot)
{
	dump_print_stack_anchor(out, L, slot);

	UJ_PEDANTIC_OFF
	/* casting a function ptr to void* */
	fprintf(out, " CONT: C:%p\n", (void *)frame_contf(slot + 1));
	UJ_PEDANTIC_ON
}

/*
 * Dumps a frame slot and possible extra slot(s) if particular layout assumes
 * utilizing aux slots on the stack.
 */
static ptrdiff_t dump_frame_slot(FILE *out, const lua_State *L,
				 const TValue *slot, int type)
{
	const GCfunc *fn = frame_func(slot);
	ptrdiff_t extra_slots = 0;

	dump_print_stack_anchor(out, L, slot);
	if (type == FRAME_LIVE)
		fprintf(out, "FRAME: ");
	else if (type == FRAME_DEAD)
		fprintf(out, "DEADF: ");
	else
		lua_assert(0); /* Unknown liveness type. */

	/*
	 * "Type of code" which created current frame
	 * Frame protection status: "P" = protected, " " = normal
	 * Delta (in slots) between current frame and caller frame
	 */

	if (frame_ispcall(slot)) {
		/* Lua frame inside an aux (x)pcall Lua frame */
		fprintf(out, "[PP]");
	} else {
		switch (frame_type(slot)) {
		case FRAME_LUA:
			fprintf(out, "[L");
			break;
		case FRAME_C:
			fprintf(out, "[C");
			break;
		/* metamethod aka continuation */
		case FRAME_CONT:
			fprintf(out, "[M");
			break;
		case FRAME_VARG:
			fprintf(out, "[V");
			break;
		default:
			/* Unknown frame type. */
			lua_assert(0);
		}
		fprintf(out, (frame_typep(slot) & FRAME_P) ? "P]" : " ]");
	}

	fprintf(out, " delta=%lu ",
		(unsigned long)(frame_islua(slot) ? frame_deltal(slot)
						  : frame_delta(slot)));

	uj_dump_func_description(out, fn, 0);
	fprintf(out, "\n");

	if (frame_iscont(slot)) {
		dump_aux_cont_slot(out, L, slot - 1);
		extra_slots++;
	}

	return extra_slots;
}

/*
 * Dumps a value slot if its tag was clobbered on the stack. We can come across
 * such slots when e.g. VM fixes L->top in advance and dumper is called right
 * after that and before these "pre-allocated" slots are filled with any
 * meaningful values.
 */
static void dump_clobbered_value_slot(FILE *out, const lua_State *L,
				      const TValue *slot)
{
	uint8_t gct = slot->gcr->gch.gct;

	/*
	 * We can identify slots that were occupied by callable
	 * objects which had already finished execution.
	 */
	if (gct == ~LJ_TFUNC) {
		dump_frame_slot(out, L, slot, FRAME_DEAD);
		return;
	}

	dump_print_stack_anchor(out, L, slot);
	fprintf(out, "NOTAG: gct=%i\n", gct);
}

static void dump_value_slot(FILE *out, const lua_State *L, const TValue *slot)
{
	char slot_value[SLOT_VALUE_BUFFER_SIZE] = {0};

	if (!tagisvalid(slot)) {
		dump_clobbered_value_slot(out, L, slot);
		return;
	}

	dump_print_stack_anchor(out, L, slot);
	fprintf(out, "VALUE: %s", lj_typename(slot));

	/* Feel free to add pretty-printed values of other types on demand */
	if (tvisfalse(slot)) {
		fprintf(out, ", false");
	} else if (tvistrue(slot)) {
		fprintf(out, ", true");
	} else if (tvisstr(slot)) {
		uj_dump_format_kstr(slot_value, strdata(strV(slot)),
				    SLOT_VALUE_COPY_THRESHOLD);
	} else if (tvisfunc(slot)) {
		fprintf(out, ", ");
		uj_dump_func_description(out, funcV(slot), 0);
	} else if (tvisnum(slot)) {
		uj_cstr_fromnum(slot_value, numV(slot));
	}

	if (slot_value[0])
		fprintf(out, ", %s", slot_value);

	fprintf(out, "\n");
}

static void dump_bottom_slot(FILE *out, const lua_State *L, const TValue *slot)
{
	dump_print_stack_anchor(out, L, slot);
	fprintf(out, frame_isdummy(L, slot) ? "FRAME: dummy L\n"
					    : "FRAME: [UNKNOWN BOTTOM]\n");
}

void uj_dump_stack(FILE *out, const lua_State *L)
{
	const TValue *frame, *slot;
	const TValue *boundary = L->top;
	const TValue *bottom = L->stack;

	dump_top_slots(out, L);

	/* Traverse frames backwards. */
	for (frame = L->base - 1; frame > bottom;) {
		ptrdiff_t extra_slots;

		for (slot = boundary - 1; slot > frame; slot--)
			dump_value_slot(out, L, slot);

		extra_slots = dump_frame_slot(out, L, slot, FRAME_LIVE);

		boundary = frame - extra_slots;
		frame = frame_prev(frame);
	}

	/* Print bottom slots (between dummy L and the first real frame). */
	for (slot = boundary - 1; slot > bottom; slot--)
		dump_value_slot(out, L, slot);

	dump_bottom_slot(out, L, bottom);
}
