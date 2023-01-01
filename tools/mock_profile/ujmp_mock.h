/*
 * Fake streams generator for uJIT profiler.
 * Copyright (C) 2020-2023 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJMP_MOCK_H
#define _UJMP_MOCK_H

struct ujp_buffer;

void ujmp_mock_all_vmstates(struct ujp_buffer *buf);
void ujmp_mock_wrong_ffunc(struct ujp_buffer *buf);
void ujmp_mock_no_bottom(struct ujp_buffer *buf);
void ujmp_mock_main_lua(struct ujp_buffer *buf);
void ujmp_mock_marked_lfunc(struct ujp_buffer *buf);
void ujmp_mock_hvmstates(struct ujp_buffer *buf);
void ujmp_mock_lfunc_miscached(struct ujp_buffer *buf);
void ujmp_mock_lfunc_diffnames(struct ujp_buffer *buf);
void ujmp_mock_lfunc_difflines(struct ujp_buffer *buf);
void ujmp_mock_trace_miscached(struct ujp_buffer *buf);
void ujmp_mock_trace_diffnames(struct ujp_buffer *buf);
void ujmp_mock_trace_difflines(struct ujp_buffer *buf);
void ujmp_mock_duplicates(struct ujp_buffer *buf);
void ujmp_mock_lua_demangle(struct ujp_buffer *buf);
void ujmp_mock_lua_demangle_badfile(struct ujp_buffer *buf);
void ujmp_mock_lua_demangle_wrongline(struct ujp_buffer *buf);
void ujmp_mock_lua_demangle_nofunc(struct ujp_buffer *buf);
void ujmp_mock_vdso(struct ujp_buffer *buf);

#endif /* !_UJMP_MOCK_H */
