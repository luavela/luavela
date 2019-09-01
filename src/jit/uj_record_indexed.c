/*
 * Indexed lods / store recorder.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "uj_meta.h"
#include "jit/lj_iropt.h"
#include "jit/lj_record.h"
#include "jit/lj_trace.h"
#include "jit/uj_record_indexed.h"
#include "uj_dispatch.h"
#include "lj_tab.h"

#define CALLEE 0
#define ARG1 1
#define ARG2 2
#define ARG3 3

#define REC_MM 0 /* JIT is prepared to record metamethod. */
#define REC_MO 1 /* Mobj isn't a function, continue lookup. */

#define TREF_CONTINUE_REC ((TRef)-1) /* Special return code. */

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref) (&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b) (lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

struct rec_store {
	int keybarrier;
	GCtab *mt;
};

static LJ_AINLINE int record_is_store(const struct RecordIndex *ix)
{
	return ix->val;
}

static LJ_AINLINE void record_hint_metaidx(jit_State *J, struct RecordIndex *ix)
{
	if (!tref_istab(ix->tab) || tref_isk(ix->tab))
		return;

	if (tabV(&ix->tabv)->metatable != NULL)
		ir_sethint(IR(tref_ref(ix->tab)), IRH_TAB_METAIDX);
}

/* Prepare metaobject recording. */
static int record_handle_mobj(struct jit_State *J, struct RecordIndex *ix)
{
	BCReg func;
	TRef *base;
	TValue *tv;

	if (!tref_isfunc(ix->mobj)) {
		/* Retry lookup with metaobject. */
		ix->tab = ix->mobj;
		copyTV(J->L, &ix->tabv, &ix->mobjv);

		if (0 == --ix->idxchain)
			lj_trace_err(J, LJ_TRERR_IDXLOOP);
		return REC_MO;
	}

	/* Handle metamethod call. */
	func = lj_record_mm_prep(J, record_is_store(ix) ? lj_cont_nop :
							  lj_cont_ra);
	base = J->base + func;
	tv = J->L->base + func;

	base[CALLEE] = ix->mobj;
	base[ARG1] = ix->tab;
	base[ARG2] = ix->key;

	setfuncV(J->L, tv + CALLEE, funcV(&ix->mobjv));
	copyTV(J->L, tv + ARG1, &ix->tabv);
	copyTV(J->L, tv + ARG2, &ix->keyv);

	if (record_is_store(ix)) {
		base[ARG3] = ix->val;
		copyTV(J->L, tv + ARG3, &ix->valv);
		/* mobj(tab, key, val) */
		lj_record_call(J, func, uj_mm_narg[MM_newindex]);
	} else {
		/* res = mobj(tab, key) */
		lj_record_call(J, func, uj_mm_narg[MM_index]);
		/* No result yet. */
	}

	return REC_MM;
}

/* Recording next metamethod. */
static TRef record_next_mm(struct jit_State *J, struct RecordIndex *ix)
{
	if (REC_MM != record_handle_mobj(J, ix))
		return TREF_CONTINUE_REC;

	return 0;
}

static int record_defer_canon(const struct jit_State *J,
			      const struct RecordIndex *ix)
{
	IRType t = itype2irt(ix->oldv);

	if (!irtype_ispri(t))
		return 0;

	if (IRT_NIL == t && ir_hashint(IR(tref_ref(ix->tab)), IRH_TAB_METAIDX))
		return 0;

	return lj_opt_movtv_defer_canon(J);
}

static TRef record_indexed_load(struct jit_State *J, struct RecordIndex *ix)
{
	const TValue *niltv = niltvg(J2G(J));
	IRType t = itype2irt(ix->oldv);
	int defer_canon = record_defer_canon(J, ix);
	TRef res;

	if (ix->oldv == niltv && !defer_canon) {
		emitir(IRTG(IR_EQ, IRT_P32), ix->xref,
		       lj_ir_kkptr(J, (TValue *)niltv));
		res = TREF_NIL;
	} else {
		res = emitir(IRTG(ix->loadop, t), ix->xref, 0);
	}

	if (IRT_NIL == t && ix->idxchain &&
	    lj_record_mm_lookup(J, ix, MM_index))
		return record_next_mm(J, ix);

	/* Canonicalize primitives. */
	if (irtype_ispri(t) && !defer_canon)
		res = TREF_PRI(t);
	return res;
}

/* Determine whether a key is NOT one of the fast metamethod names. */
static int record_nommstr(const struct jit_State *J, TRef key)
{
	const GCstr *str;
	uint32_t mm;

	if (!tref_isstr(key))
		return 1; /* CANNOT be a metamethod name. */

	if (!tref_isk(key))
		return 0; /* Variable string key MAY be a metamethod name. */

	str = ir_kstr(IR(tref_ref(key)));

	for (mm = 0; mm <= MM_FAST; mm++)
		if (str == uj_meta_name(J2G(J), mm))
			return 0; /* MUST be one the fast metamethod names. */

	return 1; /* CANNOT be a metamethod name. */
}

static TRef record_store_to_nil(struct jit_State *J, struct RecordIndex *ix,
				struct rec_store *recst)
{
	/* Need to duplicate the hasmm check for the early guards. */
	int hasmm = 0;
	const TValue *niltv = niltvg(J2G(J));

	if (ix->idxchain && recst->mt) {
		const TValue *mo = lj_tab_getstr(
			recst->mt, uj_meta_name(J2G(J), MM_newindex));

		hasmm = mo && !tvisnil(mo);
	}

	if (hasmm) {
		/* Guard for nil value. */
		emitir(IRTG(ix->loadop, IRT_NIL), ix->xref, 0);
	} else if (IR_HREF == ix->xrefop) {
		TRef irk_niltv = lj_ir_kkptr(J, (TValue *)niltv);

		emitir(IRTG(ix->oldval == niltv ? IR_EQ : IR_NE, IRT_P32),
		       ix->xref, irk_niltv);
	}

	if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_newindex)) {
		lua_assert(hasmm);

		return 1;
	}

	lua_assert(!hasmm);

	if (ix->oldval == niltv) { /* Need to insert a new key. */
		TRef key = ix->key;

		if (tref_isinteger(key)) /* NEWREF needs a TValue as a key. */
			key = emitir(IRTN(IR_CONV), key, IRCONV_NUM_INT);

		ix->xref = emitir(IRT(IR_NEWREF, IRT_P32), ix->tab, key);
		/* NEWREF already takes care of the key barrier. */
		recst->keybarrier = 0;
	}

	return 0;
}

static void record_store_to_nonnil(struct jit_State *J,
				   const struct RecordIndex *ix,
				   const struct rec_store *recst)
{
	const TValue *niltv = niltvg(J2G(J));
	TRef irk_niltv = lj_ir_kkptr(J, (TValue *)niltv);

	if (IR_HREF == ix->xrefop) /* Guard against store to niltv. */
		emitir(IRTG(IR_NE, IRT_P32), ix->xref, irk_niltv);

	if (!ix->idxchain) /* Metamethod lookup required? */
		return;

	/* A check for NULL metatable is cheaper (hoistable) than a load. */
	if (!recst->mt) {
		TRef mtref =
			emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_TAB_META);

		emitir(IRTG(IR_EQ, IRT_TAB), mtref, lj_ir_knull(J, IRT_TAB));
	} else {
		IRType t = itype2irt(ix->oldval);

		/* Guard for non-nil value. */
		emitir(IRTG(ix->loadop, t), ix->xref, 0);
	}
}

static TRef record_indexed_store(struct jit_State *J, struct RecordIndex *ix)
{
	struct rec_store recst;

	lj_ir_emit_immutable_guard(J, ix->tab);
	recst.mt = tabV(&ix->tabv)->metatable;
	recst.keybarrier = tref_isgcv(ix->key) && !tref_isnil(ix->val);

	if (tvisnil(ix->oldval)) {
		/* Previous value was nil? */
		if (record_store_to_nil(J, ix, &recst))
			return record_next_mm(J, ix);
	} else if (!lj_opt_fwd_wasnonnil(J, ix->loadop, tref_ref(ix->xref))) {
		/*
		 * Cannot derive that the previous value was non-nil,
		 * must do checks.
		 */
		record_store_to_nonnil(J, ix, &recst);
	} else {
		/* Previous non-nil value kept the key alive. */
		recst.keybarrier = 0;
	}

	/* Convert int to number before storing. */
	if (tref_isinteger(ix->val))
		ix->val = emitir(IRTN(IR_CONV), ix->val, IRCONV_NUM_INT);

	emitir(IRT(ix->loadop + IRDELTA_L2S, tref_type(ix->val)), ix->xref,
	       ix->val);

	if (recst.keybarrier || tref_isgcv(ix->val))
		emitir(IRT(IR_TBAR, IRT_NIL), ix->tab, 0);

	/*
	 * Invalidate neg. metamethod cache for stores with certain string
	 * keys.
	 */
	if (!record_nommstr(J, ix->key)) {
		TRef fref =
			emitir(IRT(IR_FREF, IRT_P32), ix->tab, IRFL_TAB_NOMM);

		emitir(IRT(IR_FSTORE, IRT_U8), fref, lj_ir_kint(J, 0));
	}

	J->needsnap = 1;
	return 0;
}

static TRef record_hash_lookup(jit_State *J, const GCtab *t,
			       const RecordIndex *ix, const TRef key)
{
	const size_t hmask = t->hmask;
	const size_t hslot =
		(size_t)((char *)ix->oldv - (char *)&t->node[0].val);
	const size_t hslot_idx = hslot / sizeof(Node);
	TRef hm;
	TRef node;
	TRef kslot;
	int hrefk_disabled = J->flags & JIT_F_OPT_NOHREFK;
	/*
	 * Refers to the second part of condition:
	 * DEPRECATED. Try to optimize lookup of constant hash keys.
	 * The idea behind this optimization is to save the pair
	 * [constant key; key's offset into table's storage] on recording to
	 * re-use it directly during trace execution without having to
	 * re-calculate hash of the key. Sounds tempting, but this approach
	 * requires a guard for hmask value. As a result, as runtime processes
	 * tables with different internal layouts, this guard starts triggering
	 * many faulty side-exits causing trace explosion and eventual
	 * peformance degradation.
	 */
	int hrefk_not_possible =
		!tref_isk(key) ||
		!(hmask > 0 && hslot_idx <= hmask && hslot_idx <= MAX_KSLOT);

	if (hrefk_disabled || hrefk_not_possible)
		/* Fall back to a regular hash lookup. */
		return emitir(IRT(IR_HREF, IRT_P32), ix->tab, key);

	hm = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_HMASK);
	emitir(IRTGI(IR_EQ), hm, lj_ir_kint(J, (int32_t)hmask));
	node = emitir(IRT(IR_FLOAD, IRT_P32), ix->tab, IRFL_TAB_NODE);
	kslot = lj_ir_kslot(J, key, hslot_idx);
	return emitir(IRTG(IR_HREFK, IRT_P32), node, kslot);
}

static TRef record_num_key(jit_State *J, RecordIndex *ix, TRef *key, GCtab *t)
{
	int32_t k = lj_num2int(numV(&ix->keyv));

	if (numV(&ix->keyv) != (lua_Number)k)
		k = LJ_MAX_ASIZE;

	if ((size_t)k < LJ_MAX_ASIZE) { /* Potential array key? */
		TRef ikey = lj_opt_narrow_index(J, *key);
		TRef asizeref = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_ASIZE);

		if ((size_t)k < t->asize) {
			/* Currently an array key? */
			TRef arrayref;

			lj_record_idx_abc(J, asizeref, ikey, t->asize);
			arrayref = emitir(IRT(IR_FLOAD, IRT_P32), ix->tab,
					  IRFL_TAB_ARRAY);
			return emitir(IRT(IR_AREF, IRT_P32), arrayref, ikey);
		} else {
			/* Hash key (may be an array extension)? */
			/* Inv. bounds check. */
			emitir(IRTGI(IR_ULE), asizeref, ikey);

			if (k == 0 && tref_isk(*key))
				/* Canonicalize 0 or +-0.0 to +0.0. */
				*key = lj_ir_knum_zero(J);

			/* And continue with the hash lookup. */
		}
	} else if (!tref_isk(*key)) {
		/*
		 * We can rule out const numbers which failed the integerness
		 * test above. But all other numbers are potential array keys.
		 */
		if (t->asize == 0) {
			/* True sparse tables have an empty array part. */
			TRef tmp;

			/* Guard that the array part stays empty. */
			tmp = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_ASIZE);
			emitir(IRTGI(IR_EQ), tmp, lj_ir_kint(J, 0));
		} else {
			lj_trace_err(J, LJ_TRERR_NYITMIX);
		}
	}

	return TREF_CONTINUE_REC;
}

/* Record indexed key lookup. */
static TRef record_idx_key(jit_State *J, RecordIndex *ix)
{
	TRef key;
	GCtab *t = tabV(&ix->tabv);
	ix->oldv = lj_tab_get(J->L, t, &ix->keyv); /* Lookup previous value. */

	/* Integer keys are looked up in the array part first. */
	key = ix->key;

	if (tref_isnumber(key)) {
		TRef keyref = record_num_key(J, ix, &key, t);

		if (TREF_CONTINUE_REC != keyref)
			return keyref;
	}

	/* Otherwise the key is located in the hash part. */
	if (t->hmask == 0 && !record_defer_canon(J, ix)) {
		/*
		 * Shortcut for empty hash part.
		 * Need a guard that the hash part stays empty.
		 */
		TRef tmp = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_HMASK);

		emitir(IRTGI(IR_EQ), tmp, lj_ir_kint(J, 0));
		return lj_ir_kkptr(J, niltvg(J2G(J)));
	}

	/* Hash keys are based on numbers, not ints. */
	if (tref_isinteger(key))
		key = emitir(IRTN(IR_CONV), key, IRCONV_NUM_INT);

	return record_hash_lookup(J, t, ix, key);
}

static TRef record_idx(struct jit_State *J, struct RecordIndex *ix)
{
	enum MMS mm = record_is_store(ix) ? MM_newindex : MM_index;

	while (!tref_istab(ix->tab)) { /* Handle non-table lookup. */
		/* Never call raw record_idx() on non-table. */
		lua_assert(0 != ix->idxchain);

		if (!lj_record_mm_lookup(J, ix, mm))
			lj_trace_err(J, LJ_TRERR_NOMM);

		if (REC_MM == record_handle_mobj(J, ix))
			return 0;
	}

	/* First catch nil and NaN keys for tables. */
	if (tvisnil(&ix->keyv) || (tvisnum(&ix->keyv) && tvisnan(&ix->keyv))) {
		if (record_is_store(ix)) /* Better fail early. */
			lj_trace_err(J, LJ_TRERR_STORENN);

		if (tref_isk(ix->key)) {
			if (ix->idxchain &&
			    lj_record_mm_lookup(J, ix, MM_index))
				return record_next_mm(J, ix);
			return TREF_NIL;
		}
	}

	record_hint_metaidx(J, ix);

	/* Record the key lookup. */
	ix->xref = record_idx_key(J, ix);
	ix->xrefop = IR(tref_ref(ix->xref))->o;
	ix->loadop = ix->xrefop == IR_AREF ? IR_ALOAD : IR_HLOAD;
	/* The uj_meta_tset() inconsistency is gone, but better play safe. */
	ix->oldval = ix->xrefop == IR_KKPTR ?
			     (const TValue *)ir_kptr(IR(tref_ref(ix->xref))) :
			     ix->oldv;

	if (record_is_store(ix))
		return record_indexed_store(J, ix);
	else
		return record_indexed_load(J, ix);
}

TRef uj_record_indexed(struct jit_State *J, struct RecordIndex *ix)
{
	TRef ret;

	do {
		ret = record_idx(J, ix);
	} while (ret == TREF_CONTINUE_REC);

	return ret;
}

#undef emitir
#undef IR
