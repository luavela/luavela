/*
 * Low-level event streaming for uJIT Profiler.
 * NB! Please note that all events are streamed inside a signal handler. This
 * means effectively that only async-signal-safe library functions and syscalls
 * MUST be used for streaming. Check with `man 7 signal` when in doubt.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJP_WRITE_H
#define _UJP_WRITE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Event stream format:
 *
 * stream       := prologue event* epilogue
 * prologue     := 'u' 'j' 'p' version reserved prof-id start-time interval \
		   so-num so-objects
 * so-num       := <ULEB128>
 * so-objects   := so-info*
 * so-info      := so-path so-base
 * so-path      := string
 * so-base      := <ULEB128>
 * version      := <BYTE>
 * reserved     := <BYTE> <BYTE> <BYTE>
 * prof-id      := <ULEB128>
 * start-time   := <ULEB128>
 * interval     := <ULEB128>
 * event        := event-header frame*
 * event-header := <BYTE>
 * frame        := frame-header frame-info
 * frame-header := <BYTE>
 * frame-info   := frame-id frame-sym? | frame-id trace-gen sym-trace?
 * frame-id     := <ULEB128>
 * frame-sym    := sym-lua | sym-ff
 * sym-lua      := chunk-name loc-line
 * sym-ff       := ff-name
 * trace-gen    := <ULEB128>
 * sym-trace    := chunk-name start-line
 * chunk-name   := string
 * ff-name      := string
 * loc-line     := <ULEB128>
 * trace-num    := <ULEB128>
 * start-line   := <ULEB128>
 * epilogue     := event-header end-time num-samples num-overruns
 * end-time     := <ULEB128>
 * num-samples  := <ULEB128>
 * num-overruns := <ULEB128>
 *
 * string         := string-len string-payload
 * string-len     := <ULEB128>
 * string-payload := <BYTE> {string-len}
 *
 * Notes.
 * 1) For strings shorter than 128 bytes (most likely scenario in our case)
 * we write the same amount of data (1-byte ULEB128 + actual payload) as we
 * would have written with straightforward serialization (actual payload + \0),
 * but make parsing easier.
 * 2) start-time and end-time must resolve to microseconds.
 *
 * <BYTE>   :  A single byte (no surprises here)
 * <ULEB128>:  Unsigned integer represented in ULEB128 encoding
 *
 * (Order of bits below is hi -> lo)
 *
 * version: [VVVVVVVV]
 *  * VVVVVVVV: Byte interpreted as a plain numeric version number
 *
 * event-header: [FUUUSSSS]
 *  * SSSS: 4 bits for representing VM state
 *  * UUU : 3 unused bits
 *  * F   : 0 for regular profiler events, 1 for epilogue's *F*inal header
 *          (if F is set to 1, all other bits are currently ignored)
 *
 * Presence of frame elements inside the event elements depends on VM state
 * encoded in event-header. For now, only following VM states assume frame
 * elements: LFUNC, FFUNC, CFUNC, TRACE.
 * For other VM states only RIP value is dumped to be able to determine
 * particular shared object which has been in use at the time.
 *
 * frame-header: [UUFFFEBT]
 *  * T  : Is current frame a top frame?
 *  * B  : Is current frame a bottom frame?
 *  * E  : Is frame-sym data explicitly set after frame-id in the stream?
 *  * FFF: 3 bits for representing type of callable object
 *         (Lua function, fast function, compiled trace)
 *  * UU : 2 unused bits
 */

#define UJP_FRAME_MIDDLE 0x0
#define UJP_FRAME_TOP 0x1
#define UJP_FRAME_BOTTOM 0x2
#define UJP_FRAME_EXPLICIT_SYMBOL 0x4

/* Main scope of a chunk. */
#define UJP_FRAME_TYPE_MAIN 0x1
/* Call to a Lua function, except the main scope. */
#define UJP_FRAME_TYPE_LFUNC 0x2
/* Call to a registered C function. */
#define UJP_FRAME_TYPE_CFUNC 0x3
/* Call to a fast function. */
#define UJP_FRAME_TYPE_FFUNC 0x4
/* JIT-compiled trace. */
#define UJP_FRAME_TYPE_TRACE 0x5
/*
 * In case VM is in a state when no per-frame data is required for reporting
 * there should be some generic pseudo frame type to stand for such states
 * in profiler binary stream.
 */
#define UJP_FRAME_TYPE_HOST 0x6

/*
 * frame-id is required according to the format, the constant below is used as
 * a dummy frame ID for the main chunk frame. Rationale:
 *  1. Making frame-id optional would complicate parsing a bit.
 *  2. In real world not that much code is run in the context of the main frame,
 *     so writing an extra byte would not add much size overhead.
 *
 */
#define FRAME_ID_MAIN_LUA 0x0
#define UJP_CURRENT_FORMAT_VERSION 0x04
#define UJP_EPILOGUE_BIT 0x80

/*
 * Although event stream format allows streaming full stack traces, current
 * implementation streams top-most frame only, so each pseudo-stack trace
 * always consists of a single frame which is both top and bottom. Below is
 * defined for convenience.
 */
#define UJP_FRAME_FOR_LEAF_PROFILE (UJP_FRAME_TOP | UJP_FRAME_BOTTOM)

/* Stream errors */
#define STREAM_ERR_IO 0x1

/* Write buffer for profiler */
struct ujp_buffer {
	int fd; /* File descriptor to dump profiling events to */
	uint8_t *buf;
	uint8_t *pos;
	size_t size;
	volatile uint8_t flags; /* Internal flags. */
};

/* Write string */
void ujp_write_string(struct ujp_buffer *buf, const char *s);
/* Write single byte */
void ujp_write_byte(struct ujp_buffer *buf, uint8_t b);
/* Write uint64_t in uleb128 format */
void ujp_write_u64(struct ujp_buffer *buf, uint64_t n);
/* Immediatly flush buffer to disk */
void ujp_write_flush_buffer(struct ujp_buffer *buf);
/* Write new trace */
void ujp_write_new_trace(struct ujp_buffer *buf, uint8_t frame_type,
			 uint64_t generation, uint64_t traceno,
			 const char *name, uint64_t line);
/* Write already marked trace */
void ujp_write_marked_trace(struct ujp_buffer *buf, uint8_t frame_type,
			    uint64_t generation, uint64_t traceno);
/* Write single FFUNC frame */
void ujp_write_ffunc(struct ujp_buffer *buf, uint8_t frame_type, uint64_t ffid);
/* Write single CFUNC frame */
void ujp_write_cfunc(struct ujp_buffer *buf, uint8_t frmae_type, uint64_t cf);
/* Write stub frame (MAIN_LUA) */
void ujp_write_main_lua(struct ujp_buffer *buf, uint8_t frame_type);
/* Bottom frame for callgraph */
void ujp_write_bottom_frame(struct ujp_buffer *buf);
/* Write new LFUNC */
void ujp_write_new_lfunc(struct ujp_buffer *buf, uint8_t frame_type,
			 uint64_t pt, const char *name, uint64_t fl);
/* Already marked LFUNC */
void ujp_write_marked_lfunc(struct ujp_buffer *buf, uint8_t frame_type,
			    uint64_t pt);
/*
 * For one of the following VM states:
 * C, GC, INTERP, RECORD, EXIT, OPT, ASM write RIP value as profiling data.
 */
void ujp_write_hvmstate(struct ujp_buffer *buf, uint64_t addr);
/* Init buffer */
void ujp_write_init(struct ujp_buffer *buf, int fd, uint8_t *mem, size_t size);
/* Check flags */
int ujp_write_test_flag(const struct ujp_buffer *buf, uint8_t flag);
/* Set pointers to NULL and reset flags */
void ujp_write_terminate(struct ujp_buffer *buf);

#endif /* !_UJP_WRITE_H */
