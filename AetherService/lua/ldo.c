
#define ldo_c
#define LUA_CORE

#include "lprefix.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"

#define errorstatus(s)	((s) > LUA_YIELD)

#if !defined(LUAI_THROW)

#if defined(__cplusplus) && !defined(LUA_USE_LONGJMP)

#define LUAI_THROW(L,c)		throw(c)
#define LUAI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define luai_jmpbuf		int

#elif defined(LUA_USE_POSIX)

#define LUAI_THROW(L,c)		_longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#else

#define LUAI_THROW(L,c)		longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#endif

#endif

struct lua_longjmp {
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;
};

void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {
      setsvalue2s(L, oldtop, G(L)->memerrmsg);
      break;
    }
    case LUA_OK: {
      setnilvalue(s2v(oldtop));
      break;
    }
    default: {
      lua_assert(errorstatus(errcode));
      setobjs2s(L, oldtop, L->top.p - 1);
      break;
    }
  }
  L->top.p = oldtop + 1;
}

l_noret luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    LUAI_THROW(L, L->errorJmp);
  }
  else {
    global_State *g = G(L);
    errcode = luaE_resetthread(L, errcode);
    L->status = errcode;
    if (g->mainthread->errorJmp) {
      setobjs2s(L, g->mainthread->top.p++, L->top.p - 1);
      luaD_throw(g->mainthread, errcode);
    }
    else {
      if (g->panic) {
        lua_unlock(L);
        g->panic(L);
      }
      abort();
    }
  }
}

int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  l_uint32 oldnCcalls = L->nCcalls;
  struct lua_longjmp lj;
  lj.status = LUA_OK;
  lj.previous = L->errorJmp;
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;
  L->nCcalls = oldnCcalls;
  return lj.status;
}

static void relstack (lua_State *L) {
  CallInfo *ci;
  UpVal *up;
  L->top.offset = savestack(L, L->top.p);
  L->tbclist.offset = savestack(L, L->tbclist.p);
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.offset = savestack(L, uplevel(up));
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.offset = savestack(L, ci->top.p);
    ci->func.offset = savestack(L, ci->func.p);
  }
}

static void correctstack (lua_State *L) {
  CallInfo *ci;
  UpVal *up;
  L->top.p = restorestack(L, L->top.offset);
  L->tbclist.p = restorestack(L, L->tbclist.offset);
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.p = s2v(restorestack(L, up->v.offset));
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.p = restorestack(L, ci->top.offset);
    ci->func.p = restorestack(L, ci->func.offset);
    if (isLua(ci))
      ci->u.l.trap = 1;
  }
}

#define ERRORSTACKSIZE	(LUAI_MAXSTACK + 200)

l_noret luaD_errerr (lua_State *L) {
  TString *msg = luaS_newliteral(L, "error in error handling");
  setsvalue2s(L, L->top.p, msg);
  L->top.p++;
  luaD_throw(L, LUA_ERRERR);
}

int luaD_reallocstack (lua_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int i;
  StkId newstack;
  int oldgcstop = G(L)->gcstopem;
  lua_assert(newsize <= LUAI_MAXSTACK || newsize == ERRORSTACKSIZE);
  relstack(L);
  G(L)->gcstopem = 1;
  newstack = luaM_reallocvector(L, L->stack.p, oldsize + EXTRA_STACK,
                                   newsize + EXTRA_STACK, StackValue);
  G(L)->gcstopem = oldgcstop;
  if (l_unlikely(newstack == NULL)) {
    correctstack(L);
    if (raiseerror)
      luaM_error(L);
    else return 0;
  }
  L->stack.p = newstack;
  correctstack(L);
  L->stack_last.p = L->stack.p + newsize;
  for (i = oldsize + EXTRA_STACK; i < newsize + EXTRA_STACK; i++)
    setnilvalue(s2v(newstack + i));
  return 1;
}

int luaD_growstack (lua_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (l_unlikely(size > LUAI_MAXSTACK)) {

    lua_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      luaD_errerr(L);
    return 0;
  }
  else if (n < LUAI_MAXSTACK) {
    int newsize = 2 * size;
    int needed = cast_int(L->top.p - L->stack.p) + n;
    if (newsize > LUAI_MAXSTACK)
      newsize = LUAI_MAXSTACK;
    if (newsize < needed)
      newsize = needed;
    if (l_likely(newsize <= LUAI_MAXSTACK))
      return luaD_reallocstack(L, newsize, raiseerror);
  }

  luaD_reallocstack(L, ERRORSTACKSIZE, raiseerror);
  if (raiseerror)
    luaG_runerror(L, "stack overflow");
  return 0;
}

static int stackinuse (lua_State *L) {
  CallInfo *ci;
  int res;
  StkId lim = L->top.p;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top.p) lim = ci->top.p;
  }
  lua_assert(lim <= L->stack_last.p + EXTRA_STACK);
  res = cast_int(lim - L->stack.p) + 1;
  if (res < LUA_MINSTACK)
    res = LUA_MINSTACK;
  return res;
}

void luaD_shrinkstack (lua_State *L) {
  int inuse = stackinuse(L);
  int max = (inuse > LUAI_MAXSTACK / 3) ? LUAI_MAXSTACK : inuse * 3;

  if (inuse <= LUAI_MAXSTACK && stacksize(L) > max) {
    int nsize = (inuse > LUAI_MAXSTACK / 2) ? LUAI_MAXSTACK : inuse * 2;
    luaD_reallocstack(L, nsize, 0);
  }
  else
    condmovestack(L,{},{});
  luaE_shrinkCI(L);
}

void luaD_inctop (lua_State *L) {
  luaD_checkstack(L, 1);
  L->top.p++;
}

void luaD_hook (lua_State *L, int event, int line,
                              int ftransfer, int ntransfer) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    int mask = CIST_HOOKED;
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top.p);
    ptrdiff_t ci_top = savestack(L, ci->top.p);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    if (ntransfer != 0) {
      mask |= CIST_TRAN;
      ci->u2.transferinfo.ftransfer = ftransfer;
      ci->u2.transferinfo.ntransfer = ntransfer;
    }
    if (isLua(ci) && L->top.p < ci->top.p)
      L->top.p = ci->top.p;
    luaD_checkstack(L, LUA_MINSTACK);
    if (ci->top.p < L->top.p + LUA_MINSTACK)
      ci->top.p = L->top.p + LUA_MINSTACK;
    L->allowhook = 0;
    ci->callstatus |= mask;
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top.p = restorestack(L, ci_top);
    L->top.p = restorestack(L, top);
    ci->callstatus &= ~mask;
  }
}

void luaD_hookcall (lua_State *L, CallInfo *ci) {
  L->oldpc = 0;
  if (L->hookmask & LUA_MASKCALL) {
    int event = (ci->callstatus & CIST_TAIL) ? LUA_HOOKTAILCALL
                                             : LUA_HOOKCALL;
    Proto *p = ci_func(ci)->p;
    ci->u.l.savedpc++;
    luaD_hook(L, event, -1, 1, p->numparams);
    ci->u.l.savedpc--;
  }
}

static void rethook (lua_State *L, CallInfo *ci, int nres) {
  if (L->hookmask & LUA_MASKRET) {
    StkId firstres = L->top.p - nres;
    int delta = 0;
    int ftransfer;
    if (isLua(ci)) {
      Proto *p = ci_func(ci)->p;
      if (p->is_vararg)
        delta = ci->u.l.nextraargs + p->numparams + 1;
    }
    ci->func.p += delta;
    ftransfer = cast(unsigned short, firstres - ci->func.p);
    luaD_hook(L, LUA_HOOKRET, -1, ftransfer, nres);
    ci->func.p -= delta;
  }
  if (isLua(ci = ci->previous))
    L->oldpc = pcRel(ci->u.l.savedpc, ci_func(ci)->p);
}

static StkId tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm;
  StkId p;
  checkstackGCp(L, 1, func);
  tm = luaT_gettmbyobj(L, s2v(func), TM_CALL);
  if (l_unlikely(ttisnil(tm)))
    luaG_callerror(L, s2v(func));
  for (p = L->top.p; p > func; p--)
    setobjs2s(L, p, p-1);
  L->top.p++;
  setobj2s(L, func, tm);
  return func;
}

l_sinline void moveresults (lua_State *L, StkId res, int nres, int wanted) {
  StkId firstresult;
  int i;
  switch (wanted) {
    case 0:
      L->top.p = res;
      return;
    case 1:
      if (nres == 0)
        setnilvalue(s2v(res));
      else
        setobjs2s(L, res, L->top.p - nres);
      L->top.p = res + 1;
      return;
    case LUA_MULTRET:
      wanted = nres;
      break;
    default:
      if (hastocloseCfunc(wanted)) {
        L->ci->callstatus |= CIST_CLSRET;
        L->ci->u2.nres = nres;
        res = luaF_close(L, res, CLOSEKTOP, 1);
        L->ci->callstatus &= ~CIST_CLSRET;
        if (L->hookmask) {
          ptrdiff_t savedres = savestack(L, res);
          rethook(L, L->ci, nres);
          res = restorestack(L, savedres);
        }
        wanted = decodeNresults(wanted);
        if (wanted == LUA_MULTRET)
          wanted = nres;
      }
      break;
  }

  firstresult = L->top.p - nres;
  if (nres > wanted)
    nres = wanted;
  for (i = 0; i < nres; i++)
    setobjs2s(L, res + i, firstresult + i);
  for (; i < wanted; i++)
    setnilvalue(s2v(res + i));
  L->top.p = res + wanted;
}

void luaD_poscall (lua_State *L, CallInfo *ci, int nres) {
  int wanted = ci->nresults;
  if (l_unlikely(L->hookmask && !hastocloseCfunc(wanted)))
    rethook(L, ci, nres);

  moveresults(L, ci->func.p, nres, wanted);

  lua_assert(!(ci->callstatus &
        (CIST_HOOKED | CIST_YPCALL | CIST_FIN | CIST_TRAN | CIST_CLSRET)));
  L->ci = ci->previous;
}

#define next_ci(L)  (L->ci->next ? L->ci->next : luaE_extendCI(L))

l_sinline CallInfo *prepCallInfo (lua_State *L, StkId func, int nret,
                                                int mask, StkId top) {
  CallInfo *ci = L->ci = next_ci(L);
  ci->func.p = func;
  ci->nresults = nret;
  ci->callstatus = mask;
  ci->top.p = top;
  return ci;
}

l_sinline int precallC (lua_State *L, StkId func, int nresults,
                                            lua_CFunction f) {
  int n;
  CallInfo *ci;
  checkstackGCp(L, LUA_MINSTACK, func);
  L->ci = ci = prepCallInfo(L, func, nresults, CIST_C,
                               L->top.p + LUA_MINSTACK);
  lua_assert(ci->top.p <= L->stack_last.p);
  if (l_unlikely(L->hookmask & LUA_MASKCALL)) {
    int narg = cast_int(L->top.p - func) - 1;
    luaD_hook(L, LUA_HOOKCALL, -1, 1, narg);
  }
  lua_unlock(L);
  n = (*f)(L);
  lua_lock(L);
  api_checknelems(L, n);
  luaD_poscall(L, ci, n);
  return n;
}

int luaD_pretailcall (lua_State *L, CallInfo *ci, StkId func,
                                    int narg1, int delta) {
 retry:
  switch (ttypetag(s2v(func))) {
    case LUA_VCCL:
      return precallC(L, func, LUA_MULTRET, clCvalue(s2v(func))->f);
    case LUA_VLCF:
      return precallC(L, func, LUA_MULTRET, fvalue(s2v(func)));
    case LUA_VLCL: {
      Proto *p = clLvalue(s2v(func))->p;
      int fsize = p->maxstacksize;
      int nfixparams = p->numparams;
      int i;
      checkstackGCp(L, fsize - delta, func);
      ci->func.p -= delta;
      for (i = 0; i < narg1; i++)
        setobjs2s(L, ci->func.p + i, func + i);
      func = ci->func.p;
      for (; narg1 <= nfixparams; narg1++)
        setnilvalue(s2v(func + narg1));
      ci->top.p = func + 1 + fsize;
      lua_assert(ci->top.p <= L->stack_last.p);
      ci->u.l.savedpc = p->code;
      ci->callstatus |= CIST_TAIL;
      L->top.p = func + narg1;
      return -1;
    }
    default: {
      func = tryfuncTM(L, func);

      narg1++;
      goto retry;
    }
  }
}

CallInfo *luaD_precall (lua_State *L, StkId func, int nresults) {
 retry:
  switch (ttypetag(s2v(func))) {
    case LUA_VCCL:
      precallC(L, func, nresults, clCvalue(s2v(func))->f);
      return NULL;
    case LUA_VLCF:
      precallC(L, func, nresults, fvalue(s2v(func)));
      return NULL;
    case LUA_VLCL: {
      CallInfo *ci;
      Proto *p = clLvalue(s2v(func))->p;
      int narg = cast_int(L->top.p - func) - 1;
      int nfixparams = p->numparams;
      int fsize = p->maxstacksize;
      checkstackGCp(L, fsize, func);
      L->ci = ci = prepCallInfo(L, func, nresults, 0, func + 1 + fsize);
      ci->u.l.savedpc = p->code;
      for (; narg < nfixparams; narg++)
        setnilvalue(s2v(L->top.p++));
      lua_assert(ci->top.p <= L->stack_last.p);
      return ci;
    }
    default: {
      func = tryfuncTM(L, func);

      goto retry;
    }
  }
}

l_sinline void ccall (lua_State *L, StkId func, int nResults, l_uint32 inc) {
  CallInfo *ci;
  L->nCcalls += inc;
  if (l_unlikely(getCcalls(L) >= LUAI_MAXCCALLS)) {
    checkstackp(L, 0, func);
    luaE_checkcstack(L);
  }
  if ((ci = luaD_precall(L, func, nResults)) != NULL) {
    ci->callstatus = CIST_FRESH;
    luaV_execute(L, ci);
  }
  L->nCcalls -= inc;
}

void luaD_call (lua_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, 1);
}

void luaD_callnoyield (lua_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, nyci);
}

static int finishpcallk (lua_State *L,  CallInfo *ci) {
  int status = getcistrecst(ci);
  if (l_likely(status == LUA_OK))
    status = LUA_YIELD;
  else {
    StkId func = restorestack(L, ci->u2.funcidx);
    L->allowhook = getoah(ci->callstatus);
    func = luaF_close(L, func, status, 1);
    luaD_seterrorobj(L, status, func);
    luaD_shrinkstack(L);
    setcistrecst(ci, LUA_OK);
  }
  ci->callstatus &= ~CIST_YPCALL;
  L->errfunc = ci->u.c.old_errfunc;

  return status;
}

static void finishCcall (lua_State *L, CallInfo *ci) {
  int n;
  if (ci->callstatus & CIST_CLSRET) {
    lua_assert(hastocloseCfunc(ci->nresults));
    n = ci->u2.nres;

  }
  else {
    int status = LUA_YIELD;

    lua_assert(ci->u.c.k != NULL && yieldable(L));
    if (ci->callstatus & CIST_YPCALL)
      status = finishpcallk(L, ci);
    adjustresults(L, LUA_MULTRET);
    lua_unlock(L);
    n = (*ci->u.c.k)(L, status, ci->u.c.ctx);
    lua_lock(L);
    api_checknelems(L, n);
  }
  luaD_poscall(L, ci, n);
}

static void unroll (lua_State *L, void *ud) {
  CallInfo *ci;
  UNUSED(ud);
  while ((ci = L->ci) != &L->base_ci) {
    if (!isLua(ci))
      finishCcall(L, ci);
    else {
      luaV_finishOp(L);
      luaV_execute(L, ci);
    }
  }
}

static CallInfo *findpcall (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;
}

static int resume_error (lua_State *L, const char *msg, int narg) {
  L->top.p -= narg;
  setsvalue2s(L, L->top.p, luaS_new(L, msg));
  api_incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}

static void resume (lua_State *L, void *ud) {
  int n = *(cast(int*, ud));
  StkId firstArg = L->top.p - n;
  CallInfo *ci = L->ci;
  if (L->status == LUA_OK)
    ccall(L, firstArg - 1, LUA_MULTRET, 0);
  else {
    lua_assert(L->status == LUA_YIELD);
    L->status = LUA_OK;
    if (isLua(ci)) {

      lua_assert(ci->callstatus & CIST_HOOKYIELD);
      ci->u.l.savedpc--;
      L->top.p = firstArg;
      luaV_execute(L, ci);
    }
    else {
      if (ci->u.c.k != NULL) {
        lua_unlock(L);
        n = (*ci->u.c.k)(L, LUA_YIELD, ci->u.c.ctx);
        lua_lock(L);
        api_checknelems(L, n);
      }
      luaD_poscall(L, ci, n);
    }
    unroll(L, NULL);
  }
}

static int precover (lua_State *L, int status) {
  CallInfo *ci;
  while (errorstatus(status) && (ci = findpcall(L)) != NULL) {
    L->ci = ci;
    setcistrecst(ci, status);
    status = luaD_rawrunprotected(L, unroll, NULL);
  }
  return status;
}

LUA_API int lua_resume (lua_State *L, lua_State *from, int nargs,
                                      int *nresults) {
  int status;
  lua_lock(L);
  if (L->status == LUA_OK) {
    if (L->ci != &L->base_ci)
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
    else if (L->top.p - (L->ci->func.p + 1) == nargs)
      return resume_error(L, "cannot resume dead coroutine", nargs);
  }
  else if (L->status != LUA_YIELD)
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  if (getCcalls(L) >= LUAI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  L->nCcalls++;
  luai_userstateresume(L, nargs);
  api_checknelems(L, (L->status == LUA_OK) ? nargs + 1 : nargs);
  status = luaD_rawrunprotected(L, resume, &nargs);

  status = precover(L, status);
  if (l_likely(!errorstatus(status)))
    lua_assert(status == L->status);
  else {
    L->status = cast_byte(status);
    luaD_seterrorobj(L, status, L->top.p);
    L->ci->top.p = L->top.p;
  }
  *nresults = (status == LUA_YIELD) ? L->ci->u2.nyield
                                    : cast_int(L->top.p - (L->ci->func.p + 1));
  lua_unlock(L);
  return status;
}

LUA_API int lua_isyieldable (lua_State *L) {
  return yieldable(L);
}

LUA_API int lua_yieldk (lua_State *L, int nresults, lua_KContext ctx,
                        lua_KFunction k) {
  CallInfo *ci;
  luai_userstateyield(L, nresults);
  lua_lock(L);
  ci = L->ci;
  api_checknelems(L, nresults);
  if (l_unlikely(!yieldable(L))) {
    if (L != G(L)->mainthread)
      luaG_runerror(L, "attempt to yield across a C-call boundary");
    else
      luaG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = LUA_YIELD;
  ci->u2.nyield = nresults;
  if (isLua(ci)) {
    lua_assert(!isLuacode(ci));
    api_check(L, nresults == 0, "hooks cannot yield values");
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)
      ci->u.c.ctx = ctx;
    luaD_throw(L, LUA_YIELD);
  }
  lua_assert(ci->callstatus & CIST_HOOKED);
  lua_unlock(L);
  return 0;
}

struct CloseP {
  StkId level;
  int status;
};

static void closepaux (lua_State *L, void *ud) {
  struct CloseP *pcl = cast(struct CloseP *, ud);
  luaF_close(L, pcl->level, pcl->status, 0);
}

int luaD_closeprotected (lua_State *L, ptrdiff_t level, int status) {
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  for (;;) {
    struct CloseP pcl;
    pcl.level = restorestack(L, level); pcl.status = status;
    status = luaD_rawrunprotected(L, &closepaux, &pcl);
    if (l_likely(status == LUA_OK))
      return pcl.status;
    else {
      L->ci = old_ci;
      L->allowhook = old_allowhooks;
    }
  }
}

int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (l_unlikely(status != LUA_OK)) {
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    status = luaD_closeprotected(L, old_top, status);
    luaD_seterrorobj(L, status, restorestack(L, old_top));
    luaD_shrinkstack(L);
  }
  L->errfunc = old_errfunc;
  return status;
}

struct SParser {
  ZIO *z;
  Mbuffer buff;
  Dyndata dyd;
  const char *mode;
  const char *name;
};

static void checkmode (lua_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    luaO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    luaD_throw(L, LUA_ERRSYNTAX);
  }
}

static void f_parser (lua_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);
  if (c == LUA_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = luaU_undump(L, p->z, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = luaY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  luaF_initupvals(L, cl);
}

int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  incnny(L);
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top.p), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  luaM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  luaM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  luaM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  decnny(L);
  return status;
}
