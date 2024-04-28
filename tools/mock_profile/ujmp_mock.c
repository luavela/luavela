/*
 * Fake streams generator for uJIT profiler.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#define _GNU_SOURCE

#include <dlfcn.h>

#include "../../src/profile/ujp_write.h"
#include "../../src/uj_ff.h"
#include "../../src/uj_vmstate.h"

/* 'Mangled' functions, used to compel demangling. */
static void _ZN11demangle_me10if_you_canERKSsPvm(void)
{
}
static void _ZN11demangle_me22very_big_function_nameERKSsS1_S1_(void)
{
}
static void _Z___wrongmangle11(void)
{
}

/*
 * TODO: C state symbols must be resolved correctly with
 * -fPIE option.
 */
void ujmp_mock_all_vmstates(struct ujp_buffer *buf)
{
	/* Idle state. */
	ujp_write_byte(buf, UJ_VMST_IDLE);
	/* Demangling must fail and return original symbol. */
	ujp_write_hvmstate(buf, (uint64_t)&_Z___wrongmangle11);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf, (uint64_t)&ujmp_mock_all_vmstates);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf, (uint64_t)&ujp_write_string);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf,
			   (uint64_t)&_ZN11demangle_me10if_you_canERKSsPvm);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	/* Function name must be truncated to 90 symbols. */
	ujp_write_hvmstate(
		buf,
		(uint64_t)&_ZN11demangle_me22very_big_function_nameERKSsS1_S1_);

	/* INTERP */
	ujp_write_byte(buf, UJ_VMST_INTERP);
	ujp_write_hvmstate(buf, 0xaabbcc23eeff);

	/* RECORD */
	ujp_write_byte(buf, UJ_VMST_RECORD);
	ujp_write_hvmstate(buf, 0xffaabe);

	/* FFUNC */
	ujp_write_byte(buf, UJ_VMST_FFUNC);
	ujp_write_ffunc(buf, UJP_FRAME_FOR_LEAF_PROFILE, FF_table_sort);
	ujp_write_byte(buf, UJ_VMST_FFUNC);
	ujp_write_ffunc(buf, UJP_FRAME_FOR_LEAF_PROFILE, FF_pcall);

	/* GC */
	ujp_write_byte(buf, UJ_VMST_GC);
	ujp_write_hvmstate(buf, 0xdddd);

	/* LFUNC */
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xaa, "test@lua", 10);
	ujp_write_marked_lfunc(buf, UJP_FRAME_MIDDLE, 0xaa);
	ujp_write_ffunc(buf, UJP_FRAME_MIDDLE, FF_math_max);
	ujp_write_cfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeef);
	ujp_write_bottom_frame(buf);

	/* GC */
	ujp_write_byte(buf, UJ_VMST_GC);
	ujp_write_hvmstate(buf, 0xdddd);

	/* ASM */
	ujp_write_byte(buf, UJ_VMST_ASM);
	ujp_write_hvmstate(buf, 0xaabbccddeeff);

	/* OPT */
	ujp_write_byte(buf, UJ_VMST_OPT);
	ujp_write_hvmstate(buf, 0xaabbccddeeff);

	/* CFUNC */
	ujp_write_byte(buf, UJ_VMST_CFUNC);
	ujp_write_cfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeef);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_CFUNC);
	ujp_write_cfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeefdeadbeef);
	ujp_write_bottom_frame(buf);

	/* TRACE */
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1, "new_trace",
			    231);
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_marked_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1);

	/* EXIT */
	ujp_write_byte(buf, UJ_VMST_EXIT);
	ujp_write_hvmstate(buf, 0xaabbcc);
}

void ujmp_mock_no_bottom(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_CFUNC);
	ujp_write_ffunc(buf, UJP_FRAME_TOP, FF_pcall);
	ujp_write_cfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeef);
}

void ujmp_mock_main_lua(struct ujp_buffer *buf)
{
	/* Stack with MAIN_LUA */
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_main_lua(buf, UJP_FRAME_TOP);
	ujp_write_main_lua(buf, UJP_FRAME_MIDDLE);
	ujp_write_main_lua(buf, UJP_FRAME_MIDDLE);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_wrong_ffunc(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_FFUNC);
	ujp_write_ffunc(buf, UJP_FRAME_FOR_LEAF_PROFILE, FF__MAX + 1);
}

void ujmp_mock_marked_lfunc(struct ujp_buffer *buf)
{
	/* Add new lfunc */
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	/* New LFUNC */
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xaa, "test@lua", 10);
	/* Already streamed LFUNC */
	ujp_write_marked_lfunc(buf, UJP_FRAME_MIDDLE, 0xaa);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_hvmstates(struct ujp_buffer *buf)
{
	size_t hvmst;
	for (hvmst = UJ_VMST_HVMST_START; hvmst < UJ_VMST__MAX; hvmst++) {
		ujp_write_byte(buf, hvmst);
		ujp_write_hvmstate(buf, 0xaadd012cce);
	}
}

/* There are no cached symbol for marked lfunc. */
void ujmp_mock_lfunc_miscached(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_marked_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_lfunc_diffnames(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef, "FirstFunc", 100);
	ujp_write_new_lfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeef, "SecondFunc",
			    100);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_lfunc_difflines(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef, "test@lua", 10);
	ujp_write_new_lfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeef, "test@lua", 101);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_trace_miscached(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_marked_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1);
}

void ujmp_mock_trace_diffnames(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1, "new_trace",
			    231);
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_TYPE_TRACE, 1, 1,
			    "new_trace_new_name", 231);
}

void ujmp_mock_trace_difflines(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1, "new_trace",
			    231);
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1, "new_trace",
			    178);
}

void ujmp_mock_duplicates(struct ujp_buffer *buf)
{
	/* Duplicated Lua functions. */
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef, "test@lua", 10);
	ujp_write_new_lfunc(buf, UJP_FRAME_MIDDLE, 0xdeadbeef, "test@lua", 10);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef, "test@lua", 10);
	ujp_write_bottom_frame(buf);

	/* Duplicated traces. */
	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1, "new_trace",
			    231);

	ujp_write_byte(buf, UJ_VMST_TRACE);
	ujp_write_new_trace(buf, UJP_FRAME_FOR_LEAF_PROFILE, 1, 1, "new_trace",
			    231);
}

void ujmp_mock_lua_demangle(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef01,
			    "@chunks/demangle/funcdefs.lua", 1);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef02,
			    "@chunks/demangle/funcdefs.lua", 2);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef03,
			    "@chunks/demangle/funcdefs.lua", 3);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef04,
			    "@chunks/demangle/funcdefs.lua", 4);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef05,
			    "@chunks/demangle/funcdefs.lua", 5);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef06,
			    "@chunks/demangle/funcdefs.lua", 6);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef07,
			    "@chunks/demangle/funcdefs.lua", 7);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef0a, "__code_elem", 7);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef0b,
			    "@chunks/demangle/funcdefs.lua", 8);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef0c,
			    "@chunks/demangle/funcdefs.lua", 9);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef0d,
			    "@chunks/demangle/funcdefs.lua", 10);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef0e,
			    "@chunks/demangle/morefuncs.lua", 1);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef0f,
			    "@chunks/demangle/morefuncs.lua", 2);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef10,
			    "@chunks/demangle/morefuncs.lua", 3);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef11,
			    "@chunks/demangle/morefuncs.lua", 4);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef12,
			    "@chunks/demangle/morefuncs.lua", 9);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef13,
			    "@chunks/demangle/morefuncs.lua", 12);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef14,
			    "@chunks/demangle/morefuncs.lua", 16);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef15,
			    "@chunks/demangle/morefuncs.lua", 21);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef16,
			    "@chunks/demangle/morefuncs.lua", 26);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef17,
			    "@chunks/demangle/morefuncs.lua", 29);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef18,
			    "@chunks/demangle/morefuncs.lua", 34);
	ujp_write_bottom_frame(buf);

	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef19,
			    "@chunks/demangle/morefuncs.lua", 40);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_lua_demangle_badfile(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef00,
			    "@tools/no_such_file", 7);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_lua_demangle_wrongline(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef00,
			    "@chunks/demangle/funcdefs.lua", 500);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_lua_demangle_nofunc(struct ujp_buffer *buf)
{
	ujp_write_byte(buf, UJ_VMST_LFUNC);
	ujp_write_new_lfunc(buf, UJP_FRAME_TOP, 0xdeadbeef00,
			    "@chunks/demangle/funcdefs.lua", 13);
	ujp_write_bottom_frame(buf);
}

void ujmp_mock_vdso(struct ujp_buffer *buf)
{
	void *vdso =
		dlopen("linux-vdso.so.1", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
	void (*vdso_get_tod)(void);
	void (*vdso_get_cpu)(void);
	void (*vdso_clock_gettime)(void);
	void (*vdso_time)(void);

	/*
	 * See http://man7.org/linux/man-pages/man7/vdso.7.html, "vDSO names"
	 * section.
	 */
	if (vdso == NULL)
		vdso = dlopen("linux-gate.so.1",
			      RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);

	UJ_PEDANTIC_OFF
	/* dlsym returns void* and we cast it to a function ptr */
	vdso_get_tod = dlsym(vdso, "__vdso_gettimeofday");
	vdso_get_cpu = dlsym(vdso, "__vdso_getcpu");
	vdso_clock_gettime = dlsym(vdso, "__vdso_clock_gettime");
	vdso_time = dlsym(vdso, "__vdso_time");
	UJ_PEDANTIC_ON
	/*
	 * There is no checks for returned addresses since they may be invalid
	 * when running under valgrind.
	 */

	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf, (uint64_t)vdso_get_tod);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf, (uint64_t)vdso_get_cpu);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf, (uint64_t)vdso_clock_gettime);
	ujp_write_byte(buf, UJ_VMST_IDLE);
	ujp_write_hvmstate(buf, (uint64_t)vdso_time);

	dlclose(vdso);
}
