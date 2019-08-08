/*
 * This module allows to access processor identification and feature
 * information as exposed by CPUID instruction and described in "Intel(r)
 * 64 and IA-32 Architectures Software Developer's Manual".
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_UTILS_CPUINFO_H_
#define _UJIT_UTILS_CPUINFO_H_

/* Returns non-zero if conditional mov CMOV instruction is supported. Returns 0 otherwise. */
int cpuinfo_has_cmov(void);

/* Returns non-zero if SSE2 is supported. Returns 0 otherwise. */
int cpuinfo_has_sse2(void);

/* Returns non-zero if SSE3 is supported. Returns 0 otherwise. */
int cpuinfo_has_sse3(void);

/* Returns non-zero if SSE 4.1 is supported. Returns 0 otherwise. */
int cpuinfo_has_sse4_1(void);

#endif /* !_UJIT_UTILS_CPUINFO_H_ */
