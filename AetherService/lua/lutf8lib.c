
#define lutf8lib_c
#define LUA_LIB

#include "lprefix.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#define MAXUNICODE	0x10FFFFu

#define MAXUTF		0x7FFFFFFFu

#define MSGInvalid	"invalid UTF-8 code"

#if (UINT_MAX >> 30) >= 1
typedef unsigned int utfint;
#else
typedef unsigned long utfint;
#endif

#define iscont(c)	(((c) & 0xC0) == 0x80)
#define iscontp(p)	iscont(*(p))

static lua_Integer u_posrelat (lua_Integer pos, size_t len) {
  if (pos >= 0) return pos;
  else if (0u - (size_t)pos > len) return 0;
  else return (lua_Integer)len + pos + 1;
}

static const char *utf8_decode (const char *s, utfint *val, int strict) {
  static const utfint limits[] =
        {~(utfint)0, 0x80, 0x800, 0x10000u, 0x200000u, 0x4000000u};
  unsigned int c = (unsigned char)s[0];
  utfint res = 0;
  if (c < 0x80)
    res = c;
  else {
    int count = 0;
    for (; c & 0x40; c <<= 1) {
      unsigned int cc = (unsigned char)s[++count];
      if (!iscont(cc))
        return NULL;
      res = (res << 6) | (cc & 0x3F);
    }
    res |= ((utfint)(c & 0x7F) << (count * 5));
    if (count > 5 || res > MAXUTF || res < limits[count])
      return NULL;
    s += count;
  }
  if (strict) {

    if (res > MAXUNICODE || (0xD800u <= res && res <= 0xDFFFu))
      return NULL;
  }
  if (val) *val = res;
  return s + 1;
}

static int utflen (lua_State *L) {
  lua_Integer n = 0;
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  lua_Integer posi = u_posrelat(luaL_optinteger(L, 2, 1), len);
  lua_Integer posj = u_posrelat(luaL_optinteger(L, 3, -1), len);
  int lax = lua_toboolean(L, 4);
  luaL_argcheck(L, 1 <= posi && --posi <= (lua_Integer)len, 2,
                   "initial position out of bounds");
  luaL_argcheck(L, --posj < (lua_Integer)len, 3,
                   "final position out of bounds");
  while (posi <= posj) {
    const char *s1 = utf8_decode(s + posi, NULL, !lax);
    if (s1 == NULL) {
      luaL_pushfail(L);
      lua_pushinteger(L, posi + 1);
      return 2;
    }
    posi = s1 - s;
    n++;
  }
  lua_pushinteger(L, n);
  return 1;
}

static int codepoint (lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  lua_Integer posi = u_posrelat(luaL_optinteger(L, 2, 1), len);
  lua_Integer pose = u_posrelat(luaL_optinteger(L, 3, posi), len);
  int lax = lua_toboolean(L, 4);
  int n;
  const char *se;
  luaL_argcheck(L, posi >= 1, 2, "out of bounds");
  luaL_argcheck(L, pose <= (lua_Integer)len, 3, "out of bounds");
  if (posi > pose) return 0;
  if (pose - posi >= INT_MAX)
    return luaL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;
  luaL_checkstack(L, n, "string slice too long");
  n = 0;
  se = s + pose;
  for (s += posi - 1; s < se;) {
    utfint code;
    s = utf8_decode(s, &code, !lax);
    if (s == NULL)
      return luaL_error(L, MSGInvalid);
    lua_pushinteger(L, code);
    n++;
  }
  return n;
}

static void pushutfchar (lua_State *L, int arg) {
  lua_Unsigned code = (lua_Unsigned)luaL_checkinteger(L, arg);
  luaL_argcheck(L, code <= MAXUTF, arg, "value out of range");
  lua_pushfstring(L, "%U", (long)code);
}

static int utfchar (lua_State *L) {
  int n = lua_gettop(L);
  if (n == 1)
    pushutfchar(L, 1);
  else {
    int i;
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (i = 1; i <= n; i++) {
      pushutfchar(L, i);
      luaL_addvalue(&b);
    }
    luaL_pushresult(&b);
  }
  return 1;
}

static int byteoffset (lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  lua_Integer n  = luaL_checkinteger(L, 2);
  lua_Integer posi = (n >= 0) ? 1 : len + 1;
  posi = u_posrelat(luaL_optinteger(L, 3, posi), len);
  luaL_argcheck(L, 1 <= posi && --posi <= (lua_Integer)len, 3,
                   "position out of bounds");
  if (n == 0) {

    while (posi > 0 && iscontp(s + posi)) posi--;
  }
  else {
    if (iscontp(s + posi))
      return luaL_error(L, "initial position is a continuation byte");
    if (n < 0) {
       while (n < 0 && posi > 0) {
         do {
           posi--;
         } while (posi > 0 && iscontp(s + posi));
         n++;
       }
     }
     else {
       n--;
       while (n > 0 && posi < (lua_Integer)len) {
         do {
           posi++;
         } while (iscontp(s + posi));
         n--;
       }
     }
  }
  if (n == 0)
    lua_pushinteger(L, posi + 1);
  else
    luaL_pushfail(L);
  return 1;
}

static int iter_aux (lua_State *L, int strict) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  lua_Unsigned n = (lua_Unsigned)lua_tointeger(L, 2);
  if (n < len) {
    while (iscontp(s + n)) n++;
  }
  if (n >= len)
    return 0;
  else {
    utfint code;
    const char *next = utf8_decode(s + n, &code, strict);
    if (next == NULL || iscontp(next))
      return luaL_error(L, MSGInvalid);
    lua_pushinteger(L, n + 1);
    lua_pushinteger(L, code);
    return 2;
  }
}

static int iter_auxstrict (lua_State *L) {
  return iter_aux(L, 1);
}

static int iter_auxlax (lua_State *L) {
  return iter_aux(L, 0);
}

static int iter_codes (lua_State *L) {
  int lax = lua_toboolean(L, 2);
  const char *s = luaL_checkstring(L, 1);
  luaL_argcheck(L, !iscontp(s), 1, MSGInvalid);
  lua_pushcfunction(L, lax ? iter_auxlax : iter_auxstrict);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}

#define UTF8PATT	"[\0-\x7F\xC2-\xFD][\x80-\xBF]*"

static const luaL_Reg funcs[] = {
  {"offset", byteoffset},
  {"codepoint", codepoint},
  {"char", utfchar},
  {"len", utflen},
  {"codes", iter_codes},

  {"charpattern", NULL},
  {NULL, NULL}
};

LUAMOD_API int luaopen_utf8 (lua_State *L) {
  luaL_newlib(L, funcs);
  lua_pushlstring(L, UTF8PATT, sizeof(UTF8PATT)/sizeof(char) - 1);
  lua_setfield(L, -2, "charpattern");
  return 1;
}
