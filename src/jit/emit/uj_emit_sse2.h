/*
 * Emitter of SSE2 instructions.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_EMIT_SSE2_H
#define _UJ_EMIT_SSE2_H

#include <stdint.h>

/* movdqu xmm, [gpr] */
size_t uj_emit_movxmmrm(uint8_t *mc, uint8_t xmm, uint8_t gpr);

/* movdqu [gpr], xmm */
size_t uj_emit_movrmxmm(uint8_t *mc, uint8_t gpr, uint8_t xmm);

/* movdqu xmm, XMMWORD PTR [rsp + ofs] */
size_t uj_emit_spload_xmm(uint8_t *mc, uint8_t xmm, int32_t ofs);

/* movdqu XMMWORD PTR [rsp + ofs], xmm */
size_t uj_emit_spstore_xmm(uint8_t *mc, uint8_t xmm, int32_t ofs);

#endif /* !_UJ_EMIT_SSE2_H */
