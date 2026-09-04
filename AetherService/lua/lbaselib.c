
#define lbaselib_c
#define LUA_LIB

#include "lprefix.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);
  int i;
  for (i = 1; i <= n; i++) {
    size_t l;
    const char *s = luaL_tolstring(L, i, &l);
    if (i > 1)
      lua_writestring("\t", 1);
    lua_writestring(s, l);
    lua_pop(L, 1);
  }
  lua_writeline();
  return 0;
}

static int luaB_warn (lua_State *L) {
  int n = lua_gettop(L);
  int i;
  luaL_checkstring(L, 1);
  for (i = 2; i <= n; i++)
    luaL_checkstring(L, i);
  for (i = 1; i < n; i++)
    lua_warning(L, lua_tostring(L, i), 1);
  lua_warning(L, lua_tostring(L, n), 0);
  return 0;
}

#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, lua_Integer *pn) {
  lua_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);
  if (*s == '-') { s++; neg = 1; }
  else if (*s == '+') s++;
  if (!isalnum((unsigned char)*s))
    return NULL;
  do {
    int digit = (isdigit((unsigned char)*s)) ? *s - '0'
                   : (toupper((unsigned char)*s) - 'A') + 10;
    if (digit >= base) return NULL;
    n = n * base + digit;
    s++;
  } while (isalnum((unsigned char)*s));
  s += strspn(s, SPACECHARS);
  *pn = (lua_Integer)((neg) ? (0u - n) : n);
  return s;
}

static int luaB_tonumber (lua_State *L) {
  if (lua_isnoneornil(L, 2)) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
      lua_settop(L, 1);
      return 1;
    }
    else {
      size_t l;
      const char *s = lua_tolstring(L, 1, &l);
      if (s != NULL && lua_stringtonumber(L, s) == l + 1)
        return 1;

      luaL_checkany(L, 1);
    }
  }
  else {
    size_t l;
    const char *s;
    lua_Integer n = 0;
    lua_Integer base = luaL_checkinteger(L, 2);
    luaL_checktype(L, 1, LUA_TSTRING);
    s = lua_tolstring(L, 1, &l);
    luaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      lua_pushinteger(L, n);
      return 1;
    }
  }
  luaL_pushfail(L);
  return 1;
}

static int luaB_error (lua_State *L) {
  int level = (int)luaL_optinteger(L, 2, 1);
  lua_settop(L, 1);
  if (lua_type(L, 1) == LUA_TSTRING && level > 0) {
    luaL_where(L, level);
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}

static int luaB_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    lua_pushnil(L);
    return 1;
  }
  luaL_getmetafield(L, 1, "__metatable");
  return 1;
}

static int luaB_setmetatable (lua_State *L) {
  int t = lua_type(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_argexpected(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil or table");
  if (l_unlikely(luaL_getmetafield(L, 1, "__metatable") != LUA_TNIL))
    return luaL_error(L, "cannot change a protected metatable");
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;
}

static int luaB_rawequal (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_checkany(L, 2);
  lua_pushboolean(L, lua_rawequal(L, 1, 2));
  return 1;
}

static int luaB_rawlen (lua_State *L) {
  int t = lua_type(L, 1);
  luaL_argexpected(L, t == LUA_TTABLE || t == LUA_TSTRING, 1,
                      "table or string");
  lua_pushinteger(L, lua_rawlen(L, 1));
  return 1;
}

static int luaB_rawget (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  lua_settop(L, 2);
  lua_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  lua_settop(L, 3);
  lua_rawset(L, 1);
  return 1;
}

static int pushmode (lua_State *L, int oldmode) {
  if (oldmode == -1)
    luaL_pushfail(L);
  else
    lua_pushstring(L, (oldmode == LUA_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}

#define checkvalres(res) { if (res == -1) break; }

static int luaB_collectgarbage (lua_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
    LUA_GCCOUNT, LUA_GCSTEP, LUA_GCSETPAUSE, LUA_GCSETSTEPMUL,
    LUA_GCISRUNNING, LUA_GCGEN, LUA_GCINC};
  int o = optsnum[luaL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case LUA_GCCOUNT: {
      int k = lua_gc(L, o);
      int b = lua_gc(L, LUA_GCCOUNTB);
      checkvalres(k);
      lua_pushnumber(L, (lua_Number)k + ((lua_Number)b/1024));
      return 1;
    }
    case LUA_GCSTEP: {
      int step = (int)luaL_optinteger(L, 2, 0);
      int res = lua_gc(L, o, step);
      checkvalres(res);
      lua_pushboolean(L, res);
      return 1;
    }
    case LUA_GCSETPAUSE:
    case LUA_GCSETSTEPMUL: {
      int p = (int)luaL_optinteger(L, 2, 0);
      int previous = lua_gc(L, o, p);
      checkvalres(previous);
      lua_pushinteger(L, previous);
      return 1;
    }
    case LUA_GCISRUNNING: {
      int res = lua_gc(L, o);
      checkvalres(res);
      lua_pushboolean(L, res);
      return 1;
    }
    case LUA_GCGEN: {
      int minormul = (int)luaL_optinteger(L, 2, 0);
      int majormul = (int)luaL_optinteger(L, 3, 0);
      return pushmode(L, lua_gc(L, o, minormul, majormul));
    }
    case LUA_GCINC: {
      int pause = (int)luaL_optinteger(L, 2, 0);
      int stepmul = (int)luaL_optinteger(L, 3, 0);
      int stepsize = (int)luaL_optinteger(L, 4, 0);
      return pushmode(L, lua_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = lua_gc(L, o);
      checkvalres(res);
      lua_pushinteger(L, res);
      return 1;
    }
  }
  luaL_pushfail(L);
  return 1;
}

static int luaB_type (lua_State *L) {
  int t = lua_type(L, 1);
  luaL_argcheck(L, t != LUA_TNONE, 1, "value expected");
  lua_pushstring(L, lua_typename(L, t));
  return 1;
}

static int luaB_next (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_settop(L, 2);
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}

static int pairscont (lua_State *L, int status, lua_KContext k) {
  (void)L; (void)status; (void)k;
  return 3;
}

static int luaB_pairs (lua_State *L) {
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, "__pairs") == LUA_TNIL) {
    lua_pushcfunction(L, luaB_next);
    lua_pushvalue(L, 1);
    lua_pushnil(L);
  }
  else {
    lua_pushvalue(L, 1);
    lua_callk(L, 1, 3, 0, pairscont);
  }
  return 3;
}

static int ipairsaux (lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 2);
  i = luaL_intop(+, i, 1);
  lua_pushinteger(L, i);
  return (lua_geti(L, 1, i) == LUA_TNIL) ? 1 : 2;
}

static int luaB_ipairs (lua_State *L) {
  luaL_checkany(L, 1);
  lua_pushcfunction(L, ipairsaux);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}

static int load_aux (lua_State *L, int status, int envidx) {
  if (l_likely(status == LUA_OK)) {
    if (envidx != 0) {
      lua_pushvalue(L, envidx);
      if (!lua_setupvalue(L, -2, 1))
        lua_pop(L, 1);
    }
    return 1;
  }
  else {
    luaL_pushfail(L);
    lua_insert(L, -2);
    return 2;
  }
}

static int luaB_loadfile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  const char *mode = luaL_optstring(L, 2, NULL);
  int env = (!lua_isnone(L, 3) ? 3 : 0);
  int status = luaL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}

#define RESERVEDSLOT	5

static const char *generic_reader (lua_State *L, void *ud, size_t *size) {
  (void)(ud);
  luaL_checkstack(L, 2, "too many nested functions");
  lua_pushvalue(L, 1);
  lua_call(L, 0, 1);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!lua_isstring(L, -1)))
    luaL_error(L, "reader function must return a string");
  lua_replace(L, RESERVEDSLOT);
  return lua_tolstring(L, RESERVEDSLOT, size);
}

static int luaB_load (lua_State *L) {
  int status;
  size_t l;
  const char *s = lua_tolstring(L, 1, &l);
  const char *mode = luaL_optstring(L, 3, "bt");
  int env = (!lua_isnone(L, 4) ? 4 : 0);
  if (s != NULL) {
    const char *chunkname = luaL_optstring(L, 2, s);
    status = luaL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {
    const char *chunkname = luaL_optstring(L, 2, "=(load)");
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, RESERVEDSLOT);
    status = lua_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

static int dofilecont (lua_State *L, int d1, lua_KContext d2) {
  (void)d1;  (void)d2;
  return lua_gettop(L) - 1;
}

static int luaB_dofile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  lua_settop(L, 1);
  if (l_unlikely(luaL_loadfile(L, fname) != LUA_OK))
    return lua_error(L);
  lua_callk(L, 0, LUA_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}

static int luaB_assert (lua_State *L) {
  if (l_likely(lua_toboolean(L, 1)))
    return lua_gettop(L);
  else {
    luaL_checkany(L, 1);
    lua_remove(L, 1);
    lua_pushliteral(L, "assertion failed!");
    lua_settop(L, 1);
    return luaB_error(L);
  }
}

static int luaB_select (lua_State *L) {
  int n = lua_gettop(L);
  if (lua_type(L, 1) == LUA_TSTRING && *lua_tostring(L, 1) == '#') {
    lua_pushinteger(L, n-1);
    return 1;
  }
  else {
    lua_Integer i = luaL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    luaL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}

static int finishpcall (lua_State *L, int status, lua_KContext extra) {
  if (l_unlikely(status != LUA_OK && status != LUA_YIELD)) {
    lua_pushboolean(L, 0);
    lua_pushvalue(L, -2);
    return 2;
  }
  else
    return lua_gettop(L) - (int)extra;
}

static int luaB_pcall (lua_State *L) {
  int status;
  luaL_checkany(L, 1);
  lua_pushboolean(L, 1);
  lua_insert(L, 1);
  status = lua_pcallk(L, lua_gettop(L) - 2, LUA_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}

static int luaB_xpcall (lua_State *L) {
  int status;
  int n = lua_gettop(L);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  lua_pushboolean(L, 1);
  lua_pushvalue(L, 1);
  lua_rotate(L, 3, 2);
  status = lua_pcallk(L, n - 2, LUA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}

static int luaB_tostring (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_tolstring(L, 1, NULL);
  return 1;
}

static const luaL_Reg base_funcs[] = {
  {"assert", luaB_assert},
  {"collectgarbage", luaB_collectgarbage},
  {"dofile", luaB_dofile},
  {"error", luaB_error},
  {"getmetatable", luaB_getmetatable},
  {"ipairs", luaB_ipairs},
  {"loadfile", luaB_loadfile},
  {"load", luaB_load},
  {"next", luaB_next},
  {"pairs", luaB_pairs},
  {"pcall", luaB_pcall},
  {"print", luaB_print},
  {"warn", luaB_warn},
  {"rawequal", luaB_rawequal},
  {"rawlen", luaB_rawlen},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"select", luaB_select},
  {"setmetatable", luaB_setmetatable},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"xpcall", luaB_xpcall},

  {LUA_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};

LUAMOD_API int luaopen_base (lua_State *L) {

  lua_pushglobaltable(L);
  luaL_setfuncs(L, base_funcs, 0);

  lua_pushvalue(L, -1);
  lua_setfield(L, -2, LUA_GNAME);

  lua_pushliteral(L, LUA_VERSION);
  lua_setfield(L, -2, "_VERSION");
  return 1;
}
