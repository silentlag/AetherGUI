
#define lstring_c
#define LUA_CORE

#include "lprefix.h"

#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"

#define MAXSTRTB	cast_int(luaM_limitN(MAX_INT, TString*))

int luaS_eqlngstr (TString *a, TString *b) {
  size_t len = a->u.lnglen;
  lua_assert(a->tt == LUA_VLNGSTR && b->tt == LUA_VLNGSTR);
  return (a == b) ||
    ((len == b->u.lnglen) &&
     (memcmp(getlngstr(a), getlngstr(b), len) == 0));
}

unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast_uint(l);
  for (; l > 0; l--)
    h ^= ((h<<5) + (h>>2) + cast_byte(str[l - 1]));
  return h;
}

unsigned int luaS_hashlongstr (TString *ts) {
  lua_assert(ts->tt == LUA_VLNGSTR);
  if (ts->extra == 0) {
    size_t len = ts->u.lnglen;
    ts->hash = luaS_hash(getlngstr(ts), len, ts->hash);
    ts->extra = 1;
  }
  return ts->hash;
}

static void tablerehash (TString **vect, int osize, int nsize) {
  int i;
  for (i = osize; i < nsize; i++)
    vect[i] = NULL;
  for (i = 0; i < osize; i++) {
    TString *p = vect[i];
    vect[i] = NULL;
    while (p) {
      TString *hnext = p->u.hnext;
      unsigned int h = lmod(p->hash, nsize);
      p->u.hnext = vect[h];
      vect[h] = p;
      p = hnext;
    }
  }
}

void luaS_resize (lua_State *L, int nsize) {
  stringtable *tb = &G(L)->strt;
  int osize = tb->size;
  TString **newvect;
  if (nsize < osize)
    tablerehash(tb->hash, osize, nsize);
  newvect = luaM_reallocvector(L, tb->hash, osize, nsize, TString*);
  if (l_unlikely(newvect == NULL)) {
    if (nsize < osize)
      tablerehash(tb->hash, nsize, osize);

  }
  else {
    tb->hash = newvect;
    tb->size = nsize;
    if (nsize > osize)
      tablerehash(newvect, osize, nsize);
  }
}

void luaS_clearcache (global_State *g) {
  int i, j;
  for (i = 0; i < STRCACHE_N; i++)
    for (j = 0; j < STRCACHE_M; j++) {
      if (iswhite(g->strcache[i][j]))
        g->strcache[i][j] = g->memerrmsg;
    }
}

void luaS_init (lua_State *L) {
  global_State *g = G(L);
  int i, j;
  stringtable *tb = &G(L)->strt;
  tb->hash = luaM_newvector(L, MINSTRTABSIZE, TString*);
  tablerehash(tb->hash, 0, MINSTRTABSIZE);
  tb->size = MINSTRTABSIZE;

  g->memerrmsg = luaS_newliteral(L, MEMERRMSG);
  luaC_fix(L, obj2gco(g->memerrmsg));
  for (i = 0; i < STRCACHE_N; i++)
    for (j = 0; j < STRCACHE_M; j++)
      g->strcache[i][j] = g->memerrmsg;
}

static TString *createstrobj (lua_State *L, size_t l, int tag, unsigned int h) {
  TString *ts;
  GCObject *o;
  size_t totalsize;
  totalsize = sizelstring(l);
  o = luaC_newobj(L, tag, totalsize);
  ts = gco2ts(o);
  ts->hash = h;
  ts->extra = 0;
  getstr(ts)[l] = '\0';
  return ts;
}

TString *luaS_createlngstrobj (lua_State *L, size_t l) {
  TString *ts = createstrobj(L, l, LUA_VLNGSTR, G(L)->seed);
  ts->u.lnglen = l;
  ts->shrlen = 0xFF;
  return ts;
}

void luaS_remove (lua_State *L, TString *ts) {
  stringtable *tb = &G(L)->strt;
  TString **p = &tb->hash[lmod(ts->hash, tb->size)];
  while (*p != ts)
    p = &(*p)->u.hnext;
  *p = (*p)->u.hnext;
  tb->nuse--;
}

static void growstrtab (lua_State *L, stringtable *tb) {
  if (l_unlikely(tb->nuse == MAX_INT)) {
    luaC_fullgc(L, 1);
    if (tb->nuse == MAX_INT)
      luaM_error(L);
  }
  if (tb->size <= MAXSTRTB / 2)
    luaS_resize(L, tb->size * 2);
}

static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  TString *ts;
  global_State *g = G(L);
  stringtable *tb = &g->strt;
  unsigned int h = luaS_hash(str, l, g->seed);
  TString **list = &tb->hash[lmod(h, tb->size)];
  lua_assert(str != NULL);
  for (ts = *list; ts != NULL; ts = ts->u.hnext) {
    if (l == ts->shrlen && (memcmp(str, getshrstr(ts), l * sizeof(char)) == 0)) {

      if (isdead(g, ts))
        changewhite(ts);
      return ts;
    }
  }

  if (tb->nuse >= tb->size) {
    growstrtab(L, tb);
    list = &tb->hash[lmod(h, tb->size)];
  }
  ts = createstrobj(L, l, LUA_VSHRSTR, h);
  ts->shrlen = cast_byte(l);
  memcpy(getshrstr(ts), str, l * sizeof(char));
  ts->u.hnext = *list;
  *list = ts;
  tb->nuse++;
  return ts;
}

TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  if (l <= LUAI_MAXSHORTLEN)
    return internshrstr(L, str, l);
  else {
    TString *ts;
    if (l_unlikely(l * sizeof(char) >= (MAX_SIZE - sizeof(TString))))
      luaM_toobig(L);
    ts = luaS_createlngstrobj(L, l);
    memcpy(getlngstr(ts), str, l * sizeof(char));
    return ts;
  }
}

TString *luaS_new (lua_State *L, const char *str) {
  unsigned int i = point2uint(str) % STRCACHE_N;
  int j;
  TString **p = G(L)->strcache[i];
  for (j = 0; j < STRCACHE_M; j++) {
    if (strcmp(str, getstr(p[j])) == 0)
      return p[j];
  }

  for (j = STRCACHE_M - 1; j > 0; j--)
    p[j] = p[j - 1];

  p[0] = luaS_newlstr(L, str, strlen(str));
  return p[0];
}

Udata *luaS_newudata (lua_State *L, size_t s, int nuvalue) {
  Udata *u;
  int i;
  GCObject *o;
  if (l_unlikely(s > MAX_SIZE - udatamemoffset(nuvalue)))
    luaM_toobig(L);
  o = luaC_newobj(L, LUA_VUSERDATA, sizeudata(nuvalue, s));
  u = gco2u(o);
  u->len = s;
  u->nuvalue = nuvalue;
  u->metatable = NULL;
  for (i = 0; i < nuvalue; i++)
    setnilvalue(&u->uv[i].uv);
  return u;
}
