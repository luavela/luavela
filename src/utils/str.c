/*
 * Common string utilities.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include <string.h>
#include "utils/lj_char.h"

const char *to_lower(char *buf, const char *s) {
  char *p = buf;
  char c;
  while ((c = *s++)) {
    *p++ = lj_char_tolower(c);
  }
  *p = '\0';
  return buf;
}

void replace_underscores(char *s) {
  char *p = strchr(s, '_');
  while (p) {
    *p = '.';
    p  = strchr(s, '_');
  }
}
