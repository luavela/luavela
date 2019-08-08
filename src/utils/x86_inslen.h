/*
 * Fast x86 instruction length decoder.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 * Originally contributed to LuaJIT by Peter Cawley.
 */

#ifndef _UJIT_UTILS_X86_INSLEN_H
#define _UJIT_UTILS_X86_INSLEN_H

#include "lj_def.h"

uint32_t x86_inslen(const uint8_t* p);

#endif /* !_UJIT_UTILS_X86_INSLEN_H */
