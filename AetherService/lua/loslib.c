
#define loslib_c
#define LUA_LIB

#include "lprefix.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#if !defined(LUA_STRFTIMEOPTIONS)

#if defined(LUA_USE_WINDOWS)
#define LUA_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"
#elif defined(LUA_USE_C89)
#define LUA_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYZ%"
#else
#define LUA_STRFTIMEOPTIONS  "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"
#endif

#endif

#if !defined(LUA_NUMTIME)

#define l_timet			lua_Integer
#define l_pushtime(L,t)		lua_pushinteger(L,(lua_Integer)(t))
#define l_gettime(L,arg)	luaL_checkinteger(L, arg)

#else

#define l_timet			lua_Number
#define l_pushtime(L,t)		lua_pushnumber(L,(lua_Number)(t))
#define l_gettime(L,arg)	luaL_checknumber(L, arg)

#endif

#if !defined(l_gmtime)

#if defined(LUA_USE_POSIX)

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else

#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)	((void)(r)->tm_sec, localtime(t))

#endif

#endif

#if !defined(lua_tmpnam)

#if defined(LUA_USE_POSIX)

#include <unistd.h>

#define LUA_TMPNAMBUFSIZE	32

#if !defined(LUA_TMPNAMTEMPLATE)
#define LUA_TMPNAMTEMPLATE	"/tmp/lua_XXXXXX"
#endif

#define lua_tmpnam(b,e) { \
        strcpy(b, LUA_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else

#define LUA_TMPNAMBUFSIZE	L_tmpnam
#define lua_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif

#endif

#if !defined(l_system)
#if defined(LUA_USE_IOS)

#define l_system(cmd) ((cmd) == NULL ? 0 : -1)
#else
#define l_system(cmd)	system(cmd)
#endif
#endif

static int os_execute (lua_State *L) {
  const char *cmd = luaL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = l_system(cmd);
  if (cmd != NULL)
    return luaL_execresult(L, stat);
  else {
    lua_pushboolean(L, stat);
    return 1;
  }
}

static int os_remove (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  errno = 0;
  return luaL_fileresult(L, remove(filename) == 0, filename);
}

static int os_rename (lua_State *L) {
  const char *fromname = luaL_checkstring(L, 1);
  const char *toname = luaL_checkstring(L, 2);
  errno = 0;
  return luaL_fileresult(L, rename(fromname, toname) == 0, NULL);
}

static int os_tmpname (lua_State *L) {
  char buff[LUA_TMPNAMBUFSIZE];
  int err;
  lua_tmpnam(buff, err);
  if (l_unlikely(err))
    return luaL_error(L, "unable to generate a unique filename");
  lua_pushstring(L, buff);
  return 1;
}

static int os_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_checkstring(L, 1)));
  return 1;
}

static int os_clock (lua_State *L) {
  lua_pushnumber(L, ((lua_Number)clock())/(lua_Number)CLOCKS_PER_SEC);
  return 1;
}

static void setfield (lua_State *L, const char *key, int value, int delta) {
  #if (defined(LUA_NUMTIME) && LUA_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > LUA_MAXINTEGER - delta))
      luaL_error(L, "field '%s' is out-of-bound", key);
  #endif
  lua_pushinteger(L, (lua_Integer)value + delta);
  lua_setfield(L, -2, key);
}

static void setboolfield (lua_State *L, const char *key, int value) {
  if (value < 0)
    return;
  lua_pushboolean(L, value);
  lua_setfield(L, -2, key);
}

static void setallfields (lua_State *L, struct tm *stm) {
  setfield(L, "year", stm->tm_year, 1900);
  setfield(L, "month", stm->tm_mon, 1);
  setfield(L, "day", stm->tm_mday, 0);
  setfield(L, "hour", stm->tm_hour, 0);
  setfield(L, "min", stm->tm_min, 0);
  setfield(L, "sec", stm->tm_sec, 0);
  setfield(L, "yday", stm->tm_yday, 1);
  setfield(L, "wday", stm->tm_wday, 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}

static int getboolfield (lua_State *L, const char *key) {
  int res;
  res = (lua_getfield(L, -1, key) == LUA_TNIL) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}

static int getfield (lua_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = lua_getfield(L, -1, key);
  lua_Integer res = lua_tointegerx(L, -1, &isnum);
  if (!isnum) {
    if (l_unlikely(t != LUA_TNIL))
      return luaL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))
      return luaL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    if (!(res >= 0 ? res - delta <= INT_MAX : INT_MIN + delta <= res))
      return luaL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  lua_pop(L, 1);
  return (int)res;
}

static const char *checkoption (lua_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = LUA_STRFTIMEOPTIONS;
  int oplen = 1;
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')
      oplen++;
    else if (memcmp(conv, option, oplen) == 0) {
      memcpy(buff, conv, oplen);
      buff[oplen] = '\0';
      return conv + oplen;
    }
  }
  luaL_argerror(L, 1,
    lua_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;
}

static time_t l_checktime (lua_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  luaL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}

#define SIZETIMEFMT	250

static int os_date (lua_State *L) {
  size_t slen;
  const char *s = luaL_optlstring(L, 1, "%c", &slen);
  time_t t = luaL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;
  struct tm tmr, *stm;
  if (*s == '!') {
    stm = l_gmtime(&t, &tmr);
    s++;
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)
    return luaL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    lua_createtable(L, 0, 9);
    setallfields(L, stm);
  }
  else {
    char cc[4];
    luaL_Buffer b;
    cc[0] = '%';
    luaL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')
        luaL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = luaL_prepbuffsize(&b, SIZETIMEFMT);
        s++;
        s = checkoption(L, s, se - s, cc + 1);
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        luaL_addsize(&b, reslen);
      }
    }
    luaL_pushresult(&b);
  }
  return 1;
}

static int os_time (lua_State *L) {
  time_t t;
  if (lua_isnoneornil(L, 1))
    t = time(NULL);
  else {
    struct tm ts;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    return luaL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}

static int os_difftime (lua_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  lua_pushnumber(L, (lua_Number)difftime(t1, t2));
  return 1;
}

static int os_setlocale (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = luaL_optstring(L, 1, NULL);
  int op = luaL_checkoption(L, 2, "all", catnames);
  lua_pushstring(L, setlocale(cat[op], l));
  return 1;
}

static int os_exit (lua_State *L) {
  int status;
  if (lua_isboolean(L, 1))
    status = (lua_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)luaL_optinteger(L, 1, EXIT_SUCCESS);
  if (lua_toboolean(L, 2))
    lua_close(L);
  if (L) exit(status);
  return 0;
}

static const luaL_Reg syslib[] = {
  {"clock",     os_clock},
  {"date",      os_date},
  {"difftime",  os_difftime},
  {"execute",   os_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
  {"remove",    os_remove},
  {"rename",    os_rename},
  {"setlocale", os_setlocale},
  {"time",      os_time},
  {"tmpname",   os_tmpname},
  {NULL, NULL}
};

LUAMOD_API int luaopen_os (lua_State *L) {
  luaL_newlib(L, syslib);
  return 1;
}
