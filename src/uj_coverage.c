/*
 * Platform-level coverage counting.
 *
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_def.h"
#include "uj_coverage.h"

#ifdef UJIT_COVERAGE

#include <regex.h>
#include <stdio.h>
#include <string.h>

#include "lj_obj.h"
#include "uj_obj_marks.h"
#include "lj_tab.h"
#include "lj_gc.h"
#include "uj_proto.h"
#include "frontend/lj_parse.h"
#include "frontend/lj_lex.h"
#include "utils/uj_crc.h"

#define COVERAGE_LINE_SIZE 1024
#define COVERAGE_LINE_FORMAT "%s\t%d\t%u"

enum coverage_state { CS_PAUSE, CS_STREAM };

struct re_array {
	regex_t *arr;
	size_t size;
};

struct coverage {
	struct GCtab *crc_cache;
	struct GCtab *exclude_cache;
	struct re_array excludes;
	enum coverage_state state;
	lua_Coveragewriter callback;
	void *cb_context;
	FILE *file;
};

static LJ_AINLINE const char *coverage_outname(const struct GCstr *chunkname)
{
	const char *outname = strdata(chunkname);
	const char *prefix = "script code ";

	/* Strip '@' and 'scrtipt code ' for back compatibility */
	if (outname[0] == '@')
		outname += 1;
	if (memcmp(outname, prefix, strlen(prefix)) == 0)
		outname += strlen(prefix);
	return outname;
}

static int coverage_re_compile(struct lua_State *L, struct re_array *output,
			       const char **excludes, size_t num)
{
	size_t i;

	output->size = num;
	if (output->size == 0)
		return 0;

	output->arr = uj_mem_alloc(L, (output->size) * sizeof(*output->arr));
	for (i = 0; i < num; i++) {
		regex_t re;
		const char *pattern = excludes[i];

		if (regcomp(&re, pattern, REG_NOSUB) != 0) {
			uj_mem_free(MEM(L), output->arr + i,
				    (output->size - i) * sizeof(*output->arr));
			output->size = i;
			return 1;
		}
		output->arr[i] = re;
	}
	return 0;
}

static void coverage_free_excludes(struct lua_State *L,
				   struct re_array *excludes)
{
	int i;

	if (excludes->size == 0)
		return;

	for (i = 0; i < excludes->size; i++)
		regfree(excludes->arr + i);
	uj_mem_free(MEM(L), excludes->arr,
		    (excludes->size) * sizeof(*excludes->arr));
}

static int coverage_exclude(struct lua_State *L, struct GCstr *chunkname)
{
	size_t i;
	struct coverage *coverage = G(L)->coverage;

	/* Check cache */
	if (lj_tab_getstr(coverage->exclude_cache, chunkname) != NULL)
		return 1;

	for (i = 0; i < coverage->excludes.size; ++i) {
		const regex_t *re = coverage->excludes.arr + i;

		if (regexec(re, strdata(chunkname), 0, NULL, 0) == 0) {
			union TValue *tv;

			/* Put into cache */
			fixstring(chunkname);
			/* NOBARRIER: fixed table holds fixed strings */
			tv = lj_tab_setstr(L, coverage->exclude_cache,
					   chunkname);
			setboolV(tv, 1);

			return 1;
		}
	}
	return 0;
}

static struct GCtab *coverage_new_cache(struct lua_State *L)
{
	/* Don't need array part and need moderately-sized hash part */
	struct GCtab *tab = lj_tab_new(L, 0, 8);

	tab->marked |= LJ_GC_FIXED;
	return tab;
}

static void coverage_free_cache(struct GCtab *crc_cache)
{
	size_t hsize = crc_cache->hmask + 1;
	Node *node = crc_cache->node;
	size_t i;

	for (i = 0; i < hsize; i++) {
		union TValue *tv = &node[i].key;

		if (!tvisnil(tv))
			strV(&node[i].key)->marked &= ~LJ_GC_FIXED;
	}
	crc_cache->marked &= ~LJ_GC_FIXED;
}

static LJ_AINLINE BCLine coverage_pc_line(const struct FuncState *fs, BCPos pc)
{
	return fs->bcbase[pc].line;
}

static LJ_AINLINE BCIns coverage_pc_ins(const struct FuncState *fs, BCPos pc)
{
	return fs->bcbase[pc].ins;
}

static LJ_AINLINE int coverage_is_jump(const struct FuncState *fs, BCPos pc)
{
	return bcmode_d(bc_op(coverage_pc_ins(fs, pc))) == BCMjump;
}

static LJ_AINLINE int coverage_is_itercn(const struct FuncState *fs, BCPos pc)
{
	return bc_op(coverage_pc_ins(fs, pc)) == BC_ITERC ||
	       bc_op(coverage_pc_ins(fs, pc)) == BC_ITERN;
}

/* Jump target is in the middle of another line */
static LJ_AINLINE int coverage_middle_jump(unsigned *targets,
					   const struct FuncState *fs, BCPos pc)
{
	ptrdiff_t offset;
	BCPos target;
	BCLine targetline;

	if (!coverage_is_jump(fs, pc))
		return 0;
	/*
	 * NB: another case of 'middle-jump' may be returning from a function,
	 * but for coverage purposes it doesn't matter.
	 * Also don't count as 'middle-jump' ITERC/ITERN. Its line will be
	 * streamed as it is the same as correspondent HOTCNT line,
	 * but exception allows not to break relative position of
	 * HOTCNT and cycle instructions.
	 */

	offset = bc_j(coverage_pc_ins(fs, pc));
	target = pc + 1 + offset;
	targetline = coverage_pc_line(fs, target);

	if (coverage_pc_line(fs, pc) == targetline ||
	    coverage_pc_line(fs, target - 1) != targetline ||
	    coverage_is_itercn(fs, target))
		return 0;

	targets[target] = 1;
	return 1;
}

/* Count how many COVERGs should be emitted */
static unsigned coverage_count(unsigned *targets, const struct FuncState *fs)
{
	BCPos pc;
	unsigned res = 0;
	BCLine line = 0;

	/* leave BC_FUNCV or BC_FUNCF + BC_HOTCNT as is */
	pc = bc_op(coverage_pc_ins(fs, 0)) == BC_FUNCF ? 2 : 1;
	for (; pc < fs->pc; ++pc) {
		BCLine curline = coverage_pc_line(fs, pc);

		if (coverage_middle_jump(targets, fs, pc))
			res++;

		if (line != curline) {
			res++;
			line = curline;
		}
	}
	return res;
}

static BCPos coverage_find_split_pos(const struct FuncState *fs, BCPos pc,
				     const unsigned *targets)
{
	BCLine curline = coverage_pc_line(fs, pc);
	BCPos i = pc;

	/* Do not go beyound first line */
	lua_assert(curline > 0);

	while (curline == coverage_pc_line(fs, i)) {
		if (targets[i] == 1) /* 'Middle-jump' */
			return i;
		if (i == 0) /* BC_FUNCV */
			return 1;
		if (i == 1 && bc_op(coverage_pc_ins(fs, i)) == BC_HOTCNT)
			return 2; /* BC_FUNCF + BC_HOTCNT */
		i--;
	}
	return i + 1;
}

struct shift_info {
	unsigned *to_new; /* BCPos at pc will be shifted by to_new[pc] */
	unsigned *to_old; /* BCPos at pc was shifted by to_old[pc] */
};

static LJ_AINLINE BCPos coverage_new_pos(BCPos old,
					 const struct shift_info *shifts)
{
	return old + shifts->to_new[old];
}

static LJ_AINLINE BCPos coverage_old_pos(BCPos new,
					 const struct shift_info *shifts)
{
	return new - shifts->to_old[new];
}

static void coverage_patch_jump_target(struct FuncState *fs, BCPos pc,
				       const struct shift_info *shifts)
{
	ptrdiff_t newoffset;
	BCIns *ins = &fs->bcbase[pc].ins;
	ptrdiff_t oldoffset = bc_j(*ins);
	BCPos oldtarget = coverage_old_pos(pc, shifts) + 1 + oldoffset;
	BCPos newtarget = coverage_new_pos(oldtarget, shifts);

	if ((coverage_pc_line(fs, newtarget) != coverage_pc_line(fs, pc)) &&
	    !coverage_is_itercn(fs, newtarget)) {
		/* Retarget jump to prev ins which should be COVERG bytecode */
		newtarget--;
		lua_assert(bc_op(coverage_pc_ins(fs, newtarget)) == BC_COVERG);
	}
	newoffset = newtarget - pc - 1;
	setbc_j(ins, newoffset);
}

/*
 * For given pc finds a correct place to emit COVERG and returns BCPos
 * of next (backward) instruction that should be instrumented
 */
static BCPos coverage_instrument_pc(struct FuncState *fs, BCPos pc,
				    struct shift_info *shifts, unsigned covergs,
				    const unsigned *jmptargets)
{
	BCPos splitpos = coverage_find_split_pos(fs, pc, jmptargets);
	size_t bcnum = pc - splitpos + 1;
	BCPos newpos;
	unsigned i;

	/*
	 * Split should not be between condition and jump.
	 * (Assume that condition opcodes are those that <= BC_ISF.)
	 */
	lua_assert(bc_op(coverage_pc_ins(fs, splitpos - 1)) > BC_ISF);

	for (i = splitpos; i <= pc; i++)
		shifts->to_new[i] = covergs;

	newpos = splitpos + covergs;
	memmove(fs->bcbase + newpos, fs->bcbase + splitpos,
		bcnum * sizeof(*fs->bcbase));
	fs->bcbase[newpos - 1].ins = BCINS_AD(BC_COVERG, 0, 0);
	fs->bcbase[newpos - 1].line = coverage_pc_line(fs, newpos);

	for (i = newpos - 1; i < newpos + bcnum; i++)
		shifts->to_old[i] = covergs;

	return splitpos - 1;
}

static void coverage_patch_varinfo(struct FuncState *fs,
				   const struct shift_info *shifts)
{
	VarInfo *vstart = fs->ls->vstack + fs->vbase;
	VarInfo *vend = fs->ls->vstack + fs->ls->vtop;
	VarInfo *vi;

	for (vi = vstart; vi < vend; vi++) {
		vi->startpc = coverage_new_pos(vi->startpc, shifts);
		if (!gola_isgotolabel(vi))
			vi->endpc = coverage_new_pos(vi->endpc, shifts);
	}
}

static struct coverage *coverage_new(struct lua_State *L)
{
	G(L)->coverage = uj_mem_alloc(L, sizeof(*G(L)->coverage));
	return G(L)->coverage;
}

static int coverage_start_helper(struct lua_State *L, const char **excludes,
				 size_t num)
{
	struct coverage *coverage = G(L)->coverage;

	if (coverage_re_compile(L, &coverage->excludes, excludes, num) != 0) {
		coverage_free_excludes(L, &coverage->excludes);
		if (coverage->file != NULL)
			fclose(coverage->file);
		uj_mem_free(MEM(L), coverage, sizeof(*coverage));
		G(L)->coverage = NULL;
		return LUAE_COV_ERROR;
	}

	coverage->state = CS_STREAM;
	coverage->crc_cache = coverage_new_cache(L);
	coverage->exclude_cache = coverage_new_cache(L);
	return LUAE_COV_SUCCESS;
}

void uj_coverage_emit(struct FuncState *fs)
{
	BCPos pc;
	unsigned i;
	unsigned covergs;
	struct shift_info shifts;
	/* Hold 1 for targets that should be additionally instrumented */
	unsigned *jmptargets;
	const BCPos old_pc = fs->pc;

	shifts.to_new = uj_mem_calloc(fs->L, old_pc * sizeof(*shifts.to_new));
	jmptargets = uj_mem_calloc(fs->L, old_pc * sizeof(*jmptargets));

	/* Emit placeholders for needed COVERG instructions. */
	covergs = coverage_count(jmptargets, fs);
	for (i = 0; i < covergs; i++)
		lj_parse_bcemit(fs, BCINS_AD(BC_COVERG, 0, 0));

	shifts.to_old = uj_mem_calloc(fs->L, fs->pc * sizeof(*shifts.to_old));

	/*
	 * Now `covergs` means how many COVERGs remains unemitted.
	 * Traverse bytecodes backwards and place COVERGs between lines.
	 */
	pc = old_pc - 1;
	while (covergs > 0) {
		pc = coverage_instrument_pc(fs, pc, &shifts, covergs,
					    jmptargets);
		covergs--;
	}

	/* Patch jumps to account COVERG emission */
	for (pc = 0; pc < fs->pc; pc++) {
		if (coverage_is_jump(fs, pc))
			coverage_patch_jump_target(fs, pc, &shifts);
	}

	coverage_patch_varinfo(fs, &shifts);

	uj_mem_free(MEM(fs->L), shifts.to_new, old_pc * sizeof(*shifts.to_new));
	uj_mem_free(MEM(fs->L), shifts.to_old, fs->pc * sizeof(*shifts.to_old));
	uj_mem_free(MEM(fs->L), jmptargets, old_pc * sizeof(*jmptargets));
}

void uj_coverage_pause(struct lua_State *L)
{
	if (uj_coverage_enabled(L))
		G(L)->coverage->state = CS_PAUSE;
}

void uj_coverage_unpause(struct lua_State *L)
{
	if (uj_coverage_enabled(L))
		G(L)->coverage->state = CS_STREAM;
}

void uj_coverage_stream_line(struct lua_State *L, const BCIns *pc)
{
	const struct GCproto *pt;
	BCLine line;
	struct GCstr *chunkname;
	const char *outname = "";
	const union TValue *crctv;
	uint32_t crc;
	struct coverage *coverage = G(L)->coverage;
	FILE *file = coverage->file;

	if (coverage == NULL || coverage->state != CS_STREAM)
		return;

	/*
	 * NB: The same line could be streamed twice in a row, which
	 * is still fine for coverage purposes.
	 */

	pt = curr_proto(L);
	chunkname = proto_chunkname(pt);
	crctv = lj_tab_getstr(coverage->crc_cache, chunkname);
	if (!crctv) {
		union TValue *tv;

		if (coverage_exclude(L, chunkname))
			return;

		fixstring(chunkname);
		/*
		 * NOBARRIER: Fixed tab holds fixed string as keys
		 * and non-gc raw values
		 */
		tv = lj_tab_setstr(L, coverage->crc_cache, chunkname);
		outname = coverage_outname(chunkname);
		setrawV(tv, uj_crc32(outname));
		crctv = (const union TValue *)tv;
	}

	crc = (uint32_t)rawV(crctv);
	line = uj_proto_line(pt, proto_bcpos(pt, pc));
	if (file != NULL) {
		fprintf(file, COVERAGE_LINE_FORMAT "\n", outname, line, crc);
	} else if (coverage->callback != NULL) {
		char buf[COVERAGE_LINE_SIZE];
		int size;

		size = snprintf(buf, COVERAGE_LINE_SIZE, COVERAGE_LINE_FORMAT,
				outname, line, crc);
		coverage->callback(coverage->cb_context, buf, size);
	}
}

int uj_coverage_start(struct lua_State *L, const char *filename,
		      const char **excludes, size_t num)
{
	FILE *file = NULL;
	struct coverage *coverage;

	if (uj_coverage_enabled(L))
		return LUAE_COV_ERROR;

	if (filename == NULL)
		return LUAE_COV_ERROR;

	file = fopen(filename, "a");
	if (file == NULL)
		return LUAE_COV_ERROR;

	coverage = coverage_new(L);
	coverage->file = file;
	coverage->callback = NULL;
	coverage->cb_context = NULL;
	return coverage_start_helper(L, excludes, num);
}

int uj_coverage_start_cb(struct lua_State *L, lua_Coveragewriter cb,
			 void *context, const char **excludes, size_t num)
{
	struct coverage *coverage;

	if (uj_coverage_enabled(L))
		return LUAE_COV_ERROR;

	if (cb == NULL)
		return LUAE_COV_ERROR;

	coverage = coverage_new(L);
	coverage->file = NULL;
	coverage->callback = cb;
	coverage->cb_context = context;
	return coverage_start_helper(L, excludes, num);
}

int uj_coverage_stop(struct lua_State *L)
{
	struct coverage *coverage = G(L)->coverage;

	if (!uj_coverage_enabled(L))
		return LUAE_COV_ERROR;

	coverage_free_excludes(L, &coverage->excludes);
	coverage_free_cache(coverage->crc_cache);
	coverage_free_cache(coverage->exclude_cache);
	if (coverage->file != NULL)
		fclose(coverage->file);
	coverage->file = NULL;
	uj_mem_free(MEM(L), coverage, sizeof(*coverage));
	G(L)->coverage = NULL;

	return LUAE_COV_SUCCESS;
}

#else /* UJIT_COVERAGE */

void uj_coverage_pause(struct lua_State *L)
{
	UNUSED(L);
}

void uj_coverage_unpause(struct lua_State *L)
{
	UNUSED(L);
}

int uj_coverage_start(struct lua_State *L, const char *filename,
		      const char **excludes, size_t num)
{
	UNUSED(L);
	UNUSED(filename);
	UNUSED(excludes);
	UNUSED(num);
	return LUA_COV_ERROR;
}

int uj_coverage_start_cb(struct lua_State *L, lua_Coveragewriter cb,
			 void *context, const char **excludes, size_t num)
{
	UNUSED(L);
	UNUSED(cb);
	UNUSED(context);
	UNUSED(excludes);
	UNUSED(num);
	return LUA_COV_ERROR;
}

int uj_coverage_stop(struct lua_State *L)
{
	UNUSED(L);
	return LUA_COV_ERROR;
}

#endif /* UJIT_COVERAGE */
