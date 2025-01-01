/*
 * Implementation of metamethod handling.
 * Copyright (C) 2020-2025 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_err.h"
#include "uj_throw.h"
#include "uj_errmsg.h"
#include "uj_str.h"
#include "lj_tab.h"
#include "lj_debug.h"
#include "uj_meta.h"
#include "uj_mtab.h"
#include "lj_frame.h"
#include "uj_cframe.h"
#include "uj_state.h"
#include "lj_vm.h"
#include "utils/uj_math.h"
#if LJ_HASJIT
#include "jit/lj_trace.h"
#endif /* LJ_HASJIT */

LJ_DATADEF const uint8_t uj_mm_narg[] = {
#define MMNARG(mm, narg) narg,
	MMDEF(MMNARG)
#undef MMNARG
};

/* String interning of metamethod names for fast indexing. */
void uj_meta_init(lua_State *L)
{
#define MMNAME(name, nargs) "__" #name
	const char *metanames = MMDEF(MMNAME);
#undef MMNAME
	global_State *g = G(L);
	const char *p;
	const char *q;
	uint32_t mm;
	for (mm = 0, p = metanames; *p; mm++, p = q) {
		GCstr *s;
		for (q = p + 2; *q && *q != '_'; q++)
			;
		s = uj_str_new(L, p, (size_t)(q - p));
		/* NOBARRIER: g->gcroot[] is a GC root. */
		g->gcroot[GCROOT_MMNAME + mm] = obj2gco(s);
	}
}

/* -- Error handling for metamethods -------------------------------------- */

/* Formatted runtime error message. */
LJ_NORET LJ_NOINLINE static void meta_err(lua_State *L, const char *efmt, ...)
{
	const char *msg;

	va_list argp;
	va_start(argp, efmt);
	uj_state_stack_sync_top(L);
	msg = uj_str_pushvf(L, efmt, argp);
	va_end(argp);

	lj_debug_addloc(L, msg, L->base - 1, NULL);
	uj_throw_run(L);
}

/* Typecheck error for operands. */
static void meta_err_optype(lua_State *L, const TValue *tv, enum err_msg opm)
{
	GCproto *pt;
	const BCIns *pc;
	const char *tname = lj_typename(tv);
	const char *opname = uj_errmsg(opm);
	const char *oname = NULL;
	const char *kind;

	if (!curr_funcisL(L)) {
		meta_err(L, uj_errmsg(UJ_ERR_BADOPRV), opname, tname);
		return; /* unreachable */
	}

	pt = curr_proto(L);
	pc = uj_cframe_Lpc(L) - 1;
	kind = lj_debug_slotname(pt, pc, (BCReg)(tv - L->base), &oname);

	if (kind) {
		const char *efmt = uj_errmsg(UJ_ERR_BADOPRT);

		meta_err(L, efmt, opname, kind, oname, tname);
	} else {
		const char *efmt = uj_errmsg(UJ_ERR_BADOPRV);

		meta_err(L, efmt, opname, tname);
	}
}

/* Typecheck error for __call. */
static void meta_err_optype_call(lua_State *L, TValue *tv)
{
	const BCIns *pc = uj_cframe_Lpc(L);
	const char *tname;

	if (((ptrdiff_t)pc & FRAME_TYPE) == FRAME_LUA) {
		meta_err_optype(L, tv, UJ_ERR_OPCALL);
		return; /* unreachable */
	}

	/*
	 * Gross hack if lua_[p]call or pcall/xpcall fail for a non-callable
	 * object: L->base still points to the caller. So add a dummy frame with
	 * L instead of a function. See lua_getstack().
	 */
	tname = lj_typename(tv);
	frame_set_dummy(L, tv, (int64_t)pc);
	L->base = L->top = tv + 1;
	meta_err(L, uj_errmsg(UJ_ERR_BADCALL), tname);
}

/* Typecheck error for ordered comparisons. */
static void meta_err_comp(lua_State *L, const TValue *tv1, const TValue *tv2)
{
	const char *t1 = lj_typename(tv1);
	const char *t2 = lj_typename(tv2);

	meta_err(L, uj_errmsg(t1 == t2 ? UJ_ERR_BADCMPV : UJ_ERR_BADCMPT), t1,
		 t2);
	/* This assumes the two "boolean" entries are commoned by the C compiler. */
}

/* -- Metamethod handling ------------------------------------------------- */

/* Looks up a metamethod mm in the metatable mt. */
static const TValue *meta_lookup_mtv(const global_State *g, const GCtab *mt,
				     enum MMS mm)
{
	return lj_tab_getstr(mt, uj_meta_name(g, mm));
}

const TValue *uj_meta_lookup(const lua_State *L, const TValue *tv, enum MMS mm)
{
	const GCtab *mt = uj_mtab_get(L, tv);
	const TValue *mtv;

	if (!mt)
		return niltv(L);

	mtv = meta_lookup_mtv(G(L), mt, mm);
	return (mtv != NULL) ? mtv : niltv(L);
}

const TValue *uj_meta_lookup_mt(const global_State *g, GCtab *mt, enum MMS mm)
{
	const TValue *mtv;

	lua_assert(mm <= MM_FAST);

	if (NULL == mt || (mt->nomm & (1u << mm)))
		return NULL;

	mtv = meta_lookup_mtv(g, mt, mm);
	if (!mtv || tvisnil(mtv)) {
		/* If there is no metamethod, set negative cache flag: */
		mt->nomm |= (uint8_t)(1u << mm);
		return NULL;
	}
	return mtv;
}

#if LJ_HASFFI
/* Tailcall from C function. */
int uj_meta_tailcall(lua_State *L, const TValue *tv)
{
	/*
	 * before:   [old_mo|PC]    [... ...]
	 *                         ^base     ^top
	 * after:    [new_mo|itype] [... ...] [NULL|PC] [dummy|delta]
	 *                                                           ^base/top
	 * tailcall: [new_mo|PC]    [... ...]
	 *                         ^base     ^top
	 */
	int64_t ftsz;
	TValue *base = L->base;
	TValue *top = L->top;
	BCIns *pc = frame_pc(base - 1); /* Preserve old PC from frame. */
	copyTV(L, base - 1, tv); /* Replace frame with new object. */
	top->u32.lo = LJ_CONT_TAILCALL;
	setframe_pc(top, pc);
	ftsz = (int64_t)((char *)(top + 2) - (char *)base) + FRAME_CONT;
	frame_set_dummy(L, top + 1, ftsz);
	L->base = L->top = top + 2;
	return 0;
}
#endif /* LJ_HASFFI */

/*
 * Setup call to metamethod to be run by Assembler VM.
 * NB! There is a special case when MM called from C API.
 *           |-- framesize -> top       top+1        top+2 top+3
 * before:   [func slots ...]
 * mm setup: [func slots ...] [cont|?]  [mtv|tmtype] [a]   [b]
 * in asm:   [func slots ...] [cont|PC] [mtv|delta]  [a]   [b]
 * C API:    [func slots ...] [nil]     [mtv|delta]  [a]   [b]
 *           ^-- func base                           ^-- mm base
 * after mm: [func slots ...]           [result]
 *                ^-- copy to base[PC_RA] --/     for lj_cont_ra
 *                          istruecond + branch   for lj_cont_cond *
 *                                       ignore   for lj_cont_nop
 * next PC:  [func slots ...]
 */
static TValue *meta_prepare_mmcall(lua_State *L, TValue *top, ASMFunction cont,
				   const TValue *mtv, const TValue *a,
				   const TValue *b)
{
	TValue *mm_base = top + 2;

	/*
	 * Careful with the order of stack copies! Second argument located in
	 * L->top must be copied before being overwritten by continuation frame.
	 */
	lua_assert(top != NULL);
	copyTV(L, mm_base + 1, b); /* Store metamethod and two arguments. */
	copyTV(L, mm_base + 0, a);
	copyTV(L, mm_base - 1, mtv);

	/*
	 * Continuation slot is set to nil, which will later be rewritten by
	 * VM to store the return PC. In the case of C calls, however, this
	 * slot is not used and will be ignored without causing potential
	 * issues during stack traversal.
	 */
	setnilV(mm_base - 2);

	/* Assembler VM stores PC in upper word. */
	setcont(mm_base - 2, cont);
	return mm_base;
}

void uj_meta_mmcall(lua_State *L, TValue *newbase, int nargs, int nres)
{
	L->top = newbase + nargs;
	lj_vm_call(L, newbase, nres + 1);
}

/* -- C helpers for some instructions, called from assembler VM ----------- */

static LJ_AINLINE void meta_store_mark(lua_State *L, GCtab *t)
{
	t->nomm = 0; /* Invalidate negative metamethod cache. */
	lj_gc_anybarriert(L, t);
}

/*
 * Helper to reference a nil result of t[k] in case of no applicable metamethods
 * (i.e. the same as the regular metatable-less behaviour):
 * * For loads, returns a VM-level nil "constant";
 * * For stores, sets a barrier and ensures a valid memory location.
 */
static TValue *meta_index_nomm(lua_State *L, GCtab *t, const TValue *k,
			       TValue *v, const enum MMS mm)
{
	lua_assert(tvisnil(v));
	lua_assert(mm == MM_index || mm == MM_newindex);

	if (mm == MM_index)
		return niltv(L);

	meta_store_mark(L, t);

	if (v != niltv(L))
		return v; /* v already points inside the table. */

	if (tvisnil(k))
		uj_err(L, UJ_ERR_NILIDX);
	else if (tvisnum(k) && tvisnan(k))
		uj_err(L, UJ_ERR_NANIDX);

	return lj_tab_newkey(L, t, k);
}

struct index_context {
	const ASMFunction cont;
	const enum MMS mm;
	const enum err_msg err;
};

static const struct index_context mm_index = {.cont = lj_cont_ra,
					      .mm = MM_index,
					      .err = UJ_ERR_GETLOOP};

static const struct index_context mm_newindex = {.cont = lj_cont_nop,
						 .mm = MM_newindex,
						 .err = UJ_ERR_SETLOOP};

/*
 * Helper for TGET* and TSET*. __index/__newindex chain and metamethod.
 * Performs generic tv[k] lookup:
 *  1. If raw tv[k] lookup returns a non-nil value, returns a pointer to
 *     this value.
 *  2. If raw tv[k] lookup returns a nil value, inspects tv's metatable.
 *  2.1. If metatable is not found, returns a pointer to a nil object.
 *       NB! The returned pointer may or may not point "inside" tv,
 *       see e.g. niltv for more details.
 *  2.2. If metatable is found, but it does not contain a relevant metaobject,
 *       does same as the 2.1.
 *  2.3. If metatable is found and it contains a relevant metaobject,
 *       inspects the type of the metaobject.
 *  2.3.1. If the metaobject is a function (i.e. a metamethod is found),
 *         prepares corresponding metamethod call on the stack and returns NULL.
 *  2.3.2. If the metaobject is not a function, recursively performs a generic
 *         lookup with the key k on the metaobject.
 */
static TValue *meta_index(lua_State *L, const TValue *tv, const TValue *k,
			  const struct index_context *ctx)
{
	int loop;
	const ASMFunction cont = ctx->cont;
	const enum MMS mm = ctx->mm;

	for (loop = 0; loop < LJ_MAX_IDXCHAIN; loop++) {
		const TValue *mtv = NULL;
		if (tvistab(tv)) {
			GCtab *t = tabV(tv);
			TValue *v = (TValue *)lj_tab_get(L, t, k);

			if (!tvisnil(v)) {
				if (mm == MM_newindex)
					meta_store_mark(L, t);
				return v;
			}

			mtv = uj_meta_lookup_mt(G(L), t->metatable, mm);
			if (mtv == NULL)
				return meta_index_nomm(L, t, k, v, mm);
		} else {
			mtv = uj_meta_lookup(L, tv, mm);
			if (tvisnil(mtv))
				meta_err_optype(L, tv, UJ_ERR_OPINDEX);
		}

		if (tvisfunc(mtv)) {
			L->top = meta_prepare_mmcall(L, curr_top(L), cont, mtv,
						     tv, k);
			return NULL;
		}

		tv = mtv;
	}
	uj_err(L, ctx->err);
	return NULL; /* unreachable */
}

/* Helper for TGET*. __index chain and metamethod. */
const TValue *uj_meta_tget(lua_State *L, const TValue *tv, const TValue *k)
{
	return meta_index(L, tv, k, &mm_index);
}

/* Helper for TSET*. __newindex chain and metamethod. */
TValue *uj_meta_tset(lua_State *L, const TValue *tv, const TValue *k)
{
	return meta_index(L, tv, k, &mm_newindex);
}

/* Helper for arithmetic instructions. Coercion, metamethod. */
TValue *uj_meta_arith(lua_State *L, TValue *ra, const TValue *rb,
		      const TValue *rc, BCReg op)
{
	enum MMS mm = bcmode_mm(op);
	TValue tempb;
	TValue tempc;
	const TValue *b;
	const TValue *c;
	const TValue *mtv;

	copyTV(L, &tempb, rb);
	copyTV(L, &tempc, rc);
	b = uj_str_tonumber(&tempb) ? &tempb : NULL;
	c = uj_str_tonumber(&tempc) ? &tempc : NULL;

	if (b != NULL && c != NULL) { /* Try coercion first. */
		enum FoldarithOp op = (enum FoldarithOp)((int)mm - MM_add);
		setnumV(ra, uj_math_foldarith(numV(b), numV(c), op));
		return NULL;
	}

	mtv = uj_meta_lookup(L, rb, mm);
	if (tvisnil(mtv)) {
		mtv = uj_meta_lookup(L, rc, mm);
		if (tvisnil(mtv)) {
			if (b == NULL)
				rc = rb;
			meta_err_optype(L, rc, UJ_ERR_OPARITH);
			return NULL; /* unreachable */
		}
	}
	return meta_prepare_mmcall(L, curr_top(L), lj_cont_ra, mtv, rb, rc);
}

/* ---- Concatenation ------------------------------------------------------ */

/* Given that at least one of the top two elements is not a string, prepares the
 * stack for a __concat metamethod call and returns the metamethod base. Stack
 * layout is as follows for calls from VM:
 *
 * before:    [...][CAT stack .........................]
 *                                 top-1     top         top+1 top+2
 * pick two:  [...][CAT stack ...] [o1]      [o2]
 * setup mm:  [...][CAT stack ...] [cont|?]  [mo|tmtype] [o1]  [o2]
 * in asm:    [...][CAT stack ...] [cont|PC] [mo|delta]  [o1]  [o2]
 *            ^-- func base                              ^-- mm base
 * after mm:  [...][CAT stack ...] <--push-- [result]
 * next step: [...][CAT stack .............]
 *
 * For calls from C API, [cont|PC] slot is omitted.
 */
static TValue *meta_cat_setup(lua_State *L, TValue *top)
{
	const TValue *mtv = NULL;

	lua_assert(!uj_str_is_coercible(top - 1) || !uj_str_is_coercible(top));

	if (!uj_str_is_coercible(top - 1))
		mtv = uj_meta_lookup(L, top - 1, MM_concat);

	if (mtv == NULL || tvisnil(mtv))
		mtv = uj_meta_lookup(L, top, MM_concat);

	lua_assert(mtv != NULL);

	if (tvisnil(mtv)) {
		if (uj_str_is_coercible(top - 1))
			top++;
		meta_err_optype(L, top - 1, UJ_ERR_OPCAT);
		return NULL; /* unreachable */
	}

	top = meta_prepare_mmcall(L, top - 1, lj_cont_cat, mtv, top - 1, top);

	return top; /* Trigger metamethod call. */
}

TValue *uj_meta_cat(lua_State *L, TValue *bottom, TValue *top)
{
	top = uj_str_cat_step(L, bottom, top);

	if (top == bottom)
		return NULL;
	return meta_cat_setup(L, top);
}

/* Helper for LEN. __len metamethod. */
TValue *uj_meta_len(lua_State *L, const TValue *tv)
{
	const TValue *mtv = uj_meta_lookup(L, tv, MM_len);
	if (!tvisnil(mtv))
		return meta_prepare_mmcall(L, curr_top(L), lj_cont_ra, mtv, tv,
					   LJ_52 ? tv : niltv(L));

	if (LJ_52 && tvistab(tv))
		tabV(tv)->metatable->nomm |= (uint8_t)(1u << MM_len);
	else
		meta_err_optype(L, tv, UJ_ERR_OPLEN);

	return NULL;
}

/* Helper for equality comparisons. __eq metamethod. */
TValue *uj_meta_equal(lua_State *L, GCobj *o1, GCobj *o2, int ne)
{
	/* Field metatable must be at same offset for GCtab and GCudata! */
	GCtab *mt1 = o1->gch.metatable;
	GCtab *mt2 = o2->gch.metatable;
	const TValue *mtv = uj_meta_lookup_mt(G(L), mt1, MM_eq);
	TValue *top;
	TValue tv1;
	TValue tv2;
	uint32_t it;

	if (mtv == NULL)
		return (TValue *)(intptr_t)ne;

	if (mt1 != mt2) {
		const TValue *mtv2 = uj_meta_lookup_mt(G(L), mt2, MM_eq);
		if (mtv2 == NULL || !uj_obj_equal(mtv, mtv2))
			return (TValue *)(intptr_t)ne;
	}

	top = curr_top(L);
	it = ~(uint32_t)o1->gch.gct;
	setgcV(L, &tv1, o1, it);
	setgcV(L, &tv2, o2, it);

	top = meta_prepare_mmcall(L, top, ne ? lj_cont_condf : lj_cont_condt,
				  mtv, &tv1, &tv2);

	return top; /* Trigger metamethod call. */
}

#if LJ_HASFFI
TValue *uj_meta_equal_cd(lua_State *L, BCIns ins)
{
	ASMFunction cont = (bc_op(ins) & 1) ? lj_cont_condf : lj_cont_condt;
	int op = (int)bc_op(ins) & ~1;
	TValue tmp;
	const TValue *mtv, *tv2, *tv1 = &L->base[bc_a(ins)];
	const TValue *tv1mm = tv1;
	if (op == BC_ISEQV) {
		tv2 = &L->base[bc_d(ins)];
		if (!tviscdata(tv1mm))
			tv1mm = tv2;
	} else if (op == BC_ISEQS) {
		GCobj *ko = proto_kgc(curr_proto(L), ~(ptrdiff_t)bc_d(ins));
		setstrV(L, &tmp, gco2str(ko));
		tv2 = &tmp;
	} else if (op == BC_ISEQN) {
		tv2 = &((const TValue *)(curr_proto(L)->k))[bc_d(ins)];
	} else {
		lua_assert(op == BC_ISEQP);
		settag(&tmp, ~bc_d(ins));
		tv2 = &tmp;
	}
	mtv = uj_meta_lookup(L, tv1mm, MM_eq);
	if (LJ_LIKELY(!tvisnil(mtv)))
		return meta_prepare_mmcall(L, curr_top(L), cont, mtv, tv1, tv2);
	else
		return (TValue *)(intptr_t)(bc_op(ins) & 1);
}
#endif /* LJ_HASFFI */

/* Selects a proper comparison continuation based on op. */
static LJ_AINLINE ASMFunction meta_comp_select_cont(int op)
{
	return (op & 1) ? lj_cont_condf : lj_cont_condt;
}

/* Selects a proper metamethod based on op. */
static LJ_AINLINE enum MMS meta_comp_select_mm(int op)
{
	return (op & 2) ? MM_le : MM_lt;
}

/* Selects a metaobject (depending on compatibility with Lua 5.2). */
static const TValue *meta_comp_select_mtv(const TValue *mtv1,
					  const TValue *mtv2)
{
#if LJ_52
	if (!tvisnil(mtv1))
		return mtv1;
	if (!tvisnil(mtv2))
		return mtv2;
	return NULL;
#else /* !LJ_52 */
	if (tvisnil(mtv1) || !uj_obj_equal(mtv1, mtv2))
		return NULL;
	return mtv1;
#endif /* LJ_52 */
}

#if LJ_HASFFI
static TValue *meta_comp_ffi(lua_State *L, const TValue *tv1, const TValue *tv2,
			     int op)
{
	lua_assert(tviscdata(tv1) || tviscdata(tv2));
	ASMFunction cont = meta_comp_select_cont(op);
	enum MMS mm = meta_comp_select_mm(op);
	const TValue *mtv = uj_meta_lookup(L, tviscdata(tv1) ? tv1 : tv2, mm);
	if (LJ_UNLIKELY(tvisnil(mtv)))
		meta_err_comp(L, tv1, tv2);
	return meta_prepare_mmcall(L, curr_top(L), cont, mtv, tv1, tv2);
}
#endif /* LJ_HASFFI */

static TValue *meta_comp_try_prepare_call(lua_State *L, const TValue *tv1,
					  const TValue *tv2, int op)
{
	enum MMS mm = meta_comp_select_mm(op);
	ASMFunction cont;

	const TValue *mtv = meta_comp_select_mtv(uj_meta_lookup(L, tv1, mm),
						 uj_meta_lookup(L, tv2, mm));

	if (mtv == NULL)
		return NULL;

	cont = meta_comp_select_cont(op);
	return meta_prepare_mmcall(L, curr_top(L), cont, mtv, tv1, tv2);
}

/* Helper for ordered comparisons. String compare, __lt/__le metamethods. */
TValue *uj_meta_comp(lua_State *L, const TValue *tv1, const TValue *tv2, int op)
{
	int try_comp;
	const TValue *tmp;
	TValue *mm_base;

#if LJ_HASFFI
	if (tviscdata(tv1) || tviscdata(tv2))
		return meta_comp_ffi(L, tv1, tv2, op);
#endif

	try_comp = LJ_52 || gettag(tv1) == gettag(tv2) ||
		   (tvisbool(tv1) && tvisbool(tv2));

	if (!try_comp)
		meta_err_comp(L, tv1, tv2);

	lua_assert(!(tvisnum(tv1) && tvisnum(tv2)));

	if (tvisstr(tv1) && tvisstr(tv2)) {
		int32_t cmp_res = uj_str_cmp(strV(tv1), strV(tv2));
		int res = (((op & 2) ? cmp_res <= 0 : cmp_res < 0) ^ (op & 1));
		return (TValue *)(intptr_t)res;
	}

	mm_base = meta_comp_try_prepare_call(L, tv1, tv2, op);
	if (mm_base != NULL)
		return mm_base;

	if (meta_comp_select_mm(op) != MM_le)
		meta_err_comp(L, tv1, tv2);

	/*
	 * MM_le not found: Swap operands, flip condition
	 * and retry with MM_lt: (a <= b) <=> !(b < a)
	 */
	tmp = tv1;
	tv1 = tv2;
	tv2 = tmp;
	op ^= 3; /* both opcode and negation are baked into 3 = 1 + 2. */

	mm_base = meta_comp_try_prepare_call(L, tv1, tv2, op);
	if (mm_base != NULL)
		return mm_base;

	meta_err_comp(L, tv1, tv2);
	return NULL; /* unreachable */
}

/* Helper for calls. __call metamethod. */
void uj_meta_call(lua_State *L, TValue *func, TValue *top)
{
	TValue *p;
	const TValue *mtv = uj_meta_lookup(L, func, MM_call);
	if (!tvisfunc(mtv))
		meta_err_optype_call(L, func);
	for (p = top; p > func; p--)
		copyTV(L, p, p - 1);
	copyTV(L, func, mtv);
}

/* Helper for FORI. Coercion. */
void uj_meta_for(lua_State *L, TValue *tv)
{
	if (!uj_str_tonumber(tv))
		uj_err(L, UJ_ERR_FORINIT);
	if (!uj_str_tonumber(tv + 1))
		uj_err(L, UJ_ERR_FORLIM);
	if (!uj_str_tonumber(tv + 2))
		uj_err(L, UJ_ERR_FORSTEP);
}

const TValue *uj_meta_index(lua_State *L)
{
	uj_meta_mmcall(L, L->top, uj_mm_narg[MM_index], 1);
	L->top -= uj_mm_narg[MM_index];
	return L->top + 1;
}

const TValue *uj_meta_rindex(lua_State *L, size_t n)
{
	size_t i;
	const TValue *tv;
	ptrdiff_t rel_base;

	lua_assert(n > 0);

	rel_base = uj_state_stack_save(L, L->base);
	tv = L->base;
	for (i = 1; i < n; i++) {
		const TValue *k;
		const TValue *v;

		if (!tvistab(tv))
			return niltv(L);

		k = uj_state_stack_restore(L, rel_base) + i;
		v = uj_meta_tget(L, tv, k);

		if (v != NULL) {
			tv = v;
		} else {
#if LJ_HASJIT
			lj_trace_abort(G(L));
#endif /* LJ_HASJIT */
			tv = uj_meta_index(L);
		}
	}

	lua_assert(tv != NULL);
	return tv;
}
