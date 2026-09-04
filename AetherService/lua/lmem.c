
#define lmem_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"

#define callfrealloc(g,block,os,ns)    ((*g->frealloc)(g->ud, block, os, ns))

#define cantryagain(g)	(completestate(g) && !g->gcstopem)

#if defined(EMERGENCYGCTESTS)

static void *firsttry (global_State *g, void *block, size_t os, size_t ns) {
  if (ns > 0 && cantryagain(g))
    return NULL;
  else
    return callfrealloc(g, block, os, ns);
}
#else
#define firsttry(g,block,os,ns)    callfrealloc(g, block, os, ns)
#endif

#define MINSIZEARRAY	4

void *luaM_growaux_ (lua_State *L, void *block, int nelems, int *psize,
                     int size_elems, int limit, const char *what) {
  void *newblock;
  int size = *psize;
  if (nelems + 1 <= size)
    return block;
  if (size >= limit / 2) {
    if (l_unlikely(size >= limit))
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    size = limit;
  }
  else {
    size *= 2;
    if (size < MINSIZEARRAY)
      size = MINSIZEARRAY;
  }
  lua_assert(nelems + 1 <= size && size <= limit);

  newblock = luaM_saferealloc_(L, block, cast_sizet(*psize) * size_elems,
                                         cast_sizet(size) * size_elems);
  *psize = size;
  return newblock;
}

void *luaM_shrinkvector_ (lua_State *L, void *block, int *size,
                          int final_n, int size_elem) {
  void *newblock;
  size_t oldsize = cast_sizet((*size) * size_elem);
  size_t newsize = cast_sizet(final_n * size_elem);
  lua_assert(newsize <= oldsize);
  newblock = luaM_saferealloc_(L, block, oldsize, newsize);
  *size = final_n;
  return newblock;
}

l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}

void luaM_free_ (lua_State *L, void *block, size_t osize) {
  global_State *g = G(L);
  lua_assert((osize == 0) == (block == NULL));
  callfrealloc(g, block, osize, 0);
  g->GCdebt -= osize;
}

static void *tryagain (lua_State *L, void *block,
                       size_t osize, size_t nsize) {
  global_State *g = G(L);
  if (cantryagain(g)) {
    luaC_fullgc(L, 1);
    return callfrealloc(g, block, osize, nsize);
  }
  else return NULL;
}

void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  lua_assert((osize == 0) == (block == NULL));
  newblock = firsttry(g, block, osize, nsize);
  if (l_unlikely(newblock == NULL && nsize > 0)) {
    newblock = tryagain(L, block, osize, nsize);
    if (newblock == NULL)
      return NULL;
  }
  lua_assert((nsize == 0) == (newblock == NULL));
  g->GCdebt = (g->GCdebt + nsize) - osize;
  return newblock;
}

void *luaM_saferealloc_ (lua_State *L, void *block, size_t osize,
                                                    size_t nsize) {
  void *newblock = luaM_realloc_(L, block, osize, nsize);
  if (l_unlikely(newblock == NULL && nsize > 0))
    luaM_error(L);
  return newblock;
}

void *luaM_malloc_ (lua_State *L, size_t size, int tag) {
  if (size == 0)
    return NULL;
  else {
    global_State *g = G(L);
    void *newblock = firsttry(g, NULL, tag, size);
    if (l_unlikely(newblock == NULL)) {
      newblock = tryagain(L, NULL, tag, size);
      if (newblock == NULL)
        luaM_error(L);
    }
    g->GCdebt += size;
    return newblock;
  }
}
