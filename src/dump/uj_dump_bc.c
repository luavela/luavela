/*
 * C-level bytecode dumper for functions, protos and single instructions.
 * Port of LuaJIT's original bytecode dumper bc.lua with some modifications
 * and reasonable simplifications (marked with NB! throughout the code).
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <errno.h>

#include "lj_bc.h"
#include "uj_proto.h"
#include "uj_str.h"

#include "dump/uj_dump_datadef.h"
#include "dump/uj_dump_utils.h"
/* max 80 chars + \0 terminator */
#define HINT_BUF_SIZE 81
/* worst case: "<34 chars>\127"~ (41 chars) */
#define FMT_KSTR_THRESHOLD_LEN 35
/* + " ; " (3 chars) for delimiting hints */
#define FMT_UVNAME_MAX_LEN 35
/* max 100 chars displayed in src line */
#define MAX_SRC_LINE_LEN 100

struct source_dumper_state {
	FILE *file; /* a source file corresponding to a dumped chunk */
	BCLine line; /* line number from which fgetc(file) will return char */
	char buf[MAX_SRC_LINE_LEN + 2]; /* for reading lines from file */
	/* source line + '\n' (from fgets) + '\0' */
};

/*
 * Prints padding at the beginning of a prologue or a byte code line.
 * NB! Not implemented in the original bc.lua.
 */
static void dump_bc_print_padding(FILE *out, uint8_t nest_level)
{
	uint8_t padding;

	for (padding = 0; padding < nest_level; padding++)
		fprintf(out, ". ");
}

/*
 * Copies upvalue name uvname into fmt_buf stripping it with a '~' character if
 * the name appears to be too long.
 */
static void dump_bc_format_uvname(char *fmt_buf, const char *uvname)
{
	size_t uvname_len = strlen(uvname);

	if (uvname_len > FMT_UVNAME_MAX_LEN) {
		uvname_len = FMT_UVNAME_MAX_LEN - 1;
		fmt_buf[uvname_len] = '~';
	}
	memcpy(fmt_buf, uvname, uvname_len);
}

void uj_dump_bc_ins(FILE *out, const GCproto *pt, const BCIns *ins,
		    uint8_t nest_level)
{
	const int pos = (int)proto_bcpos(pt, ins);
	const uint8_t op = bc_op(*ins);
	const int mode_a = bcmode_a(op);
	const int mode_b = bcmode_b(op);
	const int mode_c = bcmode_c(op);
	/*
	 * Buffer for holding a pretty-formatted hint (originates from
	 * constant values referred to by operands of the current instruction):
	 */
	GCobj *ko;
	char hint[HINT_BUF_SIZE];

	/*
	 *  instruction's 2nd operand,
	 *  either 8-bit `c` or 16-bit `d` (default):
	 */
	BCReg operand2 = bc_d(*ins);

	/*
	 * NB! Original bc.lua prepends '=>' to the line if the instruction is
	 * a target of any jump. This is omitted in current implementation as
	 * it really adds a little value while making our interfaces looks
	 * uglier: would have to add to the signature an is_jmp_target flag
	 * plus introduce a lua_State dependency for allocating memory on heap.
	 */
	fprintf(out, "%04d    ", pos);
	dump_bc_print_padding(out, nest_level);
	fprintf(out, "%-6s ", dump_bc_names[op]);

	if (mode_a != BCMnone) {
		fprintf(out, "%3d ", bc_a(*ins));
		if (op == BC_FUNCF || op == BC_FUNCV) {
			fprintf(out, "         ; ");
			uj_proto_fprintloc(out, pt, 0);
			fprintf(out, "\n");
			return;
		}
	} else {
		if (mode_b == BCMnone && mode_c == BCMnone) {
			fprintf(out, "\n");
			return;
		}
		fprintf(out, "    ");
	}

	if (mode_c == BCMjump) {
		fprintf(out, "=> %04d\n", proto_bcpos(pt, bc_target(ins)));
		return;
	}

	if (mode_b != BCMnone) {
		operand2 = bc_c(*ins);
	} else if (mode_c == BCMnone) {
		fprintf(out, "\n");
		return;
	}

	memset(hint, 0, HINT_BUF_SIZE);

	switch (mode_c) {
	case BCMstr: {
		ko = proto_kgc(pt, -((ptrdiff_t)operand2 + 1));
		uj_dump_format_kstr(hint, strdata(gco2str(ko)),
				    FMT_KSTR_THRESHOLD_LEN);
		break;
	}
	case BCMnum: {
		double nvalue = numV(proto_knumtv(pt, (ptrdiff_t)operand2));

		if (op == BC_TSETM)
			sprintf(hint, "%d",
				(int)(nvalue - ((int64_t)0x1 << 52)));
		else
			uj_cstr_fromnum(hint, nvalue);
		break;
	}
	case BCMfunc: {
		/*
		 * NB! Original bc.lua assumes that we can receive a fast
		 * function here. But looks like we cannot, so this part of
		 * logic is omitted.
		 */
		ko = proto_kgc(pt, -((ptrdiff_t)operand2 + 1));
		uj_proto_sprintloc(hint, gco2pt(ko), 0);
		break;
	}
	case BCMuv: {
		dump_bc_format_uvname(hint, uj_proto_uvname(pt, operand2));
		break;
	}
	default: {
		break;
	}
	}

	if (mode_a == BCMuv) {
		/*
		 * Type of operand a is an index to an upvalue: upvalue's name
		 * will be used in the hint. If we already have a non-empty
		 * hint, following * will be performed (pseudo-code):
		 *     hint = uvnamea + " ; " + hint
		 * Otherwise uvnamea will simply be copied to the hint.
		 * NB! Each time upvalue's name is too long for the hint buffer
		 * it will be stripped, e.g.: too_long_uv_na~. Not implemented
		 * in the original bc.lua.
		 */
		const char *uvnamea = uj_proto_uvname(pt, bc_a(*ins));

		if (hint[0]) {
			size_t uvnamea_len = strlen(uvnamea);

			if (uvnamea_len > FMT_UVNAME_MAX_LEN)
				uvnamea_len = FMT_UVNAME_MAX_LEN;
			memmove(hint + uvnamea_len + 3, hint, strlen(hint));
			hint[uvnamea_len] = ' ';
			hint[uvnamea_len + 1] = ';';
			hint[uvnamea_len + 2] = ' ';
		}
		dump_bc_format_uvname(hint, uvnamea);
	}

	if (mode_b != BCMnone) {
		fprintf(out, "%3d %3d", bc_b(*ins), operand2);
		if (hint[0])
			fprintf(out, "  ; %s", hint);
		fprintf(out, "\n");
		return;
	}

	if (hint[0]) {
		fprintf(out, "%3d      ; %s\n", operand2, hint);
		return;
	}

	if (mode_c == BCMlits && operand2 > 32767)
		operand2 = operand2 - 65536;
	fprintf(out, "%3d\n", operand2);
}

void uj_dump_nonlua_bc_ins(FILE *out, const GCfunc *func, uint8_t nest_level)
{
	lua_assert(!isluafunc(func));

	fprintf(out, "0000    ");
	dump_bc_print_padding(out, nest_level);
	fprintf(out, "FUNCC               ; ");
	uj_dump_nonluafunc_description(out, func);

	fprintf(out, "\n");
}

static void dump_bc_print_header(FILE *out, const GCproto *pt)
{
	fprintf(out, "-- BYTECODE -- ");
	uj_proto_fprintloc(out, pt, 0);
	fprintf(out, "-%d\n", pt->firstline + pt->numline);
}

static FILE *dump_bc_open_chunk(const GCproto *pt)
{
	const char *chunk_path = proto_chunknamestr(pt);
	FILE *file;

	chunk_path++; /* skip reserved prefix */
	file = fopen(chunk_path, "r");

	if (file == NULL)
		fprintf(stderr, "Error opening file %s: %s\n", chunk_path,
			strerror(errno));

	return file;
}

/* Skip to a new line in a file (can reach EOF if on the last line) */
static void dump_bc_skip_to_new_line(struct source_dumper_state *sds)
{
	char c;

	do {
		c = fgetc(sds->file);
	} while (c != EOF && c != '\n');

	sds->line++;
	/*
	 * If the file has N lines, reaching EOF will set sds->line to N + 1
	 * as if doing fgets next would attempt to read from (N+1)th line
	 */
}

/*
 * Skip to a line with number 'src_line'
 * Assumes that fgetc will return first symbol of sds->line
 * We only read forward, so 'src_line' should be >= sds->line
 */
static void dump_bc_skip_to_line(struct source_dumper_state *sds,
				 BCLine src_line)
{
	lua_assert(src_line >= sds->line);
	while (sds->line != src_line) {
		dump_bc_skip_to_new_line(sds);

		/* check that EOF is not reached before we're at needed line */
		lua_assert(!feof(sds->file));
	}
}

/*
 * Add "[...]" postfix to long source line.
 * Will replace last characters of the line making it MAX_SRC_LINE_LEN long
 */
static void dump_bc_add_long_line_postfix(struct source_dumper_state *sds)
{
	const char postfix[] = "[...]";
	const size_t postfix_len = sizeof(postfix) - 1;

	lua_assert(strlen(sds->buf) == MAX_SRC_LINE_LEN);

	sds->buf[MAX_SRC_LINE_LEN - postfix_len] = '\0';

	UJ_STRINGOP_TRUNCATION_WARN_OFF
	/* false-positive - no unintended truncation */
	strncat(sds->buf, postfix, postfix_len);
	UJ_STRINGOP_TRUNCATION_WARN_ON
}

/*
 * Get a line from file into the line buffer.
 * Assumes that at the start of the function, we're on the first char of a line
 */
static void dump_bc_get_file_line(struct source_dumper_state *sds)
{
	size_t line_len;

	/* edge case: MAX_SRC_LINE_LEN chars + '\n' + '\0' */
	if (fgets(sds->buf, MAX_SRC_LINE_LEN + 2, sds->file) == NULL) {
		/* Anything wrong? Pretend we've read an empty line. */
		sds->buf[0] = '\0';
		sds->line++;
		return;
	}

	line_len = strlen(sds->buf);
	lua_assert(line_len != 0);

	if (sds->buf[line_len - 1] == '\n') { /* read the whole line */
		sds->buf[line_len - 1] = '\0';
		sds->line++; /* next char to be read will be on next line */
	} else {
		sds->buf[MAX_SRC_LINE_LEN] = '\0';
		dump_bc_add_long_line_postfix(sds);
		/* skip to new line for dump_bc_skip_to_line to work */
		dump_bc_skip_to_new_line(sds);
	}
}

/* Prints a source line `src_line` if it wasn't printed before */
static void dump_bc_print_source_line(struct source_dumper_state *sds,
				      FILE *out, BCLine src_line)
{
	if (feof(sds->file) || /* printed the whole source already */
	    sds->line > src_line) /* can't read file backwards */
		return;

	dump_bc_skip_to_line(sds, src_line);
	dump_bc_get_file_line(sds);
	fprintf(out, "%d\t%s\n", src_line, sds->buf);
}

static void dump_bc_proto(FILE *out, const GCproto *pt, int dump_src,
			  BCPos hl_bc_pos);

static void dump_bc_protos(FILE *out, const GCproto *pt, int dump_src)
{
	for (ptrdiff_t idx = -1; idx >= -(ptrdiff_t)pt->sizekgc; idx--) {
		const GCobj *gc = proto_kgc(pt, idx);

		if (gc->gch.gct == ~LJ_TPROTO)
			dump_bc_proto(out, gco2pt(gc), dump_src, NO_BCPOS);
	}
}

/*
 * Dump bytecodes of a current frame
 * If dump_src == 1 - prints corresponding source from 'file'
 * If hl_bc_pos != NO_BCPOS - highlights a bytecode with " -> "
 */
static void dump_bc_impl(FILE *out, FILE *file, const GCproto *pt, int dump_src,
			 BCPos hl_bc_pos)
{
	struct source_dumper_state sds;
	size_t pc;
	const BCIns *bcs = proto_bc(pt);

	sds.file = file;
	sds.line = 1;

	dump_bc_print_header(out, pt);
	/*
	 * NB! Original bc.lua never prints the 1st FUNCF op. We follow this
	 * behaviour now for better output compatibility, may be changed in
	 * future.
	 */
	for (pc = 1; pc < pt->sizebc; pc++) {
		if (dump_src) {
			BCLine src_line = uj_proto_line(pt, pc);

			dump_bc_print_source_line(&sds, out, src_line);
		}

		if (hl_bc_pos != NO_BCPOS)
			fprintf(out, "%s", pc == hl_bc_pos ? " -> " : "    ");

		uj_dump_bc_ins(out, pt, &(bcs[pc]), 0);
	}
	fprintf(out, "\n");
}

/*
 * Dump all bytecode instructions of the prototype to output.
 */
static void dump_bc_proto(FILE *out, const GCproto *pt, int dump_src,
			  BCPos hl_bc_pos)
{
	FILE *file = NULL; /* a file with source code corresponding to pt */

	if (uj_proto_is_cmdline_chunk(pt))
		dump_src = 0;

	/* TODO: make this optional */
	if (pt->flags & PROTO_CHILD)
		dump_bc_protos(out, pt, dump_src);

	if (dump_src) {
		file = dump_bc_open_chunk(pt);
		if (file == NULL)
			return;
	}

	dump_bc_impl(out, file, pt, dump_src, hl_bc_pos);

	if (file != NULL)
		fclose(file);
}

void uj_dump_bc(FILE *out, const GCfunc *func)
{
	if (isluafunc(func))
		dump_bc_proto(out, funcproto(func), 0, NO_BCPOS);
	else
		uj_dump_nonlua_bc_ins(out, func, 0);
}

void uj_dump_bc_and_source(FILE *out, const GCfunc *func, BCPos hl_bc_pos)
{
	if (isluafunc(func))
		dump_bc_proto(out, funcproto(func), 1, hl_bc_pos);
	else
		uj_dump_nonlua_bc_ins(out, func, 0);
}
