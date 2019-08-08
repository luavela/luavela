/*
 * uJIT VM builder: Assembler source code emitter.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "buildvm.h"

/* ------------------------------------------------------------------------ */

/* Emit bytes piecewise as assembler text. */
static void emit_asm_bytes(BuildCtx *ctx, uint8_t *p, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if ((i & 15) == 0) {
      fprintf(ctx->fp, "\t.byte %d", p[i]);
    } else {
      fprintf(ctx->fp, ",%d", p[i]);
    }
    if ((i & 15) == 15) { putc('\n', ctx->fp); }
  }
  if ((n & 15) != 0) putc('\n', ctx->fp);
}

/* Emit relocation */
static void emit_asm_reloc(BuildCtx *ctx, int type, const char *sym) {
  switch (ctx->mode) {
  case BUILD_elfasm:
    if (type) {
      fprintf(ctx->fp, "\t.long %s-.-4\n", sym);
    } else {
      fprintf(ctx->fp, "\t.long %s\n", sym);
    }
    break;
  default:
    break;
  }
}

#define ELFASM_PX       "@"

/* Emit an assembler label. */
static void emit_asm_label(BuildCtx *ctx, const char *name, int size, int isfunc) {
  switch (ctx->mode) {
  case BUILD_elfasm:
    fprintf(ctx->fp,
      "\n\t.globl %s\n"
      "\t.hidden %s\n"
      "\t.type %s, " ELFASM_PX "%s\n"
      "\t.size %s, %d\n"
      "%s:\n",
      name, name, name, isfunc ? "function" : "object", name, size, name);
    break;
  default:
    break;
  }
}

/* Emit alignment. */
static void emit_asm_align(BuildCtx *ctx, int bits) {
  switch (ctx->mode) {
  case BUILD_elfasm:
    fprintf(ctx->fp, "\t.p2align %d\n", bits);
    break;
  default:
    break;
  }
}

/* ------------------------------------------------------------------------ */

/* Emit assembler source code. */
void emit_asm(BuildCtx *ctx) {
  int i, rel;

  fprintf(ctx->fp, "\t.file \"buildvm_%s.dasc\"\n", ctx->dasm_arch);
  fprintf(ctx->fp, "\t.text\n");
  emit_asm_align(ctx, 4);

  emit_asm_label(ctx, ctx->beginsym, 0, 0);
  fprintf(ctx->fp, ".Lbegin:\n");

  for (i = rel = 0; i < ctx->nsym; i++) {
    int32_t ofs = ctx->sym[i].ofs;
    int32_t next = ctx->sym[i+1].ofs;
    emit_asm_label(ctx, ctx->sym[i].name, next - ofs, 1);
    while (rel < ctx->nreloc && ctx->reloc[rel].ofs <= next) {
      BuildReloc *r = &ctx->reloc[rel];
      int n = r->ofs - ofs;
      emit_asm_bytes(ctx, ctx->code+ofs, n);
      emit_asm_reloc(ctx, r->type, ctx->relocsym[r->sym]);
      ofs += n+4;
      rel++;
    }
    emit_asm_bytes(ctx, ctx->code+ofs, next-ofs);
  }

  fprintf(ctx->fp, "\n");
  switch (ctx->mode) {
  case BUILD_elfasm: {
    fprintf(ctx->fp, "\t.section .note.GNU-stack,\"\"," ELFASM_PX "progbits\n");
    fprintf(ctx->fp, "\t.ident \"%s\"\n", ctx->dasm_ident);
    break;
  }
  default: { break; }
  }
  fprintf(ctx->fp, "\n");
}
