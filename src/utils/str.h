/*
 * Common string utilities.
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJIT_UTILS_STR_H
#define _UJIT_UTILS_STR_H

/* Converts input's bytes to lower case and writes the result to the out buffer.
** Returns pointer to the out buffer. Does not check input string length.
*/
const char *to_lower(char *out, const char *s);

/* Replaces underscore '_' with a dot '.' in place.
** Does not check input string length.
*/
void replace_underscores(char *s);

#endif /* !_UJIT_UTILS_STR_H */
