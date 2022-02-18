/*
 * Various utility functions for C-level dumpers.
 * Copyright (C) 2020-2022 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#ifndef _UJ_DUMP_UTILS_H
#define _UJ_DUMP_UTILS_H

#include <stdio.h>

/* Dumps description of non-Lua function fn to out. */
void uj_dump_nonluafunc_description(FILE *out, const GCfunc *fn);

/*
 * Dumps description of any functional object fn to out. If applicable, source
 * code line number corresponding to pc is dumped as well.
 */
void uj_dump_func_description(FILE *out, const GCfunc *fn, BCPos pc);

/*
 * Prints description of any functional object fn to buffer buf. If
 * applicable, source code line number corresponding to pc is dumped as well.
 * Buffer overflow is not checked.
 */
void uj_dump_format_func_description(char *buf, const GCfunc *fn, BCPos pc);

/*
 * Copies characters from val to fmt_buf escaping all control characters and
 * enclosing fmt_buf into "..." quotes. As soon as >=copy_threshold
 * octets are written to fmt_buf, further copying stops and '~' is appended
 * to fmt_buf to mark a stripped output. Returns number of octets written.
 * Correct length of fmt_buf should be ensured by the caller.
 * NB! This implementation differs from the original dumpers:
 * -- bc.lua:
 *     kc = format(#kc > 40 and '"%.40s"~' or '"%s"', gsub(kc, "%c", ctlsub))
 * -- dump.lua (IR dumper):
 *     s = format(#k > 20 and '"%.20s"~' or '"%s"', gsub(k, "%c", ctlsub))
 * as it guarantees that possible trailing escape sequence will never be
 * incorrectly stripped.
 */
size_t uj_dump_format_kstr(char *fmt_buf, const char *val,
			   size_t copy_threshold);

#endif /* !_UJ_DUMP_UTILS_H */
