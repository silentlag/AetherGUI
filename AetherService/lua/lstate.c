
#define lstate_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"

typedef struct LX {
  lu_byte extra_[LUA_EXTRASPACE];
  lua_State l;
} LX;

typedef struct LG {
  LX l;
  global_State g;
} LG;

#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))

#if !defined(luai_makeseed)

#include <time.h>

#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int luai_makeseed (lua_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);
  addbuff(buff, p, &h);
  addbuff(buff, p, &lua_newstate);
  lua_assert(p == sizeof(buff));
  return luaS_hash(buff, p, h);
}

#endif

void luaE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  lua_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}

LUA_API int lua_setcstacklimit (lua_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return LUAI_MAXCCALLS;
}

CallInfo *luaE_extendCI (lua_State *L) {
  CallInfo *ci;
  lua_assert(L->ci->next == NULL);
  ci = luaM_new(L, CallInfo);
  lua_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  ci->u.l.trap = 0;
  L->nci++;
  return ci;
}

static void freeCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    luaM_free(L, ci);
    L->nci--;
  }
}

void luaE_shrinkCI (lua_State *L) {
  CallInfo *ci = L->ci->next;
  CallInfo *next;
  if (ci == NULL)
    return;
  while ((next = ci->next) != NULL) {
    CallInfo *next2 = next->next;
    ci->next = next2;
    L->nci--;
    luaM_free(L, next);
    if (next2 == NULL)
      break;
    else {
      next2->previous = ci;
      ci = next2;
    }
  }
}

void luaE_checkcstack (lua_State *L) {
  if (getCcalls(L) == LUAI_MAXCCALLS)
    luaG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (LUAI_MAXCCALLS / 10 * 11))
    luaD_errerr(L);
}

LUAI_FUNC void luaE_incCstack (lua_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= LUAI_MAXCCALLS))
    luaE_checkcstack(L);
}

static void stack_init (lua_State *L1, lua_State *L) {
  int i; CallInfo *ci;

  L1->stack.p = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  L1->tbclist.p = L1->stack.p;
  for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack.p + i));
  L1->top.p = L1->stack.p;
  L1->stack_last.p = L1->stack.p + BASIC_STACK_SIZE;

  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = CIST_C;
  ci->func.p = L1->top.p;
  ci->u.c.k = NULL;
  ci->nresults = 0;
  setnilvalue(s2v(L1->top.p));
  L1->top.p++;
  ci->top.p = L1->top.p + LUA_MINSTACK;
  L1->ci = ci;
}

static void freestack (lua_State *L) {
  if (L->stack.p == NULL)
    return;
  L->ci = &L->base_ci;
  freeCI(L);
  lua_assert(L->nci == 0);
  luaM_freearray(L, L->stack.p, stacksize(L) + EXTRA_STACK);
}

static void init_registry (lua_State *L, global_State *g) {

  Table *registry = luaH_new(L);
  sethvalue(L, &g->l_registry, registry);
  luaH_resize(L, registry, LUA_RIDX_LAST, 0);

  setthvalue(L, &registry->array[LUA_RIDX_MAINTHREAD - 1], L);

  sethvalue(L, &registry->array[LUA_RIDX_GLOBALS - 1], luaH_new(L));
}

static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);
  init_registry(L, g);
  luaS_init(L);
  luaT_init(L);
  luaX_init(L);
  g->gcstp = 0;
  setnilvalue(&g->nilvalue);
  luai_userstateopen(L);
}

static void preinit_thread (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack.p = NULL;
  L->ci = NULL;
  L->nci = 0;
  L->twups = L;
  L->nCcalls = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->status = LUA_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}

static void close_state (lua_State *L) {
  global_State *g = G(L);
  if (!completestate(g))
    luaC_freeallobjects(L);
  else {
    L->ci = &L->base_ci;
    L->errfunc = 0;
    luaD_closeprotected(L, 1, LUA_OK);
    L->top.p = L->stack.p + 1;
    luaC_freeallobjects(L);
    luai_userstateclose(L);
  }
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  lua_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);
}

LUA_API lua_State *lua_newthread (lua_State *L) {
  global_State *g = G(L);
  GCObject *o;
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);

  o = luaC_newobjdt(L, LUA_TTHREAD, sizeof(LX), offsetof(LX, l));
  L1 = gco2th(o);

  setthvalue2s(L, L->top.p, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);

  memcpy(lua_getextraspace(L1), lua_getextraspace(g->mainthread),
         LUA_EXTRASPACE);
  luai_userstatethread(L, L1);
  stack_init(L1, L);
  lua_unlock(L);
  return L1;
}

void luaE_freethread (lua_State *L, lua_State *L1) {
  LX *l = fromstate(L1);
  luaF_closeupval(L1, L1->stack.p);
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L, L1);
  freestack(L1);
  luaM_free(L, l);
}

int luaE_resetthread (lua_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;
  setnilvalue(s2v(L->stack.p));
  ci->func.p = L->stack.p;
  ci->callstatus = CIST_C;
  if (status == LUA_YIELD)
    status = LUA_OK;
  L->status = LUA_OK;
  L->errfunc = 0;
  status = luaD_closeprotected(L, 1, status);
  if (status != LUA_OK)
    luaD_seterrorobj(L, status, L->stack.p + 1);
  else
    L->top.p = L->stack.p + 1;
  ci->top.p = L->top.p + LUA_MINSTACK;
  luaD_reallocstack(L, cast_int(ci->top.p - L->stack.p), 0);
  return status;
}

LUA_API int lua_closethread (lua_State *L, lua_State *from) {
  int status;
  lua_lock(L);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  status = luaE_resetthread(L, L->status);
  lua_unlock(L);
  return status;
}

LUA_API int lua_resetthread (lua_State *L) {
  return lua_closethread(L, NULL);
}

LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  int i;
  lua_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, LUA_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = LUA_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = luaC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);
  L->next = NULL;
  incnny(L);
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = luai_makeseed(L);
  g->gcstp = GCSTPGC;
  g->strt.size = g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
  g->gcstopem = 0;
  g->gcemergency = 0;
  g->finobj = g->tobefnz = g->fixedgc = NULL;
  g->firstold1 = g->survival = g->old1 = g->reallyold = NULL;
  g->finobjsur = g->finobjold1 = g->finobjrold = NULL;
  g->sweepgc = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->twups = NULL;
  g->totalbytes = sizeof(LG);
  g->GCdebt = 0;
  g->lastatomic = 0;
  setivalue(&g->nilvalue, 0);
  setgcparam(g->gcpause, LUAI_GCPAUSE);
  setgcparam(g->gcstepmul, LUAI_GCMUL);
  g->gcstepsize = LUAI_GCSTEPSIZE;
  setgcparam(g->genmajormul, LUAI_GENMAJORMUL);
  g->genminormul = LUAI_GENMINORMUL;
  for (i=0; i < LUA_NUMTAGS; i++) g->mt[i] = NULL;
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK) {

    close_state(L);
    L = NULL;
  }
  return L;
}

LUA_API void lua_close (lua_State *L) {
  lua_lock(L);
  L = G(L)->mainthread;
  close_state(L);
}

void luaE_warning (lua_State *L, const char *msg, int tocont) {
  lua_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}

void luaE_warnerror (lua_State *L, const char *where) {
  TValue *errobj = s2v(L->top.p - 1);
  const char *msg = (ttisstring(errobj))
                  ? getstr(tsvalue(errobj))
                  : "error object is not a string";

  luaE_warning(L, "error in ", 1);
  luaE_warning(L, where, 1);
  luaE_warning(L, " (", 1);
  luaE_warning(L, msg, 1);
  luaE_warning(L, ")", 0);
}
