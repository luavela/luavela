/*
 * uJIT VM builder: library definition compiler.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 */

#include "buildvm.h"
#include "uj_funcid.h"
#include "uj_lib.h"

/* Context for library definitions. */
static uint8_t obuf[8192];
static uint8_t *optr;
static char modname[80];
static size_t modnamelen;
static char funcname[80];
static int modstate, regfunc;
static int ffasmfunc;

/* Fast functions IDs: */
static int ffid; /* last scanned */
static int recffid; /* last one for which recorder was defined */

enum {
  REGFUNC_OK,
  REGFUNC_NOREG,
  REGFUNC_NOREGUV
};

static void libdef_name(const char *p, int kind) {
  size_t n = strlen(p);
  if (kind != LIBINIT_STRING) {
    if (n > modnamelen && p[modnamelen] == '_' &&
      !strncmp(p, modname, modnamelen)) {
      p += modnamelen+1;
      n -= modnamelen+1;
    }
  }
  if (n > LIBINIT_MAXSTR) {
    fprintf(stderr, "Error: string too long: '%s'\n",  p);
    exit(1);
  }
  if (optr+1+n+2 > obuf+sizeof(obuf)) {  /* +2 for caller. */
    fprintf(stderr, "Error: output buffer overflow\n");
    exit(1);
  }
  *optr++ = (uint8_t)(n | kind);
  memcpy(optr, p, n);
  optr += n;
}

static void libdef_endmodule(BuildCtx *ctx) {
  if (modstate != 0) {
    char line[80];
    const uint8_t *p;
    int n;
    if (modstate == 1) {
      fprintf(ctx->fp, "  (lua_CFunction)0");
    }
    fprintf(ctx->fp, "\n};\n");
    fprintf(ctx->fp, "static const uint8_t %s%s[] = {\n",
      LABEL_PREFIX_LIBINIT, modname);
    line[0] = '\0';
    for (n = 0, p = obuf; p < optr; p++) {
      n += sprintf(line+n, "%d,", *p);
      if (n >= 75) {
        fprintf(ctx->fp, "%s\n", line);
        n = 0;
        line[0] = '\0';
      }
    }
    fprintf(ctx->fp, "%s%d\n};\n#endif\n\n", line, LIBINIT_END);
  }
}

static void libdef_module(BuildCtx *ctx, char *p, int arg) {
  UNUSED(arg);
  if (ctx->mode == BUILD_libdef) {
    libdef_endmodule(ctx);
    optr = obuf;
    *optr++ = (uint8_t)(ffid + 1);
    *optr++ = (uint8_t)ffasmfunc;
    *optr++ = 0;  /* Hash table size. */
    modstate = 1;
    fprintf(ctx->fp, "#ifdef %sMODULE_%s\n", LIBDEF_PREFIX, p);
    fprintf(ctx->fp, "#undef %sMODULE_%s\n", LIBDEF_PREFIX, p);
    fprintf(ctx->fp, "static const lua_CFunction %s%s[] = {\n",
      LABEL_PREFIX_LIBCF, p);
  }
  modnamelen = strlen(p);
  if (modnamelen > sizeof(modname)-1) {
    fprintf(stderr, "Error: module name too long: '%s'\n", p);
    exit(1);
  }
  strcpy(modname, p);
}

static void libdef_func(BuildCtx *ctx, char *p, int arg) {
  if (arg != LIBINIT_CF) {
    ffasmfunc++;
  }
  if (ctx->mode == BUILD_libdef) {
    if (modstate == 0) {
      fprintf(stderr, "Error: no module for function definition %s\n", p);
      exit(1);
    }
    if (regfunc == REGFUNC_NOREG) {
      if (optr+1 > obuf+sizeof(obuf)) {
        fprintf(stderr, "Error: output buffer overflow\n");
        exit(1);
      }
      *optr++ = LIBINIT_FFID;
    } else {
      if (arg != LIBINIT_ASM_) {
        if (modstate != 1) { fprintf(ctx->fp, ",\n"); }
        modstate = 2;
        fprintf(ctx->fp, "  %s%s", arg ? LABEL_PREFIX_FFH : LABEL_PREFIX_CF, p);
      }
      if (regfunc != REGFUNC_NOREGUV) { obuf[2]++; } /* Bump hash table size. */
      libdef_name(regfunc == REGFUNC_NOREGUV ? "" : p, arg);
    }
  } else if (ctx->mode == BUILD_ffdef) {
    fprintf(ctx->fp, "FFDEF(%s)\n", p);
  } else if (ctx->mode == BUILD_recdef) {
    if (strlen(p) > sizeof(funcname)-1) {
      fprintf(stderr, "Error: function name too long: '%s'\n", p);
      exit(1);
    }
    strcpy(funcname, p);
  } else if (ctx->mode == BUILD_bcdef) {
    if (arg != LIBINIT_CF) { /* LIBINIT_ASM, LIBINIT_ASM_ */
      fprintf(ctx->fp, " \\\n  _(%s)", p);
    }
  }
  ffid++;
  regfunc = REGFUNC_OK;
}

/*
 * While recffid is behind ffid, specifies the default nyi recorder for all
 * scanned fast functions. Once ffid is reached, specifies either a real
 * recorder (if it was really declared in the code base) or adds an extra nyi.
 */
static void libdef_rec_dump(FILE *fp, const char *fn)
{
  lua_assert(recffid <= ffid);
  while (recffid != ffid) {
    const char* name = (recffid + 1 == ffid && fn != NULL) ? fn : "nyi";
    fprintf(fp, ",\n\trecff_%s", name);
    recffid++;
  }
}

static void libdef_rec(BuildCtx *ctx, char *p, int arg) {
  UNUSED(arg);
  UNUSED(p);
  if (ctx->mode != BUILD_recdef)
    return;

  libdef_rec_dump(ctx->fp, funcname);
}

static void libdef_push(BuildCtx *ctx, char *p, int arg) {
  UNUSED(arg);
  if (ctx->mode == BUILD_libdef) {
    int len = (int)strlen(p);
    if (*p == '"') {
      if (len > 1 && p[len-1] == '"') {
        p[len-1] = '\0';
        libdef_name(p+1, LIBINIT_STRING);
        return;
      }
    } else if ((*p >= '0' && *p <= '9') ||
               (len == 3 && !strncmp(p, "NAN", 3))) {
      char *ep;
      double d = strtod(p, &ep);
      if (*ep == '\0') {
        if (optr+1+sizeof(double) > obuf+sizeof(obuf)) {
          fprintf(stderr, "Error: output buffer overflow\n");
          exit(1);
        }
        *optr++ = LIBINIT_NUMBER;
        memcpy(optr, &d, sizeof(double));
        optr += sizeof(double);
        return;
      }
    } else if (!strcmp(p, "lastcl")) {
      if (optr+1 > obuf+sizeof(obuf)) {
        fprintf(stderr, "Error: output buffer overflow\n");
        exit(1);
      }
      *optr++ = LIBINIT_LASTCL;
      return;
    } else if (len > 4 && !strncmp(p, "top-", 4)) {
      if (optr+2 > obuf+sizeof(obuf)) {
        fprintf(stderr, "Error: output buffer overflow\n");
        exit(1);
      }
      *optr++ = LIBINIT_COPY;
      *optr++ = (uint8_t)atoi(p+4);
      return;
    }
    fprintf(stderr, "Error: bad value for %sPUSH(%s)\n", LIBDEF_PREFIX, p);
    exit(1);
  }
}

static void libdef_set(BuildCtx *ctx, char *p, int arg) {
  UNUSED(arg);
  if (ctx->mode == BUILD_libdef) {
    if (p[0] == '!' && p[1] == '\0') { p[0] = '\0'; } /* Set env. */
    libdef_name(p, LIBINIT_STRING);
    *optr++ = LIBINIT_SET;
    obuf[2]++;  /* Bump hash table size. */
  }
}

static void libdef_regfunc(BuildCtx *ctx, char *p, int arg) {
  UNUSED(ctx); UNUSED(p);
  regfunc = arg;
}

typedef void (*LibDefFunc)(BuildCtx *ctx, char *p, int arg);

typedef struct LibDefHandler {
  const char *suffix;
  const char *stop;
  const LibDefFunc func;
  const int arg;
} LibDefHandler;

static const LibDefHandler libdef_handlers[] = {
  { "MODULE_",  " \t\r\n",      libdef_module,          0 },
  { "CF(",      ")",            libdef_func,            LIBINIT_CF },
  { "ASM(",     ")",            libdef_func,            LIBINIT_ASM },
  { "ASM_(",    ")",            libdef_func,            LIBINIT_ASM_ },
  { "REC(",     ")",            libdef_rec,             0 },
  { "PUSH(",    ")",            libdef_push,            0 },
  { "SET(",     ")",            libdef_set,             0 },
  { "NOREGUV",  NULL,           libdef_regfunc,         REGFUNC_NOREGUV },
  { "NOREG",    NULL,           libdef_regfunc,         REGFUNC_NOREG },
  { NULL,       NULL,           (LibDefFunc)0,          0 }
};

/* Emit C source code for library function definitions. */
void emit_lib(BuildCtx *ctx) {
  const char *fname;

  if (ctx->mode == BUILD_ffdef || ctx->mode == BUILD_libdef ||
      ctx->mode == BUILD_recdef) {
    fprintf(ctx->fp, "/* This is a generated file. DO NOT EDIT! */\n\n");
  }
  if (ctx->mode == BUILD_recdef) {
    fprintf(ctx->fp, "static const RecordFunc recff_idmap[] = {\n\trecff_nyi,\n\trecff_c");
  }
  ffid = FF_C;
  recffid = FF_C;
  ffasmfunc = 0;

  while ((fname = *ctx->args++)) {
    char buf[256];  /* We don't care about analyzing lines longer than that. */
    FILE *fp;
    if (fname[0] == '-' && fname[1] == '\0') {
      fp = stdin;
    } else {
      fp = fopen(fname, "r");
      if (!fp) {
        fprintf(stderr, "Error: cannot open input file '%s': %s\n",
          fname, strerror(errno));
        exit(1);
      }
    }
    modstate = 0;
    regfunc = REGFUNC_OK;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
      char *p;
      /* Simplistic pre-processor. Only handles top-level #if/#endif. */
      if (buf[0] == '#' && buf[1] == 'i' && buf[2] == 'f') {
        int ok = 1;
        if (!strcmp(buf, "#if LJ_52\n")) {
          ok = LJ_52;
        } else if (!strcmp(buf, "#if LJ_HASJIT\n")) {
          ok = LJ_HASJIT;
        } else if (!strcmp(buf, "#if LJ_HASFFI\n")) {
          ok = LJ_HASFFI;
        }
        if (!ok) {
          int lvl = 1;
          while (fgets(buf, sizeof(buf), fp) != NULL) {
            if (buf[0] == '#' && buf[1] == 'e' && buf[2] == 'n') {
              if (--lvl == 0) { break; }
            } else if (buf[0] == '#' && buf[1] == 'i' && buf[2] == 'f') {
              lvl++;
            }
          }
          continue;
        }
      }
      for (p = buf; (p = strstr(p, LIBDEF_PREFIX)) != NULL; ) {
        const LibDefHandler *ldh;
        p += sizeof(LIBDEF_PREFIX)-1;
        for (ldh = libdef_handlers; ldh->suffix != NULL; ldh++) {
          size_t n, len = strlen(ldh->suffix);
          if (!strncmp(p, ldh->suffix, len)) {
            p += len;
            n = ldh->stop ? strcspn(p, ldh->stop) : 0;
            if (!p[n]) { break; }
            p[n] = '\0';
            ldh->func(ctx, p, ldh->arg);
            p += n+1;
            break;
          }
        }
        if (ldh->suffix == NULL) {
          buf[strlen(buf)-1] = '\0';
          fprintf(stderr, "Error: unknown library definition tag %s%s\n",
            LIBDEF_PREFIX, p);
          exit(1);
        }
      }
    }
    fclose(fp);
    if (ctx->mode == BUILD_libdef) {
      libdef_endmodule(ctx);
    }
  }

  if (ctx->mode == BUILD_ffdef) {
    fprintf(ctx->fp, "\n#undef FFDEF\n\n");
    fprintf(ctx->fp,
      "#ifndef FF_NUM_ASMFUNC\n#define FF_NUM_ASMFUNC %d\n#endif\n\n",
      ffasmfunc);
  } else if (ctx->mode == BUILD_bcdef) {
    fprintf(ctx->fp, "\n"); /* To avoid possible backslash-newline warnings. */
  } else if (ctx->mode == BUILD_recdef) {
    libdef_rec_dump(ctx->fp, NULL); /* sentinel */
    fprintf(ctx->fp, "\n};\n");
    fprintf(ctx->fp, "\n#define SIZEOF_RECFF_IDMAP (sizeof(recff_idmap) / sizeof(recff_idmap[0]))\n\n");
  }
}
