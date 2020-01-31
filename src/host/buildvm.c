/*
 * uJIT VM builder.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * This is a tool to build the hand-tuned assembler code required for
 * uJIT's bytecode interpreter. It supports a variety of output formats
 * to feed different toolchains (see usage() below).
 *
 * This tool is not particularly optimized because it's only used while
 * _building_ uJIT. There's no point in distributing or installing it.
 * Only the object code generated by this tool is linked into uJIT.
 *
 * Caveat: some memory is not free'd, error handling is lazy.
 * It's a one-shot tool -- any effort fixing this would be wasted.
 */

#include "lua.h"
#include "lextlib.h"

#include "buildvm.h"
#include "lj_obj.h"
#include "lj_bc.h"
#include "lj_gc.h"
#include "uj_err.h"
#include "uj_throw.h"
#include "jit/lj_ir.h"
#include "jit/lj_ircall.h"
#include "lj_frame.h"
#include "uj_dispatch.h"
#if LJ_HASFFI
#include "ffi/lj_ctype.h"
#include "ffi/lj_ccall.h"
#endif
#include "ujit.h"
#include "uj_cframe.h"
#include "uj_state.h"

/* ------------------------------------------------------------------------ */

/* DynASM glue definitions. */
#define Dst             ctx
#define Dst_DECL        BuildCtx *ctx
#define Dst_REF         (ctx->D)
#define DASM_CHECKS     1

#include "../dynasm/dasm_proto.h"

/* Glue macros for DynASM. */
static int collect_reloc(BuildCtx *ctx, uint8_t *addr, int idx, int type);

#define DASM_EXTERN(ctx, addr, idx, type) \
  collect_reloc(ctx, addr, idx, type)

/* ------------------------------------------------------------------------ */

/* Avoid trouble if cross-compiling for an x86 target. Speed doesn't matter. */
#define DASM_ALIGNED_WRITES     1

/* Embed architecture-specific DynASM encoder. */
#include "../dynasm/dasm_x86.h"

/* Embed generated architecture-specific backend. */
#include "buildvm_arch.h"

/* ------------------------------------------------------------------------ */

void owrite(BuildCtx *ctx, const void *ptr, size_t sz) {
  if (fwrite(ptr, 1, sz, ctx->fp) != sz) {
    fprintf(stderr, "Error: cannot write to output file: %s\n", strerror(errno));
    exit(1);
  }
}

/* ------------------------------------------------------------------------ */

/* Emit code as raw bytes. Only used for DynASM debugging. */
static void emit_raw(BuildCtx *ctx) {
  owrite(ctx, ctx->code, ctx->codesz);
}

/* -- Build machine code -------------------------------------------------- */

static const char *sym_decorate(BuildCtx *ctx, const char *prefix, const char *suffix) {
  char name[256];
  char *p;
  const char *symprefix = ctx->mode == BUILD_machasm ? "_" : "";
  sprintf(name, "%s%s%s", symprefix, prefix, suffix);
  p = strchr(name, '@');
  if (p) {
    *p = '\0';
  }
  p = (char *)malloc(strlen(name)+1);  /* MSVC doesn't like strdup. */
  strcpy(p, name);
  return p;
}

#define NRELOCSYM       (sizeof(extnames)/sizeof(extnames[0])-1)

static int relocmap[NRELOCSYM];

/* Collect external relocations. */
static int collect_reloc(BuildCtx *ctx, uint8_t *addr, int idx, int type) {
  if (ctx->nreloc >= BUILD_MAX_RELOC) {
    fprintf(stderr, "Error: too many relocations, increase BUILD_MAX_RELOC.\n");
    exit(1);
  }
  if (relocmap[idx] < 0) {
    relocmap[idx] = ctx->nrelocsym;
    ctx->relocsym[ctx->nrelocsym] = sym_decorate(ctx, "", extnames[idx]);
    ctx->nrelocsym++;
  }
  ctx->reloc[ctx->nreloc].ofs = (int32_t)(addr - ctx->code);
  ctx->reloc[ctx->nreloc].sym = relocmap[idx];
  ctx->reloc[ctx->nreloc].type = type;
  ctx->nreloc++;
  return 0;  /* Encode symbol offset of 0. */
}

/* Naive insertion sort. Performance doesn't matter here. */
static void sym_insert(BuildCtx *ctx, int32_t ofs, const char *prefix, const char *suffix) {
  ptrdiff_t i = ctx->nsym++;
  while (i > 0) {
    if (ctx->sym[i-1].ofs <= ofs) {
      break;
    }
    ctx->sym[i] = ctx->sym[i-1];
    i--;
  }
  ctx->sym[i].ofs = ofs;
  ctx->sym[i].name = sym_decorate(ctx, prefix, suffix);
}

/* Build the machine code. */
static int build_code(BuildCtx *ctx) {
  int status;
  int i;

  /* Initialize DynASM structures. */
  ctx->nglob = GLOB__MAX;
  ctx->glob = (void **)malloc(ctx->nglob*sizeof(void *));
  memset(ctx->glob, 0, ctx->nglob*sizeof(void *));
  ctx->nreloc = 0;

  ctx->globnames = globnames;
  ctx->relocsym = (const char **)malloc(NRELOCSYM*sizeof(const char *));
  ctx->nrelocsym = 0;
  for (i = 0; i < (int)NRELOCSYM; i++) { relocmap[i] = -1; }

  ctx->dasm_ident = DASM_IDENT;
  ctx->dasm_arch = DASM_ARCH;

  dasm_init(Dst, DASM_MAXSECTION);
  dasm_setupglobal(Dst, ctx->glob, ctx->nglob);
  dasm_setup(Dst, build_actionlist);

  /* Call arch-specific backend to emit the code. */
  ctx->npc = build_backend(ctx);

  /* Finalize the code. */
  (void)dasm_checkstep(Dst, -1);
  if ((status = dasm_link(Dst, &ctx->codesz))) { return status; }
  ctx->code = (uint8_t *)malloc(ctx->codesz);
  if ((status = dasm_encode(Dst, (void *)ctx->code))) { return status; }

  /* Allocate symbol table and bytecode offsets. */
  ctx->beginsym = sym_decorate(ctx, "", LABEL_PREFIX "vm_asm_begin");
  ctx->sym = (BuildSym *)malloc((ctx->npc+ctx->nglob+1)*sizeof(BuildSym));
  ctx->nsym = 0;
  ctx->bc_ofs = (int32_t *)malloc(ctx->npc*sizeof(int32_t));

  /* Collect the opcodes (PC labels). */
  for (i = 0; i < ctx->npc; i++) {
    int32_t ofs = dasm_getpclabel(Dst, i);
    if (ofs < 0) { return 0x22000000|i; }
    ctx->bc_ofs[i] = ofs;
    /*
     * Empty functions are generated for JIT/FFI specific bytecodes
     * unconditionally for linking to succeed if JIT/FFI is disabled.
     */
    sym_insert(ctx, ofs, LABEL_PREFIX_BC, bc_names[i]);
  }

  /* Collect the globals (named labels). */
  for (i = 0; i < ctx->nglob; i++) {
    const char *gl = globnames[i];
    int len = (int)strlen(gl);
    if (!ctx->glob[i]) {
      fprintf(stderr, "Error: undefined global %s\n", gl);
      exit(2);
    }
    /* Skip the _Z symbols. */
    if (!(len >= 2 && gl[len-2] == '_' && gl[len-1] == 'Z')) {
      sym_insert(ctx, (int32_t)((uint8_t *)(ctx->glob[i]) - ctx->code),
        LABEL_PREFIX, globnames[i]);
    }
  }

  /* Close the address range. */
  sym_insert(ctx, (int32_t)ctx->codesz, "", "");
  ctx->nsym--;

  dasm_free(Dst);

  return 0;
}

/* -- Generate VM enums --------------------------------------------------- */

const char *const bc_names[] = {
#define BCNAME(name, ma, mb, mc, mt)       #name,
BCDEF(BCNAME)
#undef BCNAME
  NULL
};

const char *const ir_names[] = {
#define IRNAME(name, m, m1, m2) #name,
IRDEF(IRNAME)
#undef IRNAME
  NULL
};

const char *const irt_names[] = {
#define IRTNAME(name, size)     #name,
IRTDEF(IRTNAME)
#undef IRTNAME
  NULL
};

const char *const irfpm_names[] = {
#define FPMNAME(name)           #name,
IRFPMDEF(FPMNAME)
#undef FPMNAME
  NULL
};

const char *const irfield_names[] = {
#define FLNAME(name, ofs)       #name,
IRFLDEF(FLNAME)
#undef FLNAME
  NULL
};

const char *const ircall_names[] = {
#define IRCALLNAME(cond, name, nargs, kind, type, flags)        #name,
IRCALLDEF(IRCALLNAME)
#undef IRCALLNAME
  NULL
};

/* Emit C source code for bytecode-related definitions. */
static void emit_bcdef(BuildCtx *ctx) {
  fprintf(ctx->fp, "/* This is a generated file. DO NOT EDIT! */\n\n");

  fprintf(ctx->fp, "/* This is an ordered list of all library functions with assembler counterparts. */\n");

  fprintf(ctx->fp, "#define ASMDEF(_)");
  emit_lib(ctx);
}

/* -- Argument parsing ---------------------------------------------------- */

/* Build mode names. */
static const char *const modenames[] = {
#define BUILDNAME(name)         #name,
BUILDDEF(BUILDNAME)
#undef BUILDNAME
  NULL
};

/* Print usage information and exit. */
static void usage(void) {
  int i;
  fprintf(stderr, UJIT_PRODUCT_NAME_VERSION " VM builder.\n");
  fprintf(stderr, UJIT_COPYRIGHT ", " UJIT_URL "\n");
  fprintf(stderr, "Target architecture: " UJ_ARCH_NAME "\n\n");
  fprintf(stderr, "Usage: buildvm -m mode [-o outfile] [infiles...]\n\n");
  fprintf(stderr, "Available modes:\n");
  for (i = 0; i < BUILD__MAX; i++)
    fprintf(stderr, "  %s\n", modenames[i]);
  exit(1);
}

/* Parse the output mode name. */
static BuildMode parsemode(const char *mode) {
  int i;
  for (i = 0; modenames[i]; i++) {
    if (!strcmp(mode, modenames[i])) {
      return (BuildMode)i;
    }
  }
  usage();
  return (BuildMode)-1;
}

/* Parse arguments. */
static void parseargs(BuildCtx *ctx, char **argv) {
  const char *a;
  int i;
  ctx->mode = (BuildMode)-1;
  ctx->outname = "-";
  for (i = 1; (a = argv[i]) != NULL; i++) {
    if (a[0] != '-') {
      break;
    }
    switch (a[1]) {
    case '-': {
      if (a[2]) { goto err; }
      i++;
      goto ok;
    }
    case '\0': {
      goto ok;
    }
    case 'm': {
      i++;
      if (a[2] || argv[i] == NULL) { goto err; }
      ctx->mode = parsemode(argv[i]);
      break;
    }
    case 'o': {
      i++;
      if (a[2] || argv[i] == NULL) { goto err; }
      ctx->outname = argv[i];
      break;
    }
    default: err: {
      usage();
      break;
    }
    }
  }
ok:
  ctx->args = argv+i;
  if (ctx->mode == (BuildMode)-1) { goto err; }
}

int main(int argc, char **argv) {
  BuildCtx ctx_;
  BuildCtx *ctx = &ctx_;
  int status, binmode;

  if (sizeof(void *) != 8) {
    fprintf(stderr,"Error: pointer size mismatch in cross-build.\n");
    fprintf(stderr,"Try: make HOST_CC=\"gcc -m32\" CROSS=...\n\n");
    return 1;
  }

  UNUSED(argc);
  parseargs(ctx, argv);

  if ((status = build_code(ctx))) {
    fprintf(stderr,"Error: DASM error %08x\n", status);
    return 1;
  }

  switch (ctx->mode) {
  case BUILD_raw:
    binmode = 1;
    break;
  default:
    binmode = 0;
    break;
  }

  if (ctx->outname[0] == '-' && ctx->outname[1] == '\0') {
    ctx->fp = stdout;
  } else if (!(ctx->fp = fopen(ctx->outname, binmode ? "wb" : "w"))) {
    fprintf(stderr, "Error: cannot open output file '%s': %s\n",
      ctx->outname, strerror(errno));
    exit(1);
  }

  switch (ctx->mode) {
  case BUILD_elfasm:
  case BUILD_machasm: {
    emit_asm(ctx);
    emit_asm_debug(ctx);
    break;
  }
  case BUILD_raw: {
    emit_raw(ctx);
    break;
  }
  case BUILD_bcdef: {
    emit_bcdef(ctx);
    break;
  }
  case BUILD_ffdef: case BUILD_libdef: case BUILD_recdef: {
    emit_lib(ctx);
    break;
  }
  case BUILD_folddef: {
    emit_fold(ctx);
    break;
  }
  default: {
    break;
  }
  }

  fflush(ctx->fp);
  if (ferror(ctx->fp)) {
    fprintf(stderr, "Error: cannot write to output file: %s\n", strerror(errno));
    exit(1);
  }
  fclose(ctx->fp);

  return 0;
}
