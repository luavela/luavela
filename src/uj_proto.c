/*
 * Prototype handling.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

/*
 * TODO:
 *  * bc_* and bcop_* functions should be replaced with data-driven approach.
 */

#include "lj_obj.h"
#include "uj_obj_immutable.h"
#include "lj_gc.h"
#include "uj_mem.h"
#include "lj_bc.h"
#include "uj_proto.h"
#include "uj_str.h"
#include "utils/lj_char.h"
#include "utils/leb128.h"

/* Offset that should be added to a hot-counting op code to disable
 * hot-counting and effectively switch it to its interpreter-only counterpart.
 */
#define BC_OFFSET_DISABLE_HOTCOUNT ((int)BC_ILOOP - (int)BC_LOOP)

/* Offset that should be added to a non-hot-counting op code
 * to re-enable hot-counting.
 */
#define BC_OFFSET_ENABLE_HOTCOUNT ((int)BC_LOOP - (int)BC_ILOOP)

static LJ_AINLINE int bcop_accesses_globals(const BCOp op)
{
	return op == BC_GGET || op == BC_GSET;
}

static int bcop_is_hotcounting(const BCOp op)
{
	return bc_isloop(op) || op == BC_FUNCF;
}

static int bcop_is_not_hotcounting(const BCOp op)
{
	return bc_isiloop(op) || op == BC_IFUNCF;
}

static void bc_disable_hotcount(BCIns *ins)
{
	const BCOp op = bc_op(*ins);
	if (bcop_is_hotcounting(op))
		setbc_op(ins, (int)op + BC_OFFSET_DISABLE_HOTCOUNT);
}

static void bc_enable_hotcount(BCIns *ins)
{
	const BCOp op = bc_op(*ins);
	if (bcop_is_not_hotcounting(op))
		setbc_op(ins, (int)op + BC_OFFSET_ENABLE_HOTCOUNT);
}

void uj_proto_count_closure(GCproto *pt)
{
	uint32_t count = (uint32_t)pt->flags + PROTO_CLCOUNT;
	pt->flags =
		(uint8_t)(count - ((count >> PROTO_CLC_BITS) & PROTO_CLCOUNT));
}

void uj_proto_free(global_State *g, GCproto *pt)
{
	uj_mem_free(MEM_G(g), pt, uj_proto_sizeof(pt));
}

int uj_proto_accesses_globals(const GCproto *pt)
{
	const BCIns *bc = proto_bc(pt);
	BCPos i;

	for (i = 0; i < pt->sizebc; i++)
		if (bcop_accesses_globals(bc_op(bc[i])))
			return 1;

	return 0;
}

void uj_proto_blacklist_ins(GCproto *pt, BCIns *ins)
{
	lua_assert(bcop_is_hotcounting(bc_op(*ins)));
	lua_assert(proto_bc(pt) <= ins &&
		   ins <= proto_bc(pt) + (ptrdiff_t)(pt->sizebc - 1));

	pt->flags |= PROTO_ILOOP;
	setbc_op(ins, (int)bc_op(*ins) + BC_OFFSET_DISABLE_HOTCOUNT);
}

void uj_proto_disable_jit(GCproto *pt)
{
	BCIns *bc = proto_bc(pt);
	BCPos i;

	pt->flags |= PROTO_NOJIT;
	pt->flags |= PROTO_ILOOP;

	for (i = 0; i < pt->sizebc; i++)
		bc_disable_hotcount(&bc[i]);
}

void uj_proto_enable_jit(GCproto *pt)
{
	BCIns *bc = proto_bc(pt);
	BCPos i;

	/* JIT cannot be (re)enabled on sealed prototypes. */
	if (LJ_UNLIKELY(uj_obj_is_sealed(obj2gco(pt))))
		return;

	/* Prototype has no patched bytecodes? Nothing to do here. */
	if (!(pt->flags & PROTO_ILOOP))
		return;

	pt->flags &= ~PROTO_NOJIT;
	pt->flags &= ~PROTO_ILOOP;

	for (i = 0; i < pt->sizebc; i++)
		bc_enable_hotcount(&bc[i]);
}

static void proto_mark_chunkname(lua_State *L, GCproto *pt,
				 gco_mark_flipper marker)
{
	marker(L, obj2gco(pt->chunkname));
}

static void proto_mark_constants(lua_State *L, GCproto *pt,
				 gco_mark_flipper marker)
{
	ptrdiff_t i;
	for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++)
		marker(L, proto_kgc(pt, i));
}

void uj_proto_seal_traverse(lua_State *L, GCproto *pt, gco_mark_flipper marker)
{
	/* Mark collectable members of GCproto. */
	proto_mark_chunkname(L, pt, marker);

	/* Mark collectable constants of the prototype. */
	proto_mark_constants(L, pt, marker);

#if LJ_HASJIT
	/* NYI: compilable sealed prototypes. */
	uj_proto_disable_jit(pt);
#endif
}

static LJ_AINLINE void *proto_fixup_field(GCproto *dst, const GCproto *src,
					  const void *field)
{
	ptrdiff_t offset = (char *)field - (char *)src;
	return (void *)((char *)dst + offset);
}

GCproto *uj_proto_deepcopy(lua_State *L, const GCproto *src,
			   struct deepcopy_ctx *ctx)
{
	GCproto *dst;
	GCobj *next;
	ptrdiff_t i;

	lua_assert(src->sizeuv == 0);
	lua_assert(!uj_proto_accesses_globals(src));

	dst = uj_obj_new(L, uj_proto_sizeof(src));
	next = gcnext(obj2gco(dst));
	memcpy(dst, src, uj_proto_sizeof(src));
	obj2gco(dst)->gch.nextgc = next; /* Restore GCobj chain */
	newwhite(G(L), obj2gco(dst)); /* Make it current white again */
	dst->gclist = NULL; /* For safety */
	dst->chunkname = uj_str_copy(L, src->chunkname);

	dst->k = proto_fixup_field(dst, src, src->k);
	for (i = -(ptrdiff_t)src->sizekgc; i < 0; i++) {
		GCobj **pobj = (GCobj **)(dst->k) + i;
		*pobj = uj_obj_deepcopy(L, proto_kgc(src, i), ctx);
	}

	dst->uv = proto_fixup_field(dst, src, src->uv);
	dst->lineinfo = proto_fixup_field(dst, src, src->lineinfo);
	dst->uvinfo = proto_fixup_field(dst, src, src->uvinfo);
	dst->varinfo = proto_fixup_field(dst, src, src->varinfo);

	uj_obj_immutable_set_mark(obj2gco(dst));

	return dst;
}

/* -- Line numbers -------------------------------------------------------- */

BCLine uj_proto_line(const GCproto *pt, BCPos pos)
{
	const void *lineinfo = proto_lineinfo(pt);
	BCLine first;

	if (lineinfo == NULL || pos > pt->sizebc)
		return 0;

	first = pt->firstline;

	if (pos == pt->sizebc)
		return first + pt->numline;

	if (pos == 0)
		return first;

	pos--;

	if (pt->numline <= UINT8_MAX)
		return first + (BCLine)((const uint8_t *)lineinfo)[pos];

	if (pt->numline <= UINT16_MAX)
		return first + (BCLine)((const uint16_t *)lineinfo)[pos];

	return first + (BCLine)((const uint32_t *)lineinfo)[pos];
}

/* -- Variable names ------------------------------------------------------ */

/* Read ULEB128 value. */
static uint32_t proto_read_uleb128(const uint8_t **pp)
{
	uint64_t value;

	*pp += read_uleb128(&value, *pp);
	return (uint32_t)value;
}

const char *uj_proto_varname(const GCproto *pt, BCPos pos, BCReg slot)
{
	const uint8_t *p = proto_varinfo(pt);
	BCPos lastpos;

	if (p == NULL)
		return NULL;

	lastpos = 0;
	for (;;) {
		const char *name = (const char *)p;
		uint32_t vn = *p++;
		BCPos startpos, endpos;

		if (vn < VARNAME__MAX) {
			if (vn == VARNAME_END)
				break; /* End of varinfo. */
		} else {
			while (*p++) /* Skip over variable name string. */
				;
		}
		lastpos = startpos = lastpos + proto_read_uleb128(&p);

		if (startpos > pos)
			break;

		endpos = startpos + proto_read_uleb128(&p);

		if (pos >= endpos || slot-- != 0)
			continue;

		if (vn < VARNAME__MAX) {
#define VARNAMESTR(name, str) str "\0"
			name = VARNAMEDEF(VARNAMESTR);
#undef VARNAMESTR
			if (--vn)
				while (*name++ || --vn)
					;
		}
		return name;
	}

	return NULL;
}

/* -- Upvalues ------------------------------------------------------------ */

const char *uj_proto_uvname(const GCproto *pt, uint32_t idx)
{
	const uint8_t *p = proto_uvinfo(pt);

	lua_assert(idx < pt->sizeuv);

	if (p == NULL)
		return "";

	if (idx > 0)
		while (*p++ || --idx)
			;

	return (const char *)p;
}

/* -- Pretty-printing things ---------------------------------------------- */

#define MAX_CHUNK_NAME_LEN 40

struct pretty_loc /* Location info for pretty-printing */
{
	const char *fmt; /* format string */
	const char *name; /* chunk name */
	BCLine line; /* chunk line */
};

static struct pretty_loc proto_pretty_loc(const GCproto *pt, BCPos pos)
{
	const char *name = strdata(proto_chunkname(pt));
	size_t len = proto_chunkname(pt)->len;

	struct pretty_loc loc = {
		.fmt = NULL, .name = NULL, .line = uj_proto_line(pt, pos)};

	if (*name == '@') {
		/* Full chunk names: skip the reserved prefix */
		name++;
		len--;

		if (len <= MAX_CHUNK_NAME_LEN) {
			loc.fmt = "%s:%d";
			loc.name = name;
		} else {
			loc.fmt = "~%s:%d";
			loc.name = name + len - (MAX_CHUNK_NAME_LEN - 1);
		}
	} else if (*name == '=') {
		/*
		 * File name in case LUAJIT_DISABLE_DEBUGINFO was defined
		 * (currently an empty string).
		 */
		loc.fmt = "%.40s:%d";
		loc.name = name;
	} else if (len <= MAX_CHUNK_NAME_LEN) {
		/* Short in-memory string representing a chunk */
		loc.fmt = "\"%s\":%d";
		loc.name = name;
	} else {
		/* Long in-memory string representing a chunk */
		loc.fmt = "%p:%d";
		loc.name = (const char *)pt;
	}

	lua_assert(loc.fmt != NULL);
	lua_assert(loc.name != NULL);

	return loc;
}

void uj_proto_sprintloc(char *loc_buf, const GCproto *pt, BCPos pos)
{
	const struct pretty_loc loc = proto_pretty_loc(pt, pos);

	sprintf(loc_buf, loc.fmt, loc.name, loc.line);
}

void uj_proto_fprintloc(FILE *out, const GCproto *pt, BCPos pos)
{
	const struct pretty_loc loc = proto_pretty_loc(pt, pos);

	fprintf(out, loc.fmt, loc.name, loc.line);
}

#define TRUNC_MARK "..."
#define TRUNC_MARK_LEN strlen(TRUNC_MARK)
#define CHUNK_START "[string \""
#define CHUNK_START_LEN strlen(CHUNK_START)
#define CHUNK_STOP "\"]"
#define CHUNK_STOP_LEN strlen(CHUNK_STOP)

/* Total length of all the fancy braces and the terminator: */
#define BRACES_LEN (CHUNK_START_LEN + CHUNK_STOP_LEN + 1)

/* Output [string "string"] with possible truncation. */
static void proto_shortsource(char *out, const char *src, size_t n)
{
	char *p = out;
	size_t pos;

	lua_assert(n > BRACES_LEN + TRUNC_MARK_LEN);

	for (pos = 0; pos < n - BRACES_LEN; pos++) {
		if (src[pos] == '\0')
			break;
		if (lj_char_iscntrl((unsigned char)src[pos]))
			break;
	}

	memcpy(p, CHUNK_START, CHUNK_START_LEN);
	p += CHUNK_START_LEN;

	if (src[pos] != '\0') { /* Report just a part, truncating the rest */
		if (pos > n - BRACES_LEN - TRUNC_MARK_LEN)
			pos = n - BRACES_LEN - TRUNC_MARK_LEN;
		lua_assert(pos <= strlen(src));
		memcpy(p, src, pos);
		p += pos;
		memcpy(p, TRUNC_MARK, TRUNC_MARK_LEN);
		p += TRUNC_MARK_LEN;
	} else {
		memcpy(p, src, pos);
		p += pos;
	}

	memcpy(p, CHUNK_STOP, CHUNK_STOP_LEN);
	p += CHUNK_STOP_LEN;

	*p++ = '\0';
	lua_assert((p - out) <= n);
}

void uj_proto_chunknamencpy(char *out, const GCstr *str, size_t n)
{
	UJ_STRINGOP_TRUNCATION_WARN_OFF
	/* triggered by strncpy which doesn't terminate string with nul */
	const char *src = strdata(str);

	if (*src == '=') {
		src++;
		strncpy(out, src, n);
		out[n - 1] = '\0';
	} else if (*src == '@') {
		const size_t len = str->len - 1;

		lua_assert(n > TRUNC_MARK_LEN + 1);

		src++;

		if (len < n) {
			strncpy(out, src, n);
			return;
		}

		memcpy(out, TRUNC_MARK, TRUNC_MARK_LEN);
		out += TRUNC_MARK_LEN;

		src += (len - n) + TRUNC_MARK_LEN + 1; /* + 1 ensures \0 */
		strncpy(out, src, n - TRUNC_MARK_LEN);
	} else {
		proto_shortsource(out, src, n);
	}

	UJ_STRINGOP_TRUNCATION_WARN_ON
}
