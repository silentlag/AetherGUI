
#define lfunc_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"

CClosure *luaF_newCclosure (lua_State *L, int nupvals) {
  GCObject *o = luaC_newobj(L, LUA_VCCL, sizeCclosure(nupvals));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(nupvals);
  return c;
}

LClosure *luaF_newLclosure (lua_State *L, int nupvals) {
  GCObject *o = luaC_newobj(L, LUA_VLCL, sizeLclosure(nupvals));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(nupvals);
  while (nupvals--) c->upvals[nupvals] = NULL;
  return c;
}

void luaF_initupvals (lua_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    GCObject *o = luaC_newobj(L, LUA_VUPVAL, sizeof(UpVal));
    UpVal *uv = gco2upv(o);
    uv->v.p = &uv->u.value;
    setnilvalue(uv->v.p);
    cl->upvals[i] = uv;
    luaC_objbarrier(L, cl, uv);
  }
}

static UpVal *newupval (lua_State *L, StkId level, UpVal **prev) {
  GCObject *o = luaC_newobj(L, LUA_VUPVAL, sizeof(UpVal));
  UpVal *uv = gco2upv(o);
  UpVal *next = *prev;
  uv->v.p = s2v(level);
  uv->u.open.next = next;
  uv->u.open.previous = prev;
  if (next)
    next->u.open.previous = &uv->u.open.next;
  *prev = uv;
  if (!isintwups(L)) {
    L->twups = G(L)->twups;
    G(L)->twups = L;
  }
  return uv;
}

UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  lua_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {
    lua_assert(!isdead(G(L), p));
    if (uplevel(p) == level)
      return p;
    pp = &p->u.open.next;
  }

  return newupval(L, level, pp);
}

static void callclosemethod (lua_State *L, TValue *obj, TValue *err, int yy) {
  StkId top = L->top.p;
  const TValue *tm = luaT_gettmbyobj(L, obj, TM_CLOSE);
  setobj2s(L, top, tm);
  setobj2s(L, top + 1, obj);
  setobj2s(L, top + 2, err);
  L->top.p = top + 3;
  if (yy)
    luaD_call(L, top, 0);
  else
    luaD_callnoyield(L, top, 0);
}

static void checkclosemth (lua_State *L, StkId level) {
  const TValue *tm = luaT_gettmbyobj(L, s2v(level), TM_CLOSE);
  if (ttisnil(tm)) {
    int idx = cast_int(level - L->ci->func.p);
    const char *vname = luaG_findlocal(L, L->ci, idx, NULL);
    if (vname == NULL) vname = "?";
    luaG_runerror(L, "variable '%s' got a non-closable value", vname);
  }
}

static void prepcallclosemth (lua_State *L, StkId level, int status, int yy) {
  TValue *uv = s2v(level);
  TValue *errobj;
  if (status == CLOSEKTOP)
    errobj = &G(L)->nilvalue;
  else {
    errobj = s2v(level + 1);
    luaD_seterrorobj(L, status, level + 1);
  }
  callclosemethod(L, uv, errobj, yy);
}

#define MAXDELTA  \
	((256ul << ((sizeof(L->stack.p->tbclist.delta) - 1) * 8)) - 1)

void luaF_newtbcupval (lua_State *L, StkId level) {
  lua_assert(level > L->tbclist.p);
  if (l_isfalse(s2v(level)))
    return;
  checkclosemth(L, level);
  while (cast_uint(level - L->tbclist.p) > MAXDELTA) {
    L->tbclist.p += MAXDELTA;
    L->tbclist.p->tbclist.delta = 0;
  }
  level->tbclist.delta = cast(unsigned short, level - L->tbclist.p);
  L->tbclist.p = level;
}

void luaF_unlinkupval (UpVal *uv) {
  lua_assert(upisopen(uv));
  *uv->u.open.previous = uv->u.open.next;
  if (uv->u.open.next)
    uv->u.open.next->u.open.previous = uv->u.open.previous;
}

void luaF_closeupval (lua_State *L, StkId level) {
  UpVal *uv;
  StkId upl;
  while ((uv = L->openupval) != NULL && (upl = uplevel(uv)) >= level) {
    TValue *slot = &uv->u.value;
    lua_assert(uplevel(uv) < L->top.p);
    luaF_unlinkupval(uv);
    setobj(L, slot, uv->v.p);
    uv->v.p = slot;
    if (!iswhite(uv)) {
      nw2black(uv);
      luaC_barrier(L, uv, slot);
    }
  }
}

static void poptbclist (lua_State *L) {
  StkId tbc = L->tbclist.p;
  lua_assert(tbc->tbclist.delta > 0);
  tbc -= tbc->tbclist.delta;
  while (tbc > L->stack.p && tbc->tbclist.delta == 0)
    tbc -= MAXDELTA;
  L->tbclist.p = tbc;
}

StkId luaF_close (lua_State *L, StkId level, int status, int yy) {
  ptrdiff_t levelrel = savestack(L, level);
  luaF_closeupval(L, level);
  while (L->tbclist.p >= level) {
    StkId tbc = L->tbclist.p;
    poptbclist(L);
    prepcallclosemth(L, tbc, status, yy);
    level = restorestack(L, levelrel);
  }
  return level;
}

Proto *luaF_newproto (lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_VPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->abslineinfo = NULL;
  f->sizeabslineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}

void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode);
  luaM_freearray(L, f->p, f->sizep);
  luaM_freearray(L, f->k, f->sizek);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo);
  luaM_freearray(L, f->abslineinfo, f->sizeabslineinfo);
  luaM_freearray(L, f->locvars, f->sizelocvars);
  luaM_freearray(L, f->upvalues, f->sizeupvalues);
  luaM_free(L, f);
}

const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;
}
