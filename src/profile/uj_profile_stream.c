/*
 * Event streaming for uJIT profiler.
 *
 * NB! Please note that all events are streamed inside a signal handler. This
 * means effectively that only async-signal-safe library functions and syscalls
 * MUST be used for streaming. Check with `man 7 signal` when in doubt.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <unistd.h>

#include "lj_def.h"
#include "lj_frame.h"
#include "uj_ff.h"
#include "uj_dispatch.h"
#include "uj_proto.h"

#include "profile/uj_profile_impl.h"
#include "profile/ujp_write.h"

#include "utils/leb128.h"

/*
 * Yep, 10Mb. Tuned in order not to bother the platform with too often flushes.
 */
#define STREAM_BUFFER_SIZE (10 * 1024 * 1024)

const char ujp_static_header[] = {
	'u', 'j', 'p', UJP_CURRENT_FORMAT_VERSION, 0x0, 0x0, 0x0,
};

#if LJ_HASJIT
static LJ_AINLINE int stream_trace_is_streamed(const global_State *g,
					       const GCtrace *t)
{
	return g->profcount == t->profcount;
}
#endif

static LJ_AINLINE int stream_proto_is_streamed(const global_State *g,
					       const GCproto *pt)
{
	return g->profcount == pt->profcount;
}

#if LJ_HASJIT
static LJ_AINLINE void stream_trace_mark_streamed(const global_State *g,
						  GCtrace *t)
{
	t->profcount = g->profcount;
}
#endif

static LJ_AINLINE void stream_proto_mark_streamed(const global_State *g,
						  GCproto *pt)
{
	pt->profcount = g->profcount;
}

/*
 * Returns number of microseconds elapsed since Epoch. Thoughtlessly assumes
 * that returned timestamp is always >0.
 */
static uint64_t stream_timestamp(void)
{
	struct timespec ts;
	uint64_t usec;

	clock_gettime(CLOCK_REALTIME, &ts);

	usec = (uint64_t)ts.tv_sec * (uint64_t)1000000;
	usec += (uint64_t)ts.tv_nsec / (uint64_t)1000;

	/* round up if necessary */
	if (ts.tv_nsec % 1000 >= 500)
		++usec;

	return usec;
}

static LJ_AINLINE int stream_close_fd(const struct profiler_state *ps)
{
	return close(ps->buf.fd);
}

static LJ_AINLINE void stream_free_buffer(struct profiler_state *ps)
{
	uj_mem_free(MEM_G(ps->g), ps->buf.buf, STREAM_BUFFER_SIZE);
	ujp_write_terminate(&ps->buf);
}

static void stream_so_list(struct profiler_state *ps)
{
	size_t i;

	ujp_write_u64(&ps->buf, ps->so->num);
	for (i = 0; i < ps->so->num; i++) {
		ujp_write_string(&ps->buf, ps->so->objects[i].path);
		ujp_write_u64(&ps->buf, ps->so->objects[i].base);
	}
}

static void stream_prologue(struct profiler_state *ps)
{
	size_t i = 0;
	size_t len = sizeof(ujp_static_header) / sizeof(ujp_static_header[0]);

	for (; i < len; i++)
		ujp_write_byte(&ps->buf, ujp_static_header[i]);
	ujp_write_u64(&ps->buf, ps->data.id);
	ujp_write_u64(&ps->buf, stream_timestamp());
	ujp_write_u64(&ps->buf, (uint64_t)ps->opt.interval);
	stream_so_list(ps);
}

static void stream_epilogue(struct profiler_state *ps)
{
	ujp_write_byte(&ps->buf, UJP_EPILOGUE_BIT);
	ujp_write_u64(&ps->buf, stream_timestamp());
	ujp_write_u64(&ps->buf, ps->data.num_samples);
	ujp_write_u64(&ps->buf, ps->data.num_overruns);
	ujp_write_flush_buffer(&ps->buf);
}

static void stream_frame_cfunc(struct profiler_state *ps, const GCfunc *fn,
			       uint8_t frame_type)
{
	lua_CFunction cf = fn->c.f;

	lua_assert(iscfunc(fn));
	ujp_write_cfunc(&ps->buf, frame_type, (uint64_t)cf);
}

static void stream_frame_ffunc(struct profiler_state *ps, const GCfunc *fn,
			       uint8_t frame_type)
{
	uint8_t ffid = fn->c.ffid;

	lua_assert(ffid > FF_C && ffid < FF__MAX);
	ujp_write_ffunc(&ps->buf, frame_type, ffid);
}

static void stream_frame_lfunc(struct profiler_state *ps, const GCfunc *fn,
			       uint8_t frame_type)
{
	const global_State *g = ps->g;
	BCLine fl;
	GCproto *pt;
	const char *name;

	lua_assert(isluafunc(fn));

	pt = funcproto(fn);
	fl = pt->firstline;

	if (LJ_UNLIKELY(fl == 0)) {
		ujp_write_main_lua(&ps->buf, frame_type);
		return;
	}

	lua_assert(pt->profcount <= g->profcount);

	/*
	 * For Lua code function protos which reside in sealed data in shared
	 * data state (i.e. shared among VM-s) profcount is shared as well.
	 * However, global profcount is one per VM. As a result for successive
	 * profile sessions for such sealed objects proto profcount may equal
	 * to global profcount even though it has not been yet streamed in this
	 * session (as proto profcount had already been streamed in another
	 * thread. For a more detailed description, see comment in
	 * tests/iponweb/unit/src/test_profiler.c.
	 */
	if (!uj_obj_is_sealed(obj2gco(pt)) && stream_proto_is_streamed(g, pt)) {
		ujp_write_marked_lfunc(&ps->buf, frame_type, (uint64_t)pt);
		return;
	}

	name = proto_chunknamestr(pt);
	ujp_write_new_lfunc(&ps->buf, frame_type, (uint64_t)pt, name, fl);
	stream_proto_mark_streamed(g, pt);
}

static void stream_frame_info(struct profiler_state *ps, const TValue *frame,
			      uint8_t frame_type)
{
	const GCfunc *fn = frame_func(frame);

	if (isluafunc(fn))
		stream_frame_lfunc(ps, fn, frame_type);
	else if (iscfunc(fn))
		stream_frame_cfunc(ps, fn, frame_type);
	else if (isffunc(fn))
		stream_frame_ffunc(ps, fn, frame_type);
	else
		lua_assert(0);
}

static void stream_stack(struct profiler_state *ps, const TValue *frame)
{
	uint8_t frame_type = UJP_FRAME_TOP;

	while (!frame_isbottom(frame)) {
		stream_frame_info(ps, frame, frame_type);

		if (LJ_UNLIKELY(frame_type == UJP_FRAME_TOP)) {
			if (ps->opt.mode == PROFILE_LEAF)
				break;
			frame_type = UJP_FRAME_MIDDLE;
		}

		frame = frame_prev(frame);
	}

	/* Write the last pseudo-frame */
	ujp_write_bottom_frame(&ps->buf);
}

/* Serializes frame information for the LFUNC state. */
static void stream_lfunc(struct profiler_state *ps)
{
	/*
	 * We expect a frame slot below the base. It means that a slot should
	 * contain a frame which values should not overlap with valid tag
	 * values. Yet there is a very-very little possibility of such
	 * overlapping. If this comes true, feel free to remove the assert
	 * below. For details, see TValue layout.
	 */
	const TValue *frame = ps->g->top_frame.guesttop.interp_base - 1;

	lua_assert(!tagisvalid(frame));
	stream_stack(ps, frame);
}

/* Serializes frame information for the FFUNC state. */
static void stream_ffunc(struct profiler_state *ps)
{
	uint8_t frame_type = UJP_FRAME_FOR_LEAF_PROFILE;
	uint8_t ffid = ps->g->top_frame.ffid;

	lua_assert(ffid > FF_C && ffid < FF__MAX);
	ujp_write_ffunc(&ps->buf, frame_type, ffid);
}

/* Serializes frame information for the CFUNC state. */
static void stream_cfunc(struct profiler_state *ps)
{
	const TValue *frame = ps->g->top_frame.guesttop.interp_base - 1;

	stream_stack(ps, frame);
}

static void stream_hvmstate(struct profiler_state *ps)
{
	ujp_write_hvmstate(&ps->buf, ps->context.rip);
}

/* Serializes frame information for the TRACE state. */
static void stream_trace(struct profiler_state *ps)
{
#if LJ_HASJIT
	uint8_t frame_type = UJP_FRAME_FOR_LEAF_PROFILE;
	const global_State *g = ps->g;
	uint32_t traceno = g->vmstate;
	uint64_t generation = curgeneration(G2J(g));
	GCtrace *t = G2J(g)->trace[traceno];
	const GCproto *pt;
	const char *name;
	uint64_t firstline;

	lua_assert(t->profcount <= g->profcount);

	if (stream_trace_is_streamed(g, t)) {
		ujp_write_marked_trace(&ps->buf, frame_type, generation,
				       traceno);
		return;
	}

	pt = t->startpt;
	name = proto_chunknamestr(pt);
	firstline = (uint64_t)uj_proto_line(pt, proto_bcpos(pt, t->startpc));

	ujp_write_new_trace(&ps->buf, frame_type, generation, traceno, name,
			    firstline);

	stream_trace_mark_streamed(g, t);
#else
	UNUSED(ps);
#endif /* LJ_HASJIT */
}

/* ------------------- Implementation of public module API ------------------ */

static LJ_AINLINE int stream_is_needed(const struct profiler_state *ps)
{
	return ps->opt.mode == PROFILE_LEAF ||
	       ps->opt.mode == PROFILE_CALLGRAPH;
}

int uj_profile_stream_start(struct profiler_state *ps, lua_State *L, int fd)
{
	/*
	 * NB! Whenever an error occurs, we free resources without caring about
	 * possible nested errors and return the first encountered error.
	 */
	uint8_t *buf;

	if (!stream_is_needed(ps))
		return LUAE_PROFILE_SUCCESS;

	if (fd < 0)
		return LUAE_PROFILE_ERRIO;

	buf = (uint8_t *)uj_mem_alloc(L, STREAM_BUFFER_SIZE);
	if (LJ_UNLIKELY(buf == NULL)) {
		stream_close_fd(ps);
		return LUAE_PROFILE_ERRMEM;
	}

	ujp_write_init(&ps->buf, fd, buf, STREAM_BUFFER_SIZE);
	stream_prologue(ps);

	if (LJ_UNLIKELY(ujp_write_test_flag(&ps->buf, STREAM_ERR_IO))) {
		stream_close_fd(ps);
		stream_free_buffer(ps);
		return LUAE_PROFILE_ERRIO;
	}

	return LUAE_PROFILE_SUCCESS;
}

typedef void (*event_handler_func)(struct profiler_state *);

static const event_handler_func write_event_handler[] = {
	stream_lfunc, /* UJ_VMST_LFUNC */
	stream_ffunc, /* UJ_VMST_FFUNC */
	stream_cfunc, /* UJ_VMST_CFUNC */
	stream_hvmstate, /* UJ_VMST_IDLE */
	stream_hvmstate, /* UJ_VMST_INTERP */
	stream_hvmstate, /* UJ_VMST_GC */
	stream_hvmstate, /* UJ_VMST_EXIT */
	stream_hvmstate, /* UJ_VMST_RECORD */
	stream_hvmstate, /* UJ_VMST_OPT */
	stream_hvmstate, /* UJ_VMST_ASM */
	stream_trace /* UJ_VMST_TRACE */
};

void uj_profile_stream_event(struct profiler_state *ps, uint32_t vmstate)
{
	event_handler_func handler;

	if (!stream_is_needed(ps))
		return;

	/* Write event header: */
	/* Should occupy maximum 4 bits.*/
	lua_assert((vmstate & ~(uint32_t)0xf) == 0);
	ujp_write_byte(&ps->buf, (uint8_t)vmstate);

	/* Write event body (depending on VM state): */

	handler = write_event_handler[vmstate];
	lua_assert(handler != NULL);
	handler(ps);
}

int uj_profile_stream_stop(struct profiler_state *ps)
{
	struct ujp_buffer *buf = &ps->buf;
	int status = LUAE_PROFILE_SUCCESS;
	int status_fd;

	if (!stream_is_needed(ps))
		return status;

	stream_epilogue(ps);
	status_fd = stream_close_fd(ps);

	if (ujp_write_test_flag(buf, STREAM_ERR_IO) || status_fd != 0)
		status = LUAE_PROFILE_ERRIO;

	stream_free_buffer(ps);
	return status;
}
