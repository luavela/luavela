/*
 * Utilities for working with randomness.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <stdlib.h>
#include <limits.h>
#include <time.h>

unsigned int random_time_seed(void) {
  time_t timeval;      /* Current time. */
  unsigned char *ptr;  /* Type punned pointed into timeval. */
  unsigned int   seed; /* Generated seed. */
  size_t i;

  timeval = time(NULL);
  ptr     = (unsigned char *)&timeval;

  seed = 0;
  for (i = 0; i < sizeof(timeval); i++) {
    seed = seed * (UCHAR_MAX + 2u) + ptr[i];
  }

  return seed;
}

void random_hex_file_extension(char *buffer, size_t n) {
  static const char hex_alphabet[] = "0123456789abcdef";

  if (n == 0) {
    return;
  }

  *buffer++ = '.';
  n--;

  while (n-- > 0) {
    size_t index = (size_t)((double)random() / RAND_MAX * (sizeof(hex_alphabet) - 1));
    *buffer++    = hex_alphabet[index];
  }
}
