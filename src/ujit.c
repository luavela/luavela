/*
 * uJIT frontend. Runs commands, scripts, read-eval-print (REPL) etc.
 * Copyright (C) 2015-2019 IPONWEB Ltd. See Copyright Notice in COPYRIGHT
 *
 * Portions taken verbatim or adapted from LuaJIT.
 * Copyright (C) 2005-2017 Mike Pall. See Copyright Notice in luajit.h
 *
 * Major portions taken verbatim or adapted from the Lua interpreter.
 * Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lextlib.h"
#include "ujit.h"
#include "uj_proto.h"

#include "lj_def.h"
#include "cli/opt.h"

#include <unistd.h>
#define lua_stdin_is_tty()      isatty(0)

#include <signal.h>

#define STATUS_OK        0 /* Generic OK status */
#define STATUS_ERROR     1 /* Generic error status */
#define STATUS_INNER_ERR 0 /* Inner error happened (used by pmain callback) */

#define STATUS_NO_INPUT (-1) /* Reading stdin: no more input */

#define STATUS_CLI_INVALID  (-1) /* CLI parsing: malformed arguments */
#define STATUS_CLI_NOSCRIPT  0 /* CLI parsing: arguments ok, no script set on the command line */

static lua_State  *globalL  = NULL;
static const char *progname = LUA_PROGNAME;

static void lstop(lua_State *L, lua_Debug *ar) {
  UNUSED(ar);
  lua_sethook(L, NULL, 0, 0);
  /* Avoid luaL_error -- a C hook doesn't add an extra frame. */
  luaL_where(L, 0);
  lua_pushfstring(L, "%sinterrupted!", lua_tostring(L, -1));
  lua_error(L);
}

static void laction(int i) {
  signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
                         terminate process (default action) */
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static void print_usage(void) {
  fprintf(stderr,
  "usage: %s [options]... [script [args]...].\n"
  "Available options are:\n"
  "  -e chunk  Execute string " LUA_QL("chunk") ".\n"
  "  -l name   Require library " LUA_QL("name") ".\n"
  "  -p file   Save compiler progress to " LUA_QL("file") " (- to dump to stdout).\n"
  "  -b file   Save human-readable bytecode to " LUA_QL("file") " (- to dump to stdout).\n"
  "  -B file   Same as -b, but also dumps source code and other extra info.\n"
  "  -j cmd    Perform uJIT control command.\n"
  "  -X k=v    Extended options passed as key-value pairs.\n"
  "            The switch may be repeated several times. Supported options:\n"
  "            * hashf: Hash function. Supported values: murmur (default), city\n"
  "            * itern: Enables ITERN optimization in frontend. Supported values: on (default), off\n"
  "  -O[opt]   Control uJIT optimizations.\n"
  "  -i        Enter interactive mode after executing " LUA_QL("script") ".\n"
  "  -v        Show version information.\n"
  "  -E        Ignore environment variables.\n"
  "  --        Stop handling options.\n"
  "  -         Execute stdin and stop handling options.\n"
  ,
  progname);
  fflush(stderr);
}

static void l_message(const char *pname, const char *msg) {
  if (pname) {
    fprintf(stderr, "%s: ", pname);
  }
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
}

static int report(lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) {
      msg = "(error object is not a string)";
    }
    l_message(progname, msg);
    lua_pop(L, 1);
  }
  return status;
}

/* Retrieves tracebak; uses Lua calling conventions for C API. */
static int traceback(lua_State *L) {
  if (!lua_isstring(L, 1)) { /* Non-string error object? Try metamethod. */
    if (lua_isnoneornil(L, 1)
      || !luaL_callmeta(L, 1, "__tostring")
      || !lua_isstring(L, -1)) {
      return 1; /* Return non-string error object. */
    }
    lua_remove(L, 1);  /* Replace object by result of __tostring metamethod. */
  }
  luaL_traceback(L, L, lua_tostring(L, 1), 1);
  return 1;
}

static int docall(lua_State *L, int narg, int clear) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);              /* put it under chunk and args */
  signal(SIGINT, laction);
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
  signal(SIGINT, SIG_DFL);
  lua_remove(L, base); /* remove traceback function */
  /* force a complete garbage collection in case of errors */
  if (status != 0) {
    lua_gc(L, LUA_GCCOLLECT, 0);
  }
  return status;
}

static void print_version(void) {
  fputs(UJIT_PRODUCT_NAME_VERSION " -- " UJIT_COPYRIGHT " " UJIT_URL "\n", stderr);
}

static void print_jit_status(lua_State *L) {
  int n;
  const char *s;
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit");  /* Get jit.* module table. */
  lua_remove(L, -2);
  lua_getfield(L, -1, "status");
  lua_remove(L, -2);
  n = lua_gettop(L);
  lua_call(L, 0, LUA_MULTRET);
  fputs(lua_toboolean(L, n) ? "JIT: ON" : "JIT: OFF", stderr);
  for (n++; (s = lua_tostring(L, n)); n++) {
    putc(' ', stderr);
    fputs(s, stderr);
  }
  putc('\n', stderr);
}

/* Collects *script* arguments (placed after script name on the command line). */
static int getargs(lua_State *L, char **argv, int n) {
  int narg;
  int i;
  int argc = 0;
  while (argv[argc]) { /* count total number of arguments */
    argc++;
  }
  narg = argc - (n + 1);  /* number of arguments to the script */
  luaL_checkstack(L, narg + 3, "too many arguments to script");
  for (i = n + 1; i < argc; i++) {
    lua_pushstring(L, argv[i]);
  }
  lua_createtable(L, narg, n + 1);
  for (i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i - n);
  }
  return narg;
}

static const char* bc_dump_output;
#define DUMP_CHUNK()            (bc_dump_output != NULL)
#define CHUNK_DUMPED()          (bc_dump_output = NULL)
#define DUMP_CHUNK_TO(filename) (bc_dump_output = (filename))

static int dump_src; /* used for byte code dump */

/* If bytecode of the successfully parsed chunk (located at [top - 1] of
** the Lua stack) should be dumped before execution -- do this.
*/
static int handle_dump_bc(lua_State *L) {
  FILE* dump_file;

  if (!DUMP_CHUNK()) {
    return STATUS_OK;
  }

  if (strcmp(bc_dump_output, "-") != 0) {
    dump_file = fopen(bc_dump_output, "w");
  } else {
    dump_file = stdout;
  }

  CHUNK_DUMPED();
  if (dump_file == NULL) {
    return STATUS_ERROR;
  }

  if (dump_src)
    luaE_dumpbcsource(L, -1, dump_file, -1);
  else
    luaE_dumpbc(L, -1, dump_file);

  if (dump_file != stdout) {
    fclose(dump_file);
  }
  return STATUS_OK;
}

/* Execute any preprocessing over the parsed chunk and execute it */
static int preprocess_and_call(lua_State *L, int narg, int clear) {
  if (handle_dump_bc(L) != STATUS_OK) {
    return STATUS_ERROR;
  }

  return docall(L, narg, clear);
}

static int dofile(lua_State *L, const char *name) {
  int status = luaL_loadfile(L, name) || docall(L, 0, 1);
  return report(L, status);
}

static int dostring(lua_State *L, const char *s, const char *name) {
  int status = luaL_loadbuffer(L, s, strlen(s), name) || preprocess_and_call(L, 0, 1);
  return report(L, status);
}

static int dolibrary(lua_State *L, const char *name) {
  lua_getglobal(L, "require");
  lua_pushstring(L, name);
  return report(L, docall(L, 1, 1));
}

static void write_prompt(lua_State *L, int firstline) {
  const char *p;
  lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  if (p == NULL) {
    p = firstline ? LUA_PROMPT : LUA_PROMPT2;
  }
  fputs(p, stdout);
  fflush(stdout);
  lua_pop(L, 1);  /* remove global */
}

static int incomplete(lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    const char *tp  = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
    if (strstr(msg, LUA_QL("<eof>")) == tp) {
      lua_pop(L, 1);
      return 1;
    }
  }
  return 0;  /* else... */
}

static int pushline(lua_State *L, int firstline) {
  char buf[LUA_MAXINPUT];
  write_prompt(L, firstline);
  if (fgets(buf, LUA_MAXINPUT, stdin)) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
      buf[len - 1] = '\0';
    }
    if (firstline && buf[0] == '=') {
      lua_pushfstring(L, "return %s", buf+1);
    } else {
      lua_pushstring(L, buf);
    }
    return 1;
  }
  return 0;
}

static int loadline(lua_State *L) {
  int status;
  lua_settop(L, 0);
  if (!pushline(L, 1)) {
    return STATUS_NO_INPUT;
  }
  for (;;) {  /* repeat until gets a complete line */
    status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
    if (!incomplete(L, status)) { /* cannot try to add lines? */
      break;
    }
    if (!pushline(L, 0)) { /* no more input? */
      return STATUS_NO_INPUT;
    }
    lua_pushliteral(L, "\n");  /* add a new line... */
    lua_insert(L, -2);         /* ...between the two lines */
    lua_concat(L,  3);         /* join them */
  }
  lua_remove(L, 1); /* remove line */
  return status;
}

static void dotty(lua_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;
  while ((status = loadline(L)) != STATUS_NO_INPUT) {
    if (status == 0) {
      status = docall(L, 0, 0);
    }
    report(L, status);
    if (status == 0 && lua_gettop(L) > 0) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      if (lua_pcall(L, lua_gettop(L)-1, 0, 0) != 0) {
        l_message(progname, lua_pushfstring(
          L, "error calling " LUA_QL("print") " (%s)", lua_tostring(L, -1)
        ));
      }
    }
  }
  lua_settop(L, 0);  /* clear stack */
  fputs("\n", stdout);
  fflush(stdout);
  progname = oldprogname;
}

/* Runs a chunk from a file (passing arguments, if any). */
static int handle_script(lua_State *L, char **argv, int n) {
  int status;
  const char *fname;
  int narg = getargs(L, argv, n);  /* collect arguments */
  lua_setglobal(L, "arg");
  fname = argv[n];
  if (strcmp(fname, "-") == 0 && strcmp(argv[n - 1], "--") != 0) {
    fname = NULL;  /* stdin */
  }
  status = luaL_loadfile(L, fname);
  lua_insert(L, -(narg+1));
  if (status == STATUS_OK) {
    status = preprocess_and_call(L, narg, 0);
  } else {
    lua_pop(L, narg);
  }
  return report(L, status);
}

/* Load add-on module. */
static int loadjitmodule(lua_State *L) {
  lua_getglobal  (L, "require");
  lua_pushliteral(L, "jit.");
  lua_pushvalue  (L, -3);
  lua_concat     (L, 2);
  if (lua_pcall(L, 1, 1, 0)) {
    const char *msg = lua_tostring(L, -1);
    if (msg && !strncmp(msg, "module ", 7)) {
    err:
      l_message(progname, "unknown luaJIT command or jit.* modules not installed");
      return STATUS_ERROR;
    } else {
      return report(L, 1);
    }
  }
  lua_getfield(L, -1, "start");
  if (lua_isnil(L, -1)) {
    goto err;
  }
  lua_remove(L, -2);  /* Drop module table. */
  return STATUS_OK;
}

/* Run command with options. */
static int runcmdopt(lua_State *L, const char *opt) {
  int narg = 0;
  if (opt && *opt) {
    for (;;) {  /* Split arguments. */
      const char *p = strchr(opt, ',');
      narg++;
      if (!p) {
        break;
      }
      if (p == opt) {
        lua_pushnil(L);
      } else {
        lua_pushlstring(L, opt, (size_t)(p - opt));
      }
      opt = p + 1;
    }
    if (*opt) {
      lua_pushstring(L, opt);
    } else {
      lua_pushnil(L);
    }
  }
  return report(L, lua_pcall(L, narg, 0, 0));
}

/* JIT engine control command: try jit library first or load add-on module. */
static int dojitcmd(lua_State *L, const char *cmd) {
  const char *opt = strchr(cmd, '=');
  lua_pushlstring(L, cmd, opt ? (size_t)(opt - cmd) : strlen(cmd));
  lua_getfield   (L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield   (L, -1, "jit");  /* Get jit.* module table. */
  lua_remove     (L, -2);
  lua_pushvalue  (L, -2);
  lua_gettable   (L, -2);  /* Lookup library function. */
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);  /* Drop non-function and jit.* table, keep module name. */
    if (loadjitmodule(L)) {
      return STATUS_ERROR;
    }
  } else {
    lua_remove(L, -2); /* Drop jit.* table. */
  }
  lua_remove(L, -2);   /* Drop module name. */
  return runcmdopt(L, opt ? opt + 1 : opt);
}

/* Optimization flags. */
static int dojitopt(lua_State *L, const char *opt) {
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit.opt");  /* Get jit.opt.* module table. */
  lua_remove  (L, -2);
  lua_getfield(L, -1, "start");
  lua_remove  (L, -2);
  return runcmdopt(L, opt);
}

/* check that argument has no extra characters at the end */
#define notail(x)       {if ((x)[2] != '\0') { return STATUS_CLI_INVALID; }}

#define FLAGS_INTERACTIVE       1
#define FLAGS_VERSION           2
#define FLAGS_EXEC              4
#define FLAGS_NOENV             16

/* Walks through command line arguments, maps options into flags and tries to
** determine script file position. If some options imply arguments (-k value),
** "values" are skipped over. Return values:
** value  < 0 : Malformed command-line arguments.
** value == 0 : Command-line arguments parsed ok, no script file specified.
** value  > 0 : Command-line arguments parsed ok, script file is in argv[value].
*/
static int collectargs(char **argv, int *flags) {
  int i;
  for (i = 1; argv[i] != NULL; i++) {
    if (argv[i][0] != '-') { /* Not an option? */
      return i;
    }

    switch (argv[i][1]) { /* Check option. */

    /* Stock Lua: -- */
    case '-': {
      notail(argv[i]);
      return (argv[i + 1] != NULL ? i + 1 : STATUS_CLI_NOSCRIPT);
    }

    /* Stock Lua: - */
    case '\0': {
      return i;
    }

    /* Stock Lua: -i */
    case 'i': {
      notail(argv[i]);
      *flags |= FLAGS_INTERACTIVE;
      /* fallthrough: -i implies -v */
    }

    /* Stock Lua: -v */
    case 'v': {
      notail(argv[i]);
      *flags |= FLAGS_VERSION;
      break;
    }

    /* Stock Lua: -e chunk */
    case 'e': {
      *flags |= FLAGS_EXEC;
      /* fallthrough: -e implies a required argument */
    }

    /*
     * uJIT extension: -b filename
     * Space between "-b" and "filename" is optional,
     * but "filename" is required. Fallthrough because argument is implied.
     */
    case 'b':

    /*
     * uJIT extension: -B filename
     * Space between "-B" and "filename" is optional,
     * but "filename" is required. Fallthrough because argument is implied.
     */
    case 'B':

    /*
     * uJIT extension: -p filename
     * Space between "-p" and "filename" is optional,
     * but "filename" is required. Fallthrough because argument is implied.
     */
    case 'p':

    /*
     * uJIT extension: -j cmd
     * Space between "-j" and "cmd" is optional,
     * but "cmd" is required. Fallthrough because argument is implied.
     */
    case 'j':

    /*
     * -l module
     * Space between "-l" and "module" is optional, but "module" is required.
     */
    case 'l': {
      /* Common code for all switches that imply a required argument. */
      if (argv[i][2] == '\0') {
        i++;
        if (argv[i] == NULL) {
          return STATUS_CLI_INVALID;
        }
      }
      break;
    }

    /* uJIT extension: -X key=value [...], already processed */
    case 'X': {
      if (argv[i][2] == '\0')
        i++;
      break;
    }

    /*
     * uJIT extension: -Oparameters
     * Spaces between "-O" and "parameters" are not allowed.
     */
    case 'O': {
      break;
    }

    /* Stock Lua (v5.2+!): -E */
    case 'E': {
      *flags |= FLAGS_NOENV;
      break;
    }

    default: {
      return STATUS_CLI_INVALID; /* invalid option */
    }
    }
  }
  return STATUS_CLI_NOSCRIPT;
}

/*
 * Perform a dedicated pass over the command line arguments to read all
 * -X options, which must be processed *before* creating a VM instance.
 */
static int collectoptions(char **argv, struct luae_Options *opt,
                          char *buffer, size_t n) {
  int i;

  for (i = 1; argv[i] != NULL; i++) {
    if (argv[i][0] != '-') { /* Not an option. */
      continue;
    }
    if (argv[i][1] == '-' || argv[i][1] == '\0') { /* Stop handling options. */
      break;
    }
    if (argv[i][1] != 'X') { /* Regular option, will be processed later. */
      continue;
    }

    if (argv[i][2] == '\0') {
      i++;
    }
    if (cli_opt_parse_kv(argv[i], opt, buffer, n) != OPT_PARSE_OK) {
      lua_assert(strlen(buffer) > 0);
      return STATUS_ERROR;
    }
  }

  return STATUS_OK;
}

/* Performs all actions required by CLI arguments. */
static int runargs(lua_State *L, char **argv, int n, int flags) {
  int i;
  for (i = 1; i < n; i++) {
    if (argv[i] == NULL) {
      continue;
    }
    /* lua_assert(argv[i][0] == '-'); */
    switch (argv[i][1]) { /* option */
    case 'e': {
      const char *chunk = argv[i] + 2;
      if (*chunk == '\0') {
        chunk = argv[++i];
      }
      /* lua_assert(chunk != NULL); */
      if (dostring(L, chunk, UJ_CMD_CHUNKNAME) != 0) {
        return STATUS_ERROR;
      }
      break;
    }
    case 'l': {
      const char *filename = argv[i] + 2;
      if (*filename == '\0') {
        filename = argv[++i];
      }
      /* lua_assert(filename != NULL); */
      if (dolibrary(L, filename)) {
        return STATUS_ERROR;  /* stop if file fails */
      }
      break;
    }
    case 'b': /* fallthrough */
    case 'B' :{ /* uJIT extension */
      const char *filename = argv[i] + 2;
      if (*filename == '\0') {
        filename = argv[++i];
      }

      dump_src = (argv[i][1] == 'B');
      if (dump_src && (flags & FLAGS_EXEC)) {
          fprintf(stderr, "warning: source will not be printed in a dump"
                  " (command line chunk was passed)\n");
          dump_src = 0;
      }

      /* lua_assert(filename != NULL); */
      DUMP_CHUNK_TO(filename);
      break;
    }
    case 'p': { /* uJIT extension */
      const char *filename = argv[i] + 2;
      if (*filename == '\0') {
        filename = argv[++i];
      }
      /* lua_assert(filename != NULL); */
      FILE *out = stdout;
      if (strcmp(filename, "-") != 0) {
        out = fopen(filename, "w");
        if (out == NULL) {
          return STATUS_ERROR;
        }
      }
      if (luaE_dumpstart(L, out) != 0) {
        return STATUS_ERROR;
      }
      break;
    }
    case 'j': {  /* uJIT extension */
      const char *cmd = argv[i] + 2;
      if (*cmd == '\0') {
        cmd = argv[++i];
      }
      /* lua_assert(cmd != NULL); */
      if (dojitcmd(L, cmd)) {
        return STATUS_ERROR;
      }
      break;
    }
    case 'O': { /* uJIT extension */
      if (dojitopt(L, argv[i] + 2)) {
        return STATUS_ERROR;
      }
      break;
    }
    case 'X': { /* uJIT extension, already processed . */
      if (argv[i][2] == '\0')
        i++;
      break;
    }
    default: {
      break;
    }
    }
  }
  return STATUS_OK;
}

/* Execute LUA_INIT, if it is set (as a file or an inline chunk). */
static int handle_luainit(lua_State *L) {
  const char *init = getenv(LUA_INIT);
  if (init == NULL) {
    return STATUS_OK;
  } else if (init[0] == '@') {
    return dofile(L, init + 1);
  } else {
    return dostring(L, init, "=" LUA_INIT);
  }
}

static struct Smain {
  char **argv;
  int argc;
  int status;
} smain;

/* Main protected callback: actually executes all the work. */
static int pmain(lua_State *L) {
  struct Smain *s = &smain;
  char **argv = s->argv;
  int script;
  int flags = 0;
  globalL = L;

  if (argv[0] && argv[0][0]) {
    progname = argv[0];
  }

  script = collectargs(argv, &flags);
  if (script == STATUS_CLI_INVALID) { /* invalid args? */
    print_usage();
    s->status = STATUS_ERROR;
    return STATUS_INNER_ERR;
  }

  if (flags & FLAGS_NOENV) {
    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
  }

  lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
  luaL_openlibs(L);         /* open libraries */
  lua_gc(L, LUA_GCRESTART, -1);

  if (!(flags & FLAGS_NOENV)) {
    s->status = handle_luainit(L);
    if (s->status != STATUS_OK) {
      return STATUS_INNER_ERR;
    }
  }

  if ((flags & FLAGS_VERSION)) {
    print_version();
  }

  s->status = runargs(L, argv, (script > STATUS_CLI_NOSCRIPT) ? script : s->argc, flags);
  if (s->status != STATUS_OK) {
    return STATUS_INNER_ERR;
  }

  if (script) {
    s->status = handle_script(L, argv, script);
    if (s->status != STATUS_OK) {
      return STATUS_INNER_ERR;
    }
  }

  if (flags & FLAGS_INTERACTIVE) {
    print_jit_status(L);
    dotty(L);
  } else if (script == 0 && !(flags & (FLAGS_EXEC|FLAGS_VERSION))) {
    if (lua_stdin_is_tty()) {
      print_version();
      print_jit_status(L);
      dotty(L);
    } else {
      dofile(L, NULL); /* executes stdin as a file */
    }
  }

  return STATUS_OK;
}

int main(int argc, char **argv) {
  char opt_error[128] = {0};
  struct luae_Options opt = {0};
  int status;

  if (collectoptions(argv, &opt, opt_error, sizeof(opt_error)) != STATUS_OK) {
    l_message(progname, opt_error);
    print_usage();
    return EXIT_FAILURE;
  }

  lua_State *L = luaE_createstate(&opt);
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  smain.argc = argc;
  smain.argv = argv;
  status     = lua_cpcall(L, pmain, NULL);
  report(L, status);
  luaE_dumpstop(L); /* Exit code can safely be ignored here. */
  lua_close(L);
  return (status || smain.status) ? EXIT_FAILURE : EXIT_SUCCESS;
}
