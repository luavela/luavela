/*
 * I/O library.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2011 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include <errno.h>
#include <stdio.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "uj_err.h"
#include "uj_str.h"
#include "uj_sbuf.h"
#include "uj_state.h"
#include "uj_lib.h"
#include "utils/lj_char.h"

#define IOSTDF_UD(L, id)        (&(G(L)->gcroot[(id)])->ud)
#define IOSTDF_IOF(L, id)       ((struct IOFileUD *)uddata(IOSTDF_UD(L, (id))))

/* -- Open/close helpers -------------------------------------------------- */

static struct IOFileUD *io_tofilep(lua_State *L)
{
  if (!(L->base < L->top && tvisudata(L->base) &&
        udataV(L->base)->udtype == UDTYPE_IO_FILE))
    uj_err_argtype(L, 1, "FILE*");
  return (struct IOFileUD *)uddata(udataV(L->base));
}

static struct IOFileUD *io_tofile(lua_State *L)
{
  struct IOFileUD *iof = io_tofilep(L);
  if (iof->fp == NULL)
    uj_err_caller(L, UJ_ERR_IOCLFL);
  return iof;
}

static FILE *io_stdfile(lua_State *L, ptrdiff_t id)
{
  struct IOFileUD *iof = IOSTDF_IOF(L, id);
  if (iof->fp == NULL)
    uj_err_caller(L, UJ_ERR_IOSTDCL);
  return iof->fp;
}

static struct IOFileUD *io_file_new(lua_State *L)
{
  struct IOFileUD *iof = (struct IOFileUD *)lua_newuserdata(L, sizeof(struct IOFileUD));
  GCudata *ud = udataV(L->top-1);
  ud->udtype = UDTYPE_IO_FILE;
  /* NOBARRIER: The GCudata is new (marked white). */
  ud->metatable = curr_func(L)->c.env;
  iof->fp = NULL;
  iof->type = IOFILE_TYPE_FILE;
  return iof;
}

static struct IOFileUD *io_file_open(lua_State *L, const char *mode)
{
  const char *fname = strdata(uj_lib_checkstr(L, 1));
  struct IOFileUD *iof = io_file_new(L);
  iof->fp = fopen(fname, mode);
  if (iof->fp == NULL)
    luaL_argerror(L, 1, uj_str_pushf(L, "%s: %s", fname, strerror(errno)));
  return iof;
}

static int io_file_close(lua_State *L, struct IOFileUD *iof)
{
  int ok;
  if ((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_FILE) {
    ok = (fclose(iof->fp) == 0);
  } else if ((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_PIPE) {
    int stat = -1;
    stat = pclose(iof->fp);
    ok = (stat != -1);
  } else {
    lua_assert((iof->type & IOFILE_TYPE_MASK) == IOFILE_TYPE_STDF);
    setnilV(L->top++);
    lua_pushliteral(L, "cannot close standard file");
    return 2;
  }
  iof->fp = NULL;
  return luaL_fileresult(L, ok, NULL);
}

/* -- Read/write helpers -------------------------------------------------- */

static int io_file_readnum(lua_State *L, FILE *fp)
{
  lua_Number d;
  if (fscanf(fp, LUA_NUMBER_SCAN, &d) == 1) {
    setnumV(L->top++, d);
    return 1;
  } else {
    setnilV(L->top++);
    return 0;
  }
}

static int io_file_readline(lua_State *L, FILE *fp, size_t chop)
{
  size_t m = LUAL_BUFFERSIZE, n = 0, ok = 0;
  char *buf;
  for (;;) {
    buf = uj_sbuf_tmp_bytes(L, m);
    if (fgets(buf+n, m-n, fp) == NULL) break;
    n += strlen(buf+n);
    ok |= n;
    if (n && buf[n-1] == '\n') { n -= chop; break; }
    if (n >= m - 64) m += m;
  }
  setstrV(L, L->top++, uj_str_new(L, buf, (size_t)n));
  lj_gc_check(L);
  return (int)ok;
}

static void io_file_readall(lua_State *L, FILE *fp)
{
  size_t m, n;
  for (m = LUAL_BUFFERSIZE, n = 0; ; m += m) {
    char *buf = uj_sbuf_tmp_bytes(L, m);
    n += fread(buf+n, 1, m-n, fp);
    if (n != m) {
      setstrV(L, L->top++, uj_str_new(L, buf, (size_t)n));
      lj_gc_check(L);
      return;
    }
  }
}

static int io_file_readlen(lua_State *L, FILE *fp, size_t m)
{
  if (m) {
    char *buf = uj_sbuf_tmp_bytes(L, m);
    size_t n = fread(buf, 1, m, fp);
    setstrV(L, L->top++, uj_str_new(L, buf, (size_t)n));
    lj_gc_check(L);
    return (n > 0 || m == 0);
  } else {
    int c = getc(fp);
    ungetc(c, fp);
    setstrV(L, L->top++, G(L)->strempty);
    return (c != EOF);
  }
}

static int io_file_read(lua_State *L, FILE *fp, int start)
{
  int ok, n, nargs = (int)(L->top - L->base) - start;
  clearerr(fp);
  if (nargs == 0) {
    ok = io_file_readline(L, fp, 1);
    n = start+1;  /* Return 1 result. */
  } else {
    /* The results plus the buffers go on top of the args. */
    luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
    ok = 1;
    for (n = start; nargs-- && ok; n++) {
      if (tvisstr(L->base+n)) {
        const char *p = strVdata(L->base+n);
        if (p[0] != '*')
          uj_err_arg(L, UJ_ERR_INVOPT, n+1);
        if (p[1] == 'n')
          ok = io_file_readnum(L, fp);
        else if (lj_char_casecmp(p[1], 'l'))
          ok = io_file_readline(L, fp, (p[1] == 'l'));
        else if (p[1] == 'a')
          io_file_readall(L, fp);
        else
          uj_err_arg(L, UJ_ERR_INVFMT, n+1);
      } else if (tvisnum(L->base+n)) {
        ok = io_file_readlen(L, fp, (size_t)uj_lib_checkint(L, n+1));
      } else {
        uj_err_arg(L, UJ_ERR_INVOPT, n+1);
      }
    }
  }
  if (ferror(fp))
    return luaL_fileresult(L, 0, NULL);
  if (!ok)
    setnilV(L->top-1);  /* Replace last result with nil. */
  return n - start;
}

static int io_file_write(lua_State *L, FILE *fp, int start)
{
  const TValue *tv;
  int status = 1;
  for (tv = L->base+start; tv < L->top; tv++) {
    if (tvisstr(tv)) {
      size_t len = strV(tv)->len;
      status = status && (fwrite(strVdata(tv), 1, len, fp) == len);
    } else if (tvisnum(tv)) {
      status = status && (fprintf(fp, LUA_NUMBER_FMT, numV(tv)) > 0);
    } else {
      uj_err_argt(L, (int)(tv - L->base) + 1, LUA_TSTRING);
    }
  }
  return luaL_fileresult(L, status, NULL);
}

static int io_file_iter(lua_State *L)
{
  GCfunc *fn = curr_func(L);
  struct IOFileUD *iof = uddata(udataV(&fn->c.upvalue[0]));
  int n = fn->c.nupvalues - 1;
  if (iof->fp == NULL)
    uj_err_caller(L, UJ_ERR_IOCLFL);
  L->top = L->base;
  if (n) {  /* Copy upvalues with options to stack. */
    if (n > LUAI_MAXCSTACK)
      uj_err_caller(L, UJ_ERR_STKOV);
    uj_state_stack_check(L, (size_t)n);
    memcpy(L->top, &fn->c.upvalue[1], n*sizeof(TValue));
    L->top += n;
  }
  n = io_file_read(L, iof->fp, 0);
  if (ferror(iof->fp))
    uj_err_msg_caller(L, strVdata(L->top-2));
  if (tvisnil(L->base) && (iof->type & IOFILE_FLAG_CLOSE)) {
    io_file_close(L, iof);  /* Return values are ignored. */
    return 0;
  }
  return n;
}

static int io_file_lines(lua_State *L)
{
  int n = (int)(L->top - L->base);
  if (n > LJ_MAX_UPVAL)
    uj_err_caller(L, UJ_ERR_UNPACK);
  lua_pushcclosure(L, io_file_iter, n);
  return 1;
}

/* -- I/O file methods ---------------------------------------------------- */

#define LJLIB_MODULE_io_method

LJLIB_CF(io_method_close)
{
  struct IOFileUD *iof = L->base < L->top ? io_tofile(L) :
                  IOSTDF_IOF(L, GCROOT_IO_OUTPUT);
  return io_file_close(L, iof);
}

LJLIB_CF(io_method_read)
{
  return io_file_read(L, io_tofile(L)->fp, 1);
}

LJLIB_CF(io_method_write)
{
  return io_file_write(L, io_tofile(L)->fp, 1);
}

LJLIB_CF(io_method_flush)
{
  return luaL_fileresult(L, fflush(io_tofile(L)->fp) == 0, NULL);
}

LJLIB_CF(io_method_seek)
{
  FILE *fp = io_tofile(L)->fp;
  int opt = uj_lib_checkopt(L, 2, 1, "\3set\3cur\3end");
  int64_t ofs = 0;
  const TValue *o;
  int res;
  if (opt == 0) opt = SEEK_SET;
  else if (opt == 1) opt = SEEK_CUR;
  else if (opt == 2) opt = SEEK_END;
  o = L->base+2;
  if (o < L->top) {
    if (tvisnum(o))
      ofs = (int64_t)numV(o);
    else if (!tvisnil(o))
      uj_err_argt(L, 3, LUA_TNUMBER);
  }
  res = fseeko(fp, ofs, opt);
  if (res)
    return luaL_fileresult(L, 0, NULL);
  ofs = ftello(fp);
  setintV(L->top-1, ofs);
  return 1;
}

LJLIB_CF(io_method_setvbuf)
{
  FILE *fp = io_tofile(L)->fp;
  int opt = uj_lib_checkopt(L, 2, -1, "\4full\4line\2no");
  size_t sz = (size_t)uj_lib_optint(L, 3, LUAL_BUFFERSIZE);
  if (opt == 0) opt = _IOFBF;
  else if (opt == 1) opt = _IOLBF;
  else if (opt == 2) opt = _IONBF;
  return luaL_fileresult(L, setvbuf(fp, NULL, opt, sz) == 0, NULL);
}

LJLIB_CF(io_method_lines)
{
  io_tofile(L);
  return io_file_lines(L);
}

LJLIB_CF(io_method___gc)
{
  struct IOFileUD *iof = io_tofilep(L);
  if (iof->fp != NULL && (iof->type & IOFILE_TYPE_MASK) != IOFILE_TYPE_STDF)
    io_file_close(L, iof);
  return 0;
}

LJLIB_CF(io_method___tostring)
{
  struct IOFileUD *iof = io_tofilep(L);
  if (iof->fp != NULL)
    lua_pushfstring(L, "file (%p)", iof->fp);
  else
    lua_pushliteral(L, "file (closed)");
  return 1;
}

LJLIB_PUSH(top-1) LJLIB_SET(__index)

#include "lj_libdef.h"

/* -- I/O library functions ----------------------------------------------- */

#define LJLIB_MODULE_io

LJLIB_PUSH(top-2) LJLIB_SET(!)  /* Set environment. */

LJLIB_CF(io_open)
{
  const char *fname = strdata(uj_lib_checkstr(L, 1));
  GCstr *s = uj_lib_optstr(L, 2);
  const char *mode = s ? strdata(s) : "r";
  struct IOFileUD *iof = io_file_new(L);
  iof->fp = fopen(fname, mode);
  return iof->fp != NULL ? 1 : luaL_fileresult(L, 0, fname);
}

LJLIB_CF(io_popen)
{
  const char *fname = strdata(uj_lib_checkstr(L, 1));
  GCstr *s = uj_lib_optstr(L, 2);
  const char *mode = s ? strdata(s) : "r";
  struct IOFileUD *iof = io_file_new(L);
  iof->type = IOFILE_TYPE_PIPE;
  fflush(NULL);
  iof->fp = popen(fname, mode);
  return iof->fp != NULL ? 1 : luaL_fileresult(L, 0, fname);
}

LJLIB_CF(io_tmpfile)
{
  struct IOFileUD *iof = io_file_new(L);
  iof->fp = tmpfile();
  return iof->fp != NULL ? 1 : luaL_fileresult(L, 0, NULL);
}

LJLIB_CF(io_close)
{
  return lj_cf_io_method_close(L);
}

LJLIB_CF(io_read)
{
  return io_file_read(L, io_stdfile(L, GCROOT_IO_INPUT), 0);
}

LJLIB_CF(io_write)
{
  return io_file_write(L, io_stdfile(L, GCROOT_IO_OUTPUT), 0);
}

LJLIB_CF(io_flush)
{
  return luaL_fileresult(L, fflush(io_stdfile(L, GCROOT_IO_OUTPUT)) == 0, NULL);
}

static int io_std_getset(lua_State *L, ptrdiff_t id, const char *mode)
{
  if (L->base < L->top && !tvisnil(L->base)) {
    if (tvisudata(L->base)) {
      io_tofile(L);
      L->top = L->base+1;
    } else {
      io_file_open(L, mode);
    }
    /* NOBARRIER: The standard I/O handles are GC roots. */
    G(L)->gcroot[id] = gcV(L->top-1);
  } else {
    setudataV(L, L->top++, IOSTDF_UD(L, id));
  }
  return 1;
}

LJLIB_CF(io_input)
{
  return io_std_getset(L, GCROOT_IO_INPUT, "r");
}

LJLIB_CF(io_output)
{
  return io_std_getset(L, GCROOT_IO_OUTPUT, "w");
}

LJLIB_CF(io_lines)
{
  if (L->base == L->top) setnilV(L->top++);
  if (!tvisnil(L->base)) {  /* io.lines(fname) */
    struct IOFileUD *iof = io_file_open(L, "r");
    iof->type = IOFILE_TYPE_FILE|IOFILE_FLAG_CLOSE;
    L->top--;
    setudataV(L, L->base, udataV(L->top));
  } else {  /* io.lines() iterates over stdin. */
    setudataV(L, L->base, IOSTDF_UD(L, GCROOT_IO_INPUT));
  }
  return io_file_lines(L);
}

LJLIB_CF(io_type)
{
  const TValue *o = uj_lib_checkany(L, 1);
  if (!(tvisudata(o) && udataV(o)->udtype == UDTYPE_IO_FILE))
    setnilV(L->top++);
  else if (((struct IOFileUD *)uddata(udataV(o)))->fp != NULL)
    lua_pushliteral(L, "file");
  else
    lua_pushliteral(L, "closed file");
  return 1;
}

#include "lj_libdef.h"

/* ------------------------------------------------------------------------ */

static GCobj *io_std_new(lua_State *L, FILE *fp, const char *name)
{
  struct IOFileUD *iof = (struct IOFileUD *)lua_newuserdata(L, sizeof(struct IOFileUD));
  GCudata *ud = udataV(L->top-1);
  ud->udtype = UDTYPE_IO_FILE;
  /* NOBARRIER: The GCudata is new (marked white). */
  ud->metatable = tabV(L->top-3);
  iof->fp = fp;
  iof->type = IOFILE_TYPE_STDF;
  lua_setfield(L, -2, name);
  return obj2gco(ud);
}

LUALIB_API int luaopen_io(lua_State *L)
{
  LJ_LIB_REG(L, NULL, io_method);
  copyTV(L, L->top, L->top-1); L->top++;
  lua_setfield(L, LUA_REGISTRYINDEX, LUA_FILEHANDLE);
  LJ_LIB_REG(L, LUA_IOLIBNAME, io);
  G(L)->gcroot[GCROOT_IO_INPUT] = io_std_new(L, stdin, "stdin");
  G(L)->gcroot[GCROOT_IO_OUTPUT] = io_std_new(L, stdout, "stdout");
  io_std_new(L, stderr, "stderr");
  return 1;
}

