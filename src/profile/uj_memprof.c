/*
 * Implementation of memory profiler.
 *
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "profile/uj_memprof_iface.h"
#include "lextlib.h"
#include "lj_def.h"

#ifdef UJIT_MEMPROF

#include <unistd.h>
#ifdef UJIT_IS_THREAD_SAFE
#include <pthread.h>
#endif /* UJIT_IS_THREAD_SAFE */

#include "lua.h"

#include "lj_obj.h"
#include "lj_frame.h"
#include "lj_debug.h"
#include "uj_vmstate.h"
#include "uj_timerint.h"
#include "profile/uj_symtab.h"
#include "profile/ujp_write.h"

/* Allocation events: */
#define AEVENT_ALLOC ((uint8_t)1)
#define AEVENT_FREE ((uint8_t)2)
#define AEVENT_REALLOC ((uint8_t)(AEVENT_ALLOC | AEVENT_FREE))

/* Allocation sources: */
#define ASOURCE_INT ((uint8_t)(1 << 2))
#define ASOURCE_LFUNC ((uint8_t)(2 << 2))
#define ASOURCE_CFUNC ((uint8_t)(3 << 2))

/* Aux bits: */

/*
 * There is ~1 second between each two events marked with this flag. This will
 * possibly be used later to implement dumps of the evolving heap.
 */
#define UJM_TIMESTAMP ((uint8_t)(0x40))

#define UJM_EPILOGUE_HEADER 0x80

/*
 * Yep, 10Mb. Tuned in order not to bother the platform with too often flushes.
 */
#define STREAM_BUFFER_SIZE (10 * 1024 * 1024)

enum memprof_state {
	MPS_IDLE, /* memprof not running */
	MPS_PROFILE /* memprof running */
};

struct alloc {
	lua_Alloc allocf; /* Allocating function. */
	void *state; /* Opaque allocator's state. */
};

struct memprof {
	global_State *g; /* Profiled VM. */
	uint64_t nticks; /* Ticks at the latest timestamp event. */
	uint64_t expticks; /* Ticks to expire at. */
	enum memprof_state state; /* Internal state. */
	struct ujp_buffer out; /* Output accumulator. */
	struct alloc orig_alloc; /* Original allocator. */
	struct memprof_options opt; /* Profiling options. */
};

#ifdef UJIT_IS_THREAD_SAFE

pthread_mutex_t memprof_mutex = PTHREAD_MUTEX_INITIALIZER;

static LJ_AINLINE void memprof_lock(void)
{
	pthread_mutex_lock(&memprof_mutex);
}

static LJ_AINLINE void memprof_unlock(void)
{
	pthread_mutex_unlock(&memprof_mutex);
}

#else /* UJIT_IS_THREAD_SAFE */

#define memprof_lock()
#define memprof_unlock()

#endif /* UJIT_IS_THREAD_SAFE */

/*
 * Event stream format:
 *
 * stream         := symtab memprof
 * symtab         := <see uj_symtab.h>
 * memprof        := prologue event* epilogue
 * prologue       := 'u' 'j' 'm' version reserved
 * version        := <BYTE>
 * reserved       := <BYTE> <BYTE> <BYTE>
 * prof-id        := <ULEB128>
 * event          := event-alloc | event-realloc | event-free
 * event-alloc    := event-header loc? naddr nsize
 * event-realloc  := event-header loc? oaddr osize naddr nsize
 * event-free     := event-header loc? oaddr osize
 * event-header   := <BYTE>
 * loc            := loc-lua | loc-c
 * loc-lua        := sym-addr line-no
 * loc-c          := sym-addr
 * sym-addr       := <ULEB128>
 * line-no        := <ULEB128>
 * oaddr          := <ULEB128>
 * naddr          := <ULEB128>
 * osize          := <ULEB128>
 * nsize          := <ULEB128>
 * epilogue       := event-header
 *
 * <BYTE>   :  A single byte (no surprises here)
 * <ULEB128>:  Unsigned integer represented in ULEB128 encoding
 *
 * (Order of bits below is hi -> lo)
 *
 * version: [VVVVVVVV]
 *  * VVVVVVVV: Byte interpreted as a plain integer version number
 *
 * event-header: [FTUUSSEE]
 *  * EE   : 2 bits for representing allocation event type (AEVENT_*)
 *  * SS   : 2 bits for representing allocation source type (ASOURCE_*)
 *  * UU   : 2 unused bits
 *  * T    : 0 for regular events, 1 for the events marked with the timestamp
 *           mark. It is assumed that the time distance between two marked
 *           events is approximately the same and is equal to 1 second.
 *  * F    : 0 for regular events, 1 for epilogue's *F*inal header
 *           (if F is set to 1, all other bits are currently ignored)
 */

static struct memprof memprof = {0};

const char ujm_header[] = {'u', 'j', 'm', UJM_CURRENT_FORMAT_VERSION,
			   0x0, 0x0, 0x0};

/* Converts sec seconds to the number of ticks. */
static LJ_AINLINE uint64_t memprof_toticks(uint64_t sec)
{
	return uj_timerint_to_ticks(sec * 1000000);
}

static void memprof_write_lfunc(struct ujp_buffer *out, uint8_t header,
				GCfunc *fn, struct lua_State *L,
				const TValue *nextframe)
{
	const BCLine line = lj_debug_frameline(L, fn, nextframe);
	ujp_write_byte(out, header | ASOURCE_LFUNC);
	ujp_write_u64(out, (uint64_t)funcproto(fn));
	ujp_write_u64(out, line >= 0 ? (uint64_t)line : 0);
}

static void memprof_write_cfunc(struct ujp_buffer *out, uint8_t header,
				const GCfunc *fn)
{
	ujp_write_byte(out, header | ASOURCE_CFUNC);
	ujp_write_u64(out, (uint64_t)fn->c.f);
}

static void memprof_write_ffunc(struct ujp_buffer *out, uint8_t header,
				GCfunc *fn, struct lua_State *L,
				const TValue *frame)
{
	const TValue *pframe = frame_prev(frame);
	GCfunc *pfn = !frame_isbottom(pframe) ? frame_func(pframe) : NULL;

	/*
	 * NB! If a fast function is called by a Lua function, report the
	 * Lua function for more meaningful output. Otherwise report the fast
	 * function as a C function.
	 */
	if (pfn != NULL && isluafunc(pfn))
		memprof_write_lfunc(out, header, pfn, L, frame);
	else
		memprof_write_cfunc(out, header, fn);
}

static void memprof_write_func(struct memprof *mp, uint8_t header)
{
	struct ujp_buffer *out = &mp->out;
	lua_State *L = mp->g->L_mem;
	const TValue *frame = L->base - 1;
	GCfunc *fn;

	lua_assert(!frame_isbottom(frame));

	fn = frame_func(frame);

	if (isluafunc(fn))
		memprof_write_lfunc(out, header, fn, L, NULL);
	else if (isffunc(fn))
		memprof_write_ffunc(out, header, fn, L, frame);
	else if (iscfunc(fn))
		memprof_write_cfunc(out, header, fn);
	else
		lua_assert(0);
}

static void memprof_write_hvmstate(struct memprof *mp, uint8_t header)
{
	ujp_write_byte(&mp->out, header | ASOURCE_INT);
}

/*
 * NB! In ideal world, we should report allocations from traces as well.
 * But since traces must follow the semantics of the original code, behaviour of
 * Lua and JITted code must match 1:1 in terms of allocations, which makes
 * using memprof with enabled JIT virtually redundant. Hence the stub below.
 */
static void memprof_write_trace(struct memprof *mp, uint8_t header)
{
	ujp_write_byte(&mp->out, header | ASOURCE_INT);
}

typedef void (*memprof_writer)(struct memprof *mp, uint8_t header);

static const memprof_writer memprof_writers[] = {
	memprof_write_func, /* UJ_VMST_LFUNC */
	memprof_write_func, /* UJ_VMST_FFUNC */
	memprof_write_func, /* UJ_VMST_CFUNC */
	memprof_write_hvmstate, /* UJ_VMST_IDLE */
	memprof_write_hvmstate, /* UJ_VMST_INTERP */
	memprof_write_hvmstate, /* UJ_VMST_GC */
	memprof_write_hvmstate, /* UJ_VMST_EXIT */
	memprof_write_hvmstate, /* UJ_VMST_RECORD */
	memprof_write_hvmstate, /* UJ_VMST_OPT */
	memprof_write_hvmstate, /* UJ_VMST_ASM */
	memprof_write_trace /* UJ_VMST_TRACE */
};

static void memprof_write_caller(struct memprof *mp, uint8_t aevent,
				 uint64_t nticks)
{
	global_State *g = mp->g;
	uint32_t _vmstate = (uint32_t)uj_vmstate_get(&g->vmstate);
	uint32_t vmstate = _vmstate < UJ_VMST_TRACE ? _vmstate : UJ_VMST_TRACE;
	uint8_t header = aevent;

	lua_assert(nticks >= mp->nticks);

	if ((nticks - mp->nticks) >= memprof_toticks(1)) {
		header |= UJM_TIMESTAMP;
		mp->nticks = nticks;
	}

	memprof_writers[vmstate](mp, header);
}

static int memprof_stop(const struct global_State *g);

static void *memprof_allocf(void *ud, void *ptr, size_t osize, size_t nsize)
{
	struct memprof *mp = &memprof;
	struct alloc *oalloc = &mp->orig_alloc;
	struct ujp_buffer *out = &mp->out;
	uint64_t nticks = uj_timerint_ticks();
	void *nptr;

	lua_assert(MPS_PROFILE == mp->state);
	lua_assert(oalloc->allocf != memprof_allocf);
	lua_assert(oalloc->allocf != NULL);
	lua_assert(ud == oalloc->state);
	lua_assert(uj_timerint_is_ticking());

	nptr = oalloc->allocf(ud, ptr, osize, nsize);

	if (0 == nsize) {
		memprof_write_caller(mp, AEVENT_FREE, nticks);
		ujp_write_u64(out, (uint64_t)ptr);
		ujp_write_u64(out, (uint64_t)osize);
	} else if (NULL == ptr) {
		memprof_write_caller(mp, AEVENT_ALLOC, nticks);
		ujp_write_u64(out, (uint64_t)nptr);
		ujp_write_u64(out, (uint64_t)nsize);
	} else {
		memprof_write_caller(mp, AEVENT_REALLOC, nticks);
		ujp_write_u64(out, (uint64_t)ptr);
		ujp_write_u64(out, (uint64_t)osize);
		ujp_write_u64(out, (uint64_t)nptr);
		ujp_write_u64(out, (uint64_t)nsize);
	}

	if (mp->expticks != MEMPROF_DURATION_INFINITE && nticks >= mp->expticks)
		memprof_stop(NULL);

	return nptr;
}

static void memprof_write_prologue(struct ujp_buffer *out)
{
	size_t i = 0;
	const size_t len = sizeof(ujm_header) / sizeof(ujm_header[0]);

	for (; i < len; i++)
		ujp_write_byte(out, ujm_header[i]);
}

int uj_memprof_start(struct lua_State *L, const struct memprof_options *opt)
{
	struct memprof *mp = &memprof;
	struct alloc *oalloc = &mp->orig_alloc;
	uint8_t *buf;

	memprof_lock();

	if (uj_timerint_init_default() != LUAE_INT_SUCCESS) {
		memprof_unlock();
		return LUAE_PROFILE_ERR;
	}

	if (mp->state != MPS_IDLE) {
		memprof_unlock();
		return LUAE_PROFILE_ERR;
	}

	buf = (uint8_t *)uj_mem_alloc(L, STREAM_BUFFER_SIZE);
	if (NULL == buf) {
		memprof_unlock();
		return LUAE_PROFILE_ERRMEM;
	}

	/* Init options: */
	memcpy(&mp->opt, opt, sizeof(*opt));

	/* Init general fields: */
	mp->g = G(L);
	mp->state = MPS_PROFILE;
	mp->nticks = uj_timerint_ticks();
	if (mp->opt.dursec != MEMPROF_DURATION_INFINITE)
		mp->expticks = mp->nticks + memprof_toticks(mp->opt.dursec);
	else
		mp->expticks = MEMPROF_DURATION_INFINITE;

	/* Init output: */
	ujp_write_init(&mp->out, mp->opt.fd, buf, STREAM_BUFFER_SIZE);
	uj_symtab_write(&mp->out, mp->g);
	memprof_write_prologue(&mp->out);

	/* Override allocating function: */
	oalloc->allocf = lua_getallocf(L, &oalloc->state);
	lua_assert(oalloc->allocf != NULL);
	lua_assert(oalloc->allocf != memprof_allocf);
	lua_assert(oalloc->state != NULL);
	lua_setallocf(L, memprof_allocf, oalloc->state);

	memprof_unlock();
	return LUAE_PROFILE_SUCCESS;
}

static int memprof_stop(const struct global_State *g)
{
	struct memprof *mp = &memprof;
	struct alloc *oalloc = &mp->orig_alloc;
	struct ujp_buffer *out = &mp->out;
	struct lua_State *L;

	memprof_lock();

	if (mp->state != MPS_PROFILE) {
		memprof_unlock();
		return LUAE_PROFILE_ERR;
	}

	if (g != NULL && mp->g != g) {
		memprof_unlock();
		return LUAE_PROFILE_ERR;
	}

	mp->state = MPS_IDLE;

	lua_assert(mp->g != NULL);
	L = mainthread(mp->g);

	lua_assert(memprof_allocf == lua_getallocf(L, NULL));
	lua_assert(oalloc->allocf != NULL);
	lua_assert(oalloc->state != NULL);
	lua_setallocf(L, oalloc->allocf, oalloc->state);

	ujp_write_byte(out, UJM_EPILOGUE_HEADER);

	ujp_write_flush_buffer(out);

	close(mp->opt.fd); /* Ignore possible errors. */
	uj_mem_free(MEM(L), out->buf, STREAM_BUFFER_SIZE);
	ujp_write_terminate(out);

	memprof_unlock();
	return LUAE_PROFILE_SUCCESS;
}

int uj_memprof_stop(void)
{
	return memprof_stop(NULL);
}

int uj_memprof_stop_vm(const struct global_State *g)
{
	return memprof_stop(g);
}

#else /* UJIT_MEMPROF */

int uj_memprof_start(struct lua_State *L, const struct memprof_options *opt)
{
	UNUSED(L);
	UNUSED(opt);
	return LUA_PROFILE_ERR;
}

int uj_memprof_stop(void)
{
	return LUA_PROFILE_ERR;
}

int uj_memprof_stop_vm(const struct global_State *g)
{
	UNUSED(g);
	return LUA_PROFILE_ERR;
}

#endif /* UJIT_MEMPROF */
