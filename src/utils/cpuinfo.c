/*
 * Accessing processor identification and feature information.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * NB! Built-in function __get_cpuid is not documented in GCC manual,
 * but it's been available since forever. For newer versions of GCC
 * starting with 4.8 documented __builtin_cpu_supports is used.
 */

#include "cpuinfo.h"

#ifndef __GNUC__
/* Since this module depends heavily on compiler builtins (which is
** a good thing itself), it's known or most likely to fail on compilers
** other than explicitly supported.
*/
#error "cpuinfo module supports GCC compilation only"
#endif

#if __GNUC__ < 4 || \
  ( __GNUC__ == 4 && __GNUC_MINOR < 8)
#include <cpuid.h>

int cpuinfo_has_cmov(void) {
  unsigned int eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    return ((edx >> 15) & 1);
  }
  return 0;
}

int cpuinfo_has_sse2(void) {
  unsigned int eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    return ((edx >> 26) & 1);
  }
  return 0;
}

int cpuinfo_has_sse3(void) {
  unsigned int eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    return ((ecx >> 9) & 1);
  }
  return 0;
}

int cpuinfo_has_sse4_1(void) {
  unsigned int eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    return ((ecx >> 19) & 1);
  }
  return 0;
}

#else

int cpuinfo_has_cmov(void) {
  return __builtin_cpu_supports("cmov");
}

int cpuinfo_has_sse2(void) {
  return __builtin_cpu_supports("sse2");
}

int cpuinfo_has_sse3(void) {
  return __builtin_cpu_supports("sse3");
}

int cpuinfo_has_sse4_1(void) {
  return __builtin_cpu_supports("sse4.1");
}
#endif

