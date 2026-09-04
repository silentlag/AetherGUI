
#define liolib_c
#define LUA_LIB

#include "lprefix.h"

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#if !defined(l_checkmode)

#if !defined(L_MODEEXT)
#define L_MODEEXT	"b"
#endif

static int l_checkmode (const char *mode) {
  return (*mode != '\0' && strchr("rwa", *(mode++)) != NULL &&
         (*mode != '+' || ((void)(++mode), 1)) &&
         (strspn(mode, L_MODEEXT) == strlen(mode)));
}

#endif

#if !defined(l_popen)

#if defined(LUA_USE_POSIX)

#define l_popen(L,c,m)		(fflush(NULL), popen(c,m))
#define l_pclose(L,file)	(pclose(file))

#elif defined(LUA_USE_WINDOWS)

#define l_popen(L,c,m)		(_popen(c,m))
#define l_pclose(L,file)	(_pclose(file))

#if !defined(l_checkmodep)

#define l_checkmodep(m)	((m[0] == 'r' || m[0] == 'w') && \
  (m[1] == '\0' || ((m[1] == 'b' || m[1] == 't') && m[2] == '\0')))
#endif

#else

#define l_popen(L,c,m)  \
	  ((void)c, (void)m, \
	  luaL_error(L, "'popen' not supported"), \
	  (FILE*)0)
#define l_pclose(L,file)		((void)L, (void)file, -1)

#endif

#endif

#if !defined(l_checkmodep)

#define l_checkmodep(m)        ((m[0] == 'r' || m[0] == 'w') && m[1] == '\0')
#endif

#if !defined(l_getc)

#if defined(LUA_USE_POSIX)
#define l_getc(f)		getc_unlocked(f)
#define l_lockfile(f)		flockfile(f)
#define l_unlockfile(f)		funlockfile(f)
#else
#define l_getc(f)		getc(f)
#define l_lockfile(f)		((void)0)
#define l_unlockfile(f)		((void)0)
#endif

#endif

#if !defined(l_fseek)

#if defined(LUA_USE_POSIX)

#include <sys/types.h>

#define l_fseek(f,o,w)		fseeko(f,o,w)
#define l_ftell(f)		ftello(f)
#define l_seeknum		off_t

#elif defined(LUA_USE_WINDOWS) && !defined(_CRTIMP_TYPEINFO) \
   && defined(_MSC_VER) && (_MSC_VER >= 1400)

#define l_fseek(f,o,w)		_fseeki64(f,o,w)
#define l_ftell(f)		_ftelli64(f)
#define l_seeknum		__int64

#else

#define l_fseek(f,o,w)		fseek(f,o,w)
#define l_ftell(f)		ftell(f)
#define l_seeknum		long

#endif

#endif

#define IO_PREFIX	"_IO_"
#define IOPREF_LEN	(sizeof(IO_PREFIX)/sizeof(char) - 1)
#define IO_INPUT	(IO_PREFIX "input")
#define IO_OUTPUT	(IO_PREFIX "output")

typedef luaL_Stream LStream;

#define tolstream(L)	((LStream *)luaL_checkudata(L, 1, LUA_FILEHANDLE))

#define isclosed(p)	((p)->closef == NULL)

static int io_type (lua_State *L) {
  LStream *p;
  luaL_checkany(L, 1);
  p = (LStream *)luaL_testudata(L, 1, LUA_FILEHANDLE);
  if (p == NULL)
    luaL_pushfail(L);
  else if (isclosed(p))
    lua_pushliteral(L, "closed file");
  else
    lua_pushliteral(L, "file");
  return 1;
}

static int f_tostring (lua_State *L) {
  LStream *p = tolstream(L);
  if (isclosed(p))
    lua_pushliteral(L, "file (closed)");
  else
    lua_pushfstring(L, "file (%p)", p->f);
  return 1;
}

static FILE *tofile (lua_State *L) {
  LStream *p = tolstream(L);
  if (l_unlikely(isclosed(p)))
    luaL_error(L, "attempt to use a closed file");
  lua_assert(p->f);
  return p->f;
}

static LStream *newprefile (lua_State *L) {
  LStream *p = (LStream *)lua_newuserdatauv(L, sizeof(LStream), 0);
  p->closef = NULL;
  luaL_setmetatable(L, LUA_FILEHANDLE);
  return p;
}

static int aux_close (lua_State *L) {
  LStream *p = tolstream(L);
  volatile lua_CFunction cf = p->closef;
  p->closef = NULL;
  return (*cf)(L);
}

static int f_close (lua_State *L) {
  tofile(L);
  return aux_close(L);
}

static int io_close (lua_State *L) {
  if (lua_isnone(L, 1))
    lua_getfield(L, LUA_REGISTRYINDEX, IO_OUTPUT);
  return f_close(L);
}

static int f_gc (lua_State *L) {
  LStream *p = tolstream(L);
  if (!isclosed(p) && p->f != NULL)
    aux_close(L);
  return 0;
}

static int io_fclose (lua_State *L) {
  LStream *p = tolstream(L);
  errno = 0;
  return luaL_fileresult(L, (fclose(p->f) == 0), NULL);
}

static LStream *newfile (lua_State *L) {
  LStream *p = newprefile(L);
  p->f = NULL;
  p->closef = &io_fclose;
  return p;
}

static void opencheck (lua_State *L, const char *fname, const char *mode) {
  LStream *p = newfile(L);
  p->f = fopen(fname, mode);
  if (l_unlikely(p->f == NULL))
    luaL_error(L, "cannot open file '%s' (%s)", fname, strerror(errno));
}

static int io_open (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *mode = luaL_optstring(L, 2, "r");
  LStream *p = newfile(L);
  const char *md = mode;
  luaL_argcheck(L, l_checkmode(md), 2, "invalid mode");
  errno = 0;
  p->f = fopen(filename, mode);
  return (p->f == NULL) ? luaL_fileresult(L, 0, filename) : 1;
}

static int io_pclose (lua_State *L) {
  LStream *p = tolstream(L);
  errno = 0;
  return luaL_execresult(L, l_pclose(L, p->f));
}

static int io_popen (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *mode = luaL_optstring(L, 2, "r");
  LStream *p = newprefile(L);
  luaL_argcheck(L, l_checkmodep(mode), 2, "invalid mode");
  errno = 0;
  p->f = l_popen(L, filename, mode);
  p->closef = &io_pclose;
  return (p->f == NULL) ? luaL_fileresult(L, 0, filename) : 1;
}

static int io_tmpfile (lua_State *L) {
  LStream *p = newfile(L);
  errno = 0;
  p->f = tmpfile();
  return (p->f == NULL) ? luaL_fileresult(L, 0, NULL) : 1;
}

static FILE *getiofile (lua_State *L, const char *findex) {
  LStream *p;
  lua_getfield(L, LUA_REGISTRYINDEX, findex);
  p = (LStream *)lua_touserdata(L, -1);
  if (l_unlikely(isclosed(p)))
    luaL_error(L, "default %s file is closed", findex + IOPREF_LEN);
  return p->f;
}

static int g_iofile (lua_State *L, const char *f, const char *mode) {
  if (!lua_isnoneornil(L, 1)) {
    const char *filename = lua_tostring(L, 1);
    if (filename)
      opencheck(L, filename, mode);
    else {
      tofile(L);
      lua_pushvalue(L, 1);
    }
    lua_setfield(L, LUA_REGISTRYINDEX, f);
  }

  lua_getfield(L, LUA_REGISTRYINDEX, f);
  return 1;
}

static int io_input (lua_State *L) {
  return g_iofile(L, IO_INPUT, "r");
}

static int io_output (lua_State *L) {
  return g_iofile(L, IO_OUTPUT, "w");
}

static int io_readline (lua_State *L);

#define MAXARGLINE	250

static void aux_lines (lua_State *L, int toclose) {
  int n = lua_gettop(L) - 1;
  luaL_argcheck(L, n <= MAXARGLINE, MAXARGLINE + 2, "too many arguments");
  lua_pushvalue(L, 1);
  lua_pushinteger(L, n);
  lua_pushboolean(L, toclose);
  lua_rotate(L, 2, 3);
  lua_pushcclosure(L, io_readline, 3 + n);
}

static int f_lines (lua_State *L) {
  tofile(L);
  aux_lines(L, 0);
  return 1;
}

static int io_lines (lua_State *L) {
  int toclose;
  if (lua_isnone(L, 1)) lua_pushnil(L);
  if (lua_isnil(L, 1)) {
    lua_getfield(L, LUA_REGISTRYINDEX, IO_INPUT);
    lua_replace(L, 1);
    tofile(L);
    toclose = 0;
  }
  else {
    const char *filename = luaL_checkstring(L, 1);
    opencheck(L, filename, "r");
    lua_replace(L, 1);
    toclose = 1;
  }
  aux_lines(L, toclose);
  if (toclose) {
    lua_pushnil(L);
    lua_pushnil(L);
    lua_pushvalue(L, 1);
    return 4;
  }
  else
    return 1;
}

#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM     200
#endif

typedef struct {
  FILE *f;
  int c;
  int n;
  char buff[L_MAXLENNUM + 1];
} RN;

static int nextc (RN *rn) {
  if (l_unlikely(rn->n >= L_MAXLENNUM)) {
    rn->buff[0] = '\0';
    return 0;
  }
  else {
    rn->buff[rn->n++] = rn->c;
    rn->c = l_getc(rn->f);
    return 1;
  }
}

static int test2 (RN *rn, const char *set) {
  if (rn->c == set[0] || rn->c == set[1])
    return nextc(rn);
  else return 0;
}

static int readdigits (RN *rn, int hex) {
  int count = 0;
  while ((hex ? isxdigit(rn->c) : isdigit(rn->c)) && nextc(rn))
    count++;
  return count;
}

static int read_number (lua_State *L, FILE *f) {
  RN rn;
  int count = 0;
  int hex = 0;
  char decp[2];
  rn.f = f; rn.n = 0;
  decp[0] = lua_getlocaledecpoint();
  decp[1] = '.';
  l_lockfile(rn.f);
  do { rn.c = l_getc(rn.f); } while (isspace(rn.c));
  test2(&rn, "-+");
  if (test2(&rn, "00")) {
    if (test2(&rn, "xX")) hex = 1;
    else count = 1;
  }
  count += readdigits(&rn, hex);
  if (test2(&rn, decp))
    count += readdigits(&rn, hex);
  if (count > 0 && test2(&rn, (hex ? "pP" : "eE"))) {
    test2(&rn, "-+");
    readdigits(&rn, 0);
  }
  ungetc(rn.c, rn.f);
  l_unlockfile(rn.f);
  rn.buff[rn.n] = '\0';
  if (l_likely(lua_stringtonumber(L, rn.buff)))
    return 1;
  else {
   lua_pushnil(L);
   return 0;
  }
}

static int test_eof (lua_State *L, FILE *f) {
  int c = getc(f);
  ungetc(c, f);
  lua_pushliteral(L, "");
  return (c != EOF);
}

static int read_line (lua_State *L, FILE *f, int chop) {
  luaL_Buffer b;
  int c;
  luaL_buffinit(L, &b);
  do {
    char *buff = luaL_prepbuffer(&b);
    int i = 0;
    l_lockfile(f);
    while (i < LUAL_BUFFERSIZE && (c = l_getc(f)) != EOF && c != '\n')
      buff[i++] = c;
    l_unlockfile(f);
    luaL_addsize(&b, i);
  } while (c != EOF && c != '\n');
  if (!chop && c == '\n')
    luaL_addchar(&b, c);
  luaL_pushresult(&b);

  return (c == '\n' || lua_rawlen(L, -1) > 0);
}

static void read_all (lua_State *L, FILE *f) {
  size_t nr;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  do {
    char *p = luaL_prepbuffer(&b);
    nr = fread(p, sizeof(char), LUAL_BUFFERSIZE, f);
    luaL_addsize(&b, nr);
  } while (nr == LUAL_BUFFERSIZE);
  luaL_pushresult(&b);
}

static int read_chars (lua_State *L, FILE *f, size_t n) {
  size_t nr;
  char *p;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  p = luaL_prepbuffsize(&b, n);
  nr = fread(p, sizeof(char), n, f);
  luaL_addsize(&b, nr);
  luaL_pushresult(&b);
  return (nr > 0);
}

static int g_read (lua_State *L, FILE *f, int first) {
  int nargs = lua_gettop(L) - 1;
  int n, success;
  clearerr(f);
  errno = 0;
  if (nargs == 0) {
    success = read_line(L, f, 1);
    n = first + 1;
  }
  else {

    luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)luaL_checkinteger(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = luaL_checkstring(L, n);
        if (*p == '*') p++;
        switch (*p) {
          case 'n':
            success = read_number(L, f);
            break;
          case 'l':
            success = read_line(L, f, 1);
            break;
          case 'L':
            success = read_line(L, f, 0);
            break;
          case 'a':
            read_all(L, f);
            success = 1;
            break;
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (ferror(f))
    return luaL_fileresult(L, 0, NULL);
  if (!success) {
    lua_pop(L, 1);
    luaL_pushfail(L);
  }
  return n - first;
}

static int io_read (lua_State *L) {
  return g_read(L, getiofile(L, IO_INPUT), 1);
}

static int f_read (lua_State *L) {
  return g_read(L, tofile(L), 2);
}

static int io_readline (lua_State *L) {
  LStream *p = (LStream *)lua_touserdata(L, lua_upvalueindex(1));
  int i;
  int n = (int)lua_tointeger(L, lua_upvalueindex(2));
  if (isclosed(p))
    return luaL_error(L, "file is already closed");
  lua_settop(L , 1);
  luaL_checkstack(L, n, "too many arguments");
  for (i = 1; i <= n; i++)
    lua_pushvalue(L, lua_upvalueindex(3 + i));
  n = g_read(L, p->f, 2);
  lua_assert(n > 0);
  if (lua_toboolean(L, -n))
    return n;
  else {
    if (n > 1) {

      return luaL_error(L, "%s", lua_tostring(L, -n + 1));
    }
    if (lua_toboolean(L, lua_upvalueindex(3))) {
      lua_settop(L, 0);
      lua_pushvalue(L, lua_upvalueindex(1));
      aux_close(L);
    }
    return 0;
  }
}

static int g_write (lua_State *L, FILE *f, int arg) {
  int nargs = lua_gettop(L) - arg;
  int status = 1;
  errno = 0;
  for (; nargs--; arg++) {
    if (lua_type(L, arg) == LUA_TNUMBER) {

      int len = lua_isinteger(L, arg)
                ? fprintf(f, LUA_INTEGER_FMT,
                             (LUAI_UACINT)lua_tointeger(L, arg))
                : fprintf(f, LUA_NUMBER_FMT,
                             (LUAI_UACNUMBER)lua_tonumber(L, arg));
      status = status && (len > 0);
    }
    else {
      size_t l;
      const char *s = luaL_checklstring(L, arg, &l);
      status = status && (fwrite(s, sizeof(char), l, f) == l);
    }
  }
  if (l_likely(status))
    return 1;
  else
    return luaL_fileresult(L, status, NULL);
}

static int io_write (lua_State *L) {
  return g_write(L, getiofile(L, IO_OUTPUT), 1);
}

static int f_write (lua_State *L) {
  FILE *f = tofile(L);
  lua_pushvalue(L, 1);
  return g_write(L, f, 2);
}

static int f_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, "cur", modenames);
  lua_Integer p3 = luaL_optinteger(L, 3, 0);
  l_seeknum offset = (l_seeknum)p3;
  luaL_argcheck(L, (lua_Integer)offset == p3, 3,
                  "not an integer in proper range");
  errno = 0;
  op = l_fseek(f, offset, mode[op]);
  if (l_unlikely(op))
    return luaL_fileresult(L, 0, NULL);
  else {
    lua_pushinteger(L, (lua_Integer)l_ftell(f));
    return 1;
  }
}

static int f_setvbuf (lua_State *L) {
  static const int mode[] = {_IONBF, _IOFBF, _IOLBF};
  static const char *const modenames[] = {"no", "full", "line", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, NULL, modenames);
  lua_Integer sz = luaL_optinteger(L, 3, LUAL_BUFFERSIZE);
  int res;
  errno = 0;
  res = setvbuf(f, NULL, mode[op], (size_t)sz);
  return luaL_fileresult(L, res == 0, NULL);
}

static int io_flush (lua_State *L) {
  FILE *f = getiofile(L, IO_OUTPUT);
  errno = 0;
  return luaL_fileresult(L, fflush(f) == 0, NULL);
}

static int f_flush (lua_State *L) {
  FILE *f = tofile(L);
  errno = 0;
  return luaL_fileresult(L, fflush(f) == 0, NULL);
}

static const luaL_Reg iolib[] = {
  {"close", io_close},
  {"flush", io_flush},
  {"input", io_input},
  {"lines", io_lines},
  {"open", io_open},
  {"output", io_output},
  {"popen", io_popen},
  {"read", io_read},
  {"tmpfile", io_tmpfile},
  {"type", io_type},
  {"write", io_write},
  {NULL, NULL}
};

static const luaL_Reg meth[] = {
  {"read", f_read},
  {"write", f_write},
  {"lines", f_lines},
  {"flush", f_flush},
  {"seek", f_seek},
  {"close", f_close},
  {"setvbuf", f_setvbuf},
  {NULL, NULL}
};

static const luaL_Reg metameth[] = {
  {"__index", NULL},
  {"__gc", f_gc},
  {"__close", f_gc},
  {"__tostring", f_tostring},
  {NULL, NULL}
};

static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_FILEHANDLE);
  luaL_setfuncs(L, metameth, 0);
  luaL_newlibtable(L, meth);
  luaL_setfuncs(L, meth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

static int io_noclose (lua_State *L) {
  LStream *p = tolstream(L);
  p->closef = &io_noclose;
  luaL_pushfail(L);
  lua_pushliteral(L, "cannot close standard file");
  return 2;
}

static void createstdfile (lua_State *L, FILE *f, const char *k,
                           const char *fname) {
  LStream *p = newprefile(L);
  p->f = f;
  p->closef = &io_noclose;
  if (k != NULL) {
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, k);
  }
  lua_setfield(L, -2, fname);
}

LUAMOD_API int luaopen_io (lua_State *L) {
  luaL_newlib(L, iolib);
  createmeta(L);

  createstdfile(L, stdin, IO_INPUT, "stdin");
  createstdfile(L, stdout, IO_OUTPUT, "stdout");
  createstdfile(L, stderr, NULL, "stderr");
  return 1;
}
