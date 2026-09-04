
#define ltablib_c
#define LUA_LIB

#include "lprefix.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#define TAB_R	1
#define TAB_W	2
#define TAB_L	4
#define TAB_RW	(TAB_R | TAB_W)

#define aux_getn(L,n,w)	(checktab(L, n, (w) | TAB_L), luaL_len(L, n))

static int checkfield (lua_State *L, const char *key, int n) {
  lua_pushstring(L, key);
  return (lua_rawget(L, -n) != LUA_TNIL);
}

static void checktab (lua_State *L, int arg, int what) {
  if (lua_type(L, arg) != LUA_TTABLE) {
    int n = 1;
    if (lua_getmetatable(L, arg) &&
        (!(what & TAB_R) || checkfield(L, "__index", ++n)) &&
        (!(what & TAB_W) || checkfield(L, "__newindex", ++n)) &&
        (!(what & TAB_L) || checkfield(L, "__len", ++n))) {
      lua_pop(L, n);
    }
    else
      luaL_checktype(L, arg, LUA_TTABLE);
  }
}

static int tinsert (lua_State *L) {
  lua_Integer pos;
  lua_Integer e = aux_getn(L, 1, TAB_RW);
  e = luaL_intop(+, e, 1);
  switch (lua_gettop(L)) {
    case 2: {
      pos = e;
      break;
    }
    case 3: {
      lua_Integer i;
      pos = luaL_checkinteger(L, 2);

      luaL_argcheck(L, (lua_Unsigned)pos - 1u < (lua_Unsigned)e, 2,
                       "position out of bounds");
      for (i = e; i > pos; i--) {
        lua_geti(L, 1, i - 1);
        lua_seti(L, 1, i);
      }
      break;
    }
    default: {
      return luaL_error(L, "wrong number of arguments to 'insert'");
    }
  }
  lua_seti(L, 1, pos);
  return 0;
}

static int tremove (lua_State *L) {
  lua_Integer size = aux_getn(L, 1, TAB_RW);
  lua_Integer pos = luaL_optinteger(L, 2, size);
  if (pos != size)

    luaL_argcheck(L, (lua_Unsigned)pos - 1u <= (lua_Unsigned)size, 2,
                     "position out of bounds");
  lua_geti(L, 1, pos);
  for ( ; pos < size; pos++) {
    lua_geti(L, 1, pos + 1);
    lua_seti(L, 1, pos);
  }
  lua_pushnil(L);
  lua_seti(L, 1, pos);
  return 1;
}

static int tmove (lua_State *L) {
  lua_Integer f = luaL_checkinteger(L, 2);
  lua_Integer e = luaL_checkinteger(L, 3);
  lua_Integer t = luaL_checkinteger(L, 4);
  int tt = !lua_isnoneornil(L, 5) ? 5 : 1;
  checktab(L, 1, TAB_R);
  checktab(L, tt, TAB_W);
  if (e >= f) {
    lua_Integer n, i;
    luaL_argcheck(L, f > 0 || e < LUA_MAXINTEGER + f, 3,
                  "too many elements to move");
    n = e - f + 1;
    luaL_argcheck(L, t <= LUA_MAXINTEGER - n + 1, 4,
                  "destination wrap around");
    if (t > e || t <= f || (tt != 1 && !lua_compare(L, 1, tt, LUA_OPEQ))) {
      for (i = 0; i < n; i++) {
        lua_geti(L, 1, f + i);
        lua_seti(L, tt, t + i);
      }
    }
    else {
      for (i = n - 1; i >= 0; i--) {
        lua_geti(L, 1, f + i);
        lua_seti(L, tt, t + i);
      }
    }
  }
  lua_pushvalue(L, tt);
  return 1;
}

static void addfield (lua_State *L, luaL_Buffer *b, lua_Integer i) {
  lua_geti(L, 1, i);
  if (l_unlikely(!lua_isstring(L, -1)))
    luaL_error(L, "invalid value (%s) at index %I in table for 'concat'",
                  luaL_typename(L, -1), (LUAI_UACINT)i);
  luaL_addvalue(b);
}

static int tconcat (lua_State *L) {
  luaL_Buffer b;
  lua_Integer last = aux_getn(L, 1, TAB_R);
  size_t lsep;
  const char *sep = luaL_optlstring(L, 2, "", &lsep);
  lua_Integer i = luaL_optinteger(L, 3, 1);
  last = luaL_optinteger(L, 4, last);
  luaL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    luaL_addlstring(&b, sep, lsep);
  }
  if (i == last)
    addfield(L, &b, i);
  luaL_pushresult(&b);
  return 1;
}

static int tpack (lua_State *L) {
  int i;
  int n = lua_gettop(L);
  lua_createtable(L, n, 1);
  lua_insert(L, 1);
  for (i = n; i >= 1; i--)
    lua_seti(L, 1, i);
  lua_pushinteger(L, n);
  lua_setfield(L, 1, "n");
  return 1;
}

static int tunpack (lua_State *L) {
  lua_Unsigned n;
  lua_Integer i = luaL_optinteger(L, 2, 1);
  lua_Integer e = luaL_opt(L, luaL_checkinteger, 3, luaL_len(L, 1));
  if (i > e) return 0;
  n = (lua_Unsigned)e - i;
  if (l_unlikely(n >= (unsigned int)INT_MAX  ||
                 !lua_checkstack(L, (int)(++n))))
    return luaL_error(L, "too many results to unpack");
  for (; i < e; i++) {
    lua_geti(L, 1, i);
  }
  lua_geti(L, 1, e);
  return (int)n;
}

typedef unsigned int IdxT;

#if !defined(l_randomizePivot)

#include <time.h>

#define sof(e)		(sizeof(e) / sizeof(unsigned int))

static unsigned int l_randomizePivot (void) {
  clock_t c = clock();
  time_t t = time(NULL);
  unsigned int buff[sof(c) + sof(t)];
  unsigned int i, rnd = 0;
  memcpy(buff, &c, sof(c) * sizeof(unsigned int));
  memcpy(buff + sof(c), &t, sof(t) * sizeof(unsigned int));
  for (i = 0; i < sof(buff); i++)
    rnd += buff[i];
  return rnd;
}

#endif

#define RANLIMIT	100u

static void set2 (lua_State *L, IdxT i, IdxT j) {
  lua_seti(L, 1, i);
  lua_seti(L, 1, j);
}

static int sort_comp (lua_State *L, int a, int b) {
  if (lua_isnil(L, 2))
    return lua_compare(L, a, b, LUA_OPLT);
  else {
    int res;
    lua_pushvalue(L, 2);
    lua_pushvalue(L, a-1);
    lua_pushvalue(L, b-2);
    lua_call(L, 2, 1);
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
  }
}

static IdxT partition (lua_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;
  IdxT j = up - 1;

  for (;;) {

    while ((void)lua_geti(L, 1, ++i), sort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))
        luaL_error(L, "invalid order function for sorting");
      lua_pop(L, 1);
    }

    while ((void)lua_geti(L, 1, --j), sort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))
        luaL_error(L, "invalid order function for sorting");
      lua_pop(L, 1);
    }

    if (j < i) {

      lua_pop(L, 1);

      set2(L, up - 1, i);
      return i;
    }

    set2(L, i, j);
  }
}

static IdxT choosePivot (IdxT lo, IdxT up, unsigned int rnd) {
  IdxT r4 = (up - lo) / 4;
  IdxT p = rnd % (r4 * 2) + (lo + r4);
  lua_assert(lo + r4 <= p && p <= up - r4);
  return p;
}

static void auxsort (lua_State *L, IdxT lo, IdxT up,
                                   unsigned int rnd) {
  while (lo < up) {
    IdxT p;
    IdxT n;

    lua_geti(L, 1, lo);
    lua_geti(L, 1, up);
    if (sort_comp(L, -1, -2))
      set2(L, lo, up);
    else
      lua_pop(L, 2);
    if (up - lo == 1)
      return;
    if (up - lo < RANLIMIT || rnd == 0)
      p = (lo + up)/2;
    else
      p = choosePivot(lo, up, rnd);
    lua_geti(L, 1, p);
    lua_geti(L, 1, lo);
    if (sort_comp(L, -2, -1))
      set2(L, p, lo);
    else {
      lua_pop(L, 1);
      lua_geti(L, 1, up);
      if (sort_comp(L, -1, -2))
        set2(L, p, up);
      else
        lua_pop(L, 2);
    }
    if (up - lo == 2)
      return;
    lua_geti(L, 1, p);
    lua_pushvalue(L, -1);
    lua_geti(L, 1, up - 1);
    set2(L, p, up - 1);
    p = partition(L, lo, up);

    if (p - lo < up - p) {
      auxsort(L, lo, p - 1, rnd);
      n = p - lo;
      lo = p + 1;
    }
    else {
      auxsort(L, p + 1, up, rnd);
      n = up - p;
      up = p - 1;
    }
    if ((up - lo) / 128 > n)
      rnd = l_randomizePivot();
  }
}

static int sort (lua_State *L) {
  lua_Integer n = aux_getn(L, 1, TAB_RW);
  if (n > 1) {
    luaL_argcheck(L, n < INT_MAX, 1, "array too big");
    if (!lua_isnoneornil(L, 2))
      luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 2);
    auxsort(L, 1, (IdxT)n, 0);
  }
  return 0;
}

static const luaL_Reg tab_funcs[] = {
  {"concat", tconcat},
  {"insert", tinsert},
  {"pack", tpack},
  {"unpack", tunpack},
  {"remove", tremove},
  {"move", tmove},
  {"sort", sort},
  {NULL, NULL}
};

LUAMOD_API int luaopen_table (lua_State *L) {
  luaL_newlib(L, tab_funcs);
  return 1;
}
