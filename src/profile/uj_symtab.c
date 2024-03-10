/*
 * Implementation of the Lua symbol table dumper.
 *
 * Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
 * Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 */

#include "lj_obj.h"
#include "profile/ujp_write.h"
#include "profile/uj_symtab.h"

#define UJS_CURRENT_VERSION 2

static const char ujs_header[] = {'u', 'j', 's', UJS_CURRENT_VERSION,
				  0x0, 0x0, 0x0};

static void symtab_write_prologue(struct ujp_buffer *out,
				  const struct global_State *g)
{
	size_t i;
	const size_t len = sizeof(ujs_header) / sizeof(ujs_header[0]);

	for (i = 0; i < len; i++)
		ujp_write_byte(out, ujs_header[i]);

	ujp_write_u64(out, (uint64_t)g);
}

void uj_symtab_write(struct ujp_buffer *out, const struct global_State *g)
{
	const GCobj *o;
	const GCobj **iter = (const GCobj **)&g->gc.root;

	symtab_write_prologue(out, g);

	while (NULL != (o = *iter)) {
		switch (o->gch.gct) {
		case (~LJ_TPROTO): {
			const GCproto *pt = gco2pt(o);

			ujp_write_byte(out, SYMTAB_LFUNC);
			ujp_write_u64(out, (uint64_t)pt);
			ujp_write_string(out, proto_chunknamestr(pt));
			ujp_write_u64(out, (uint64_t)pt->firstline);
			break;
		}
		case (~LJ_TTRACE): {
			/* TODO: Implement dumping a trace info */
			break;
		}
		default: {
			break;
		}
		}
		iter = (const GCobj **)&o->gch.nextgc;
	}

	ujp_write_byte(out, SYMTAB_FINAL);
}
