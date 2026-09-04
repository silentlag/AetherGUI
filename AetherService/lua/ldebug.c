
#define ldebug_c
#define LUA_CORE

#include "lprefix.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"

#define LuaClosure(f)		((f) != NULL && (f)->c.tt == LUA_VLCL)

static const char *funcnamefromcall (lua_State *L, CallInfo *ci,
                                                   const char **name);

static const char strlocal[] = "local";
static const char strupval[] = "upvalue";

static int currentpc (CallInfo *ci) {
  lua_assert(isLua(ci));
  return pcRel(ci->u.l.savedpc, ci_func(ci)->p);
}

static int getbaseline (const Proto *f, int pc, int *basepc) {
  if (f->sizeabslineinfo == 0 || pc < f->abslineinfo[0].pc) {
    *basepc = -1;
    return f->linedefined;
  }
  else {
    int i = cast_uint(pc) / MAXIWTHABS - 1;

    lua_assert(i < 0 ||
              (i < f->sizeabslineinfo && f->abslineinfo[i].pc <= pc));
    while (i + 1 < f->sizeabslineinfo && pc >= f->abslineinfo[i + 1].pc)
      i++;
    *basepc = f->abslineinfo[i].pc;
    return f->abslineinfo[i].line;
  }
}

int luaG_getfuncline (const Proto *f, int pc) {
  if (f->lineinfo == NULL)
    return -1;
  else {
    int basepc;
    int baseline = getbaseline(f, pc, &basepc);
    while (basepc++ < pc) {
      lua_assert(f->lineinfo[basepc] != ABSLINEINFO);
      baseline += f->lineinfo[basepc];
    }
    return baseline;
  }
}

static int getcurrentline (CallInfo *ci) {
  return luaG_getfuncline(ci_func(ci)->p, currentpc(ci));
}

static void settraps (CallInfo *ci) {
  for (; ci != NULL; ci = ci->previous)
    if (isLua(ci))
      ci->u.l.trap = 1;
}

LUA_API void lua_sethook (lua_State *L, lua_Hook func, int mask, int count) {
  if (func == NULL || mask == 0) {
    mask = 0;
    func = NULL;
  }
  L->hook = func;
  L->basehookcount = count;
  resethookcount(L);
  L->hookmask = cast_byte(mask);
  if (mask)
    settraps(L->ci);
}

LUA_API lua_Hook lua_gethook (lua_State *L) {
  return L->hook;
}

LUA_API int lua_gethookmask (lua_State *L) {
  return L->hookmask;
}

LUA_API int lua_gethookcount (lua_State *L) {
  return L->basehookcount;
}

LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  int status;
  CallInfo *ci;
  if (level < 0) return 0;
  lua_lock(L);
  for (ci = L->ci; level > 0 && ci != &L->base_ci; ci = ci->previous)
    level--;
  if (level == 0 && ci != &L->base_ci) {
    status = 1;
    ar->i_ci = ci;
  }
  else status = 0;
  lua_unlock(L);
  return status;
}

static const char *upvalname (const Proto *p, int uv) {
  TString *s = check_exp(uv < p->sizeupvalues, p->upvalues[uv].name);
  if (s == NULL) return "?";
  else return getstr(s);
}

static const char *findvararg (CallInfo *ci, int n, StkId *pos) {
  if (clLvalue(s2v(ci->func.p))->p->is_vararg) {
    int nextra = ci->u.l.nextraargs;
    if (n >= -nextra) {
      *pos = ci->func.p - nextra - (n + 1);
      return "(vararg)";
    }
  }
  return NULL;
}

const char *luaG_findlocal (lua_State *L, CallInfo *ci, int n, StkId *pos) {
  StkId base = ci->func.p + 1;
  const char *name = NULL;
  if (isLua(ci)) {
    if (n < 0)
      return findvararg(ci, n, pos);
    else
      name = luaF_getlocalname(ci_func(ci)->p, n, currentpc(ci));
  }
  if (name == NULL) {
    StkId limit = (ci == L->ci) ? L->top.p : ci->next->func.p;
    if (limit - base >= n && n > 0) {

      name = isLua(ci) ? "(temporary)" : "(C temporary)";
    }
    else
      return NULL;
  }
  if (pos)
    *pos = base + (n - 1);
  return name;
}

LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  lua_lock(L);
  if (ar == NULL) {
    if (!isLfunction(s2v(L->top.p - 1)))
      name = NULL;
    else
      name = luaF_getlocalname(clLvalue(s2v(L->top.p - 1))->p, n, 0);
  }
  else {
    StkId pos = NULL;
    name = luaG_findlocal(L, ar->i_ci, n, &pos);
    if (name) {
      setobjs2s(L, L->top.p, pos);
      api_incr_top(L);
    }
  }
  lua_unlock(L);
  return name;
}

LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  StkId pos = NULL;
  const char *name;
  lua_lock(L);
  name = luaG_findlocal(L, ar->i_ci, n, &pos);
  if (name) {
    setobjs2s(L, pos, L->top.p - 1);
    L->top.p--;
  }
  lua_unlock(L);
  return name;
}

static void funcinfo (lua_Debug *ar, Closure *cl) {
  if (!LuaClosure(cl)) {
    ar->source = "=[C]";
    ar->srclen = LL("=[C]");
    ar->linedefined = -1;
    ar->lastlinedefined = -1;
    ar->what = "C";
  }
  else {
    const Proto *p = cl->l.p;
    if (p->source) {
      ar->source = getstr(p->source);
      ar->srclen = tsslen(p->source);
    }
    else {
      ar->source = "=?";
      ar->srclen = LL("=?");
    }
    ar->linedefined = p->linedefined;
    ar->lastlinedefined = p->lastlinedefined;
    ar->what = (ar->linedefined == 0) ? "main" : "Lua";
  }
  luaO_chunkid(ar->short_src, ar->source, ar->srclen);
}

static int nextline (const Proto *p, int currentline, int pc) {
  if (p->lineinfo[pc] != ABSLINEINFO)
    return currentline + p->lineinfo[pc];
  else
    return luaG_getfuncline(p, pc);
}

static void collectvalidlines (lua_State *L, Closure *f) {
  if (!LuaClosure(f)) {
    setnilvalue(s2v(L->top.p));
    api_incr_top(L);
  }
  else {
    const Proto *p = f->l.p;
    int currentline = p->linedefined;
    Table *t = luaH_new(L);
    sethvalue2s(L, L->top.p, t);
    api_incr_top(L);
    if (p->lineinfo != NULL) {
      int i;
      TValue v;
      setbtvalue(&v);
      if (!p->is_vararg)
        i = 0;
      else {
        lua_assert(GET_OPCODE(p->code[0]) == OP_VARARGPREP);
        currentline = nextline(p, currentline, 0);
        i = 1;
      }
      for (; i < p->sizelineinfo; i++) {
        currentline = nextline(p, currentline, i);
        luaH_setint(L, t, currentline, &v);
      }
    }
  }
}

static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name) {

  if (ci != NULL && !(ci->callstatus & CIST_TAIL))
    return funcnamefromcall(L, ci->previous, name);
  else return NULL;
}

static int auxgetinfo (lua_State *L, const char *what, lua_Debug *ar,
                       Closure *f, CallInfo *ci) {
  int status = 1;
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(ar, f);
        break;
      }
      case 'l': {
        ar->currentline = (ci && isLua(ci)) ? getcurrentline(ci) : -1;
        break;
      }
      case 'u': {
        ar->nups = (f == NULL) ? 0 : f->c.nupvalues;
        if (!LuaClosure(f)) {
          ar->isvararg = 1;
          ar->nparams = 0;
        }
        else {
          ar->isvararg = f->l.p->is_vararg;
          ar->nparams = f->l.p->numparams;
        }
        break;
      }
      case 't': {
        ar->istailcall = (ci) ? ci->callstatus & CIST_TAIL : 0;
        break;
      }
      case 'n': {
        ar->namewhat = getfuncname(L, ci, &ar->name);
        if (ar->namewhat == NULL) {
          ar->namewhat = "";
          ar->name = NULL;
        }
        break;
      }
      case 'r': {
        if (ci == NULL || !(ci->callstatus & CIST_TRAN))
          ar->ftransfer = ar->ntransfer = 0;
        else {
          ar->ftransfer = ci->u2.transferinfo.ftransfer;
          ar->ntransfer = ci->u2.transferinfo.ntransfer;
        }
        break;
      }
      case 'L':
      case 'f':
        break;
      default: status = 0;
    }
  }
  return status;
}

LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  int status;
  Closure *cl;
  CallInfo *ci;
  TValue *func;
  lua_lock(L);
  if (*what == '>') {
    ci = NULL;
    func = s2v(L->top.p - 1);
    api_check(L, ttisfunction(func), "function expected");
    what++;
    L->top.p--;
  }
  else {
    ci = ar->i_ci;
    func = s2v(ci->func.p);
    lua_assert(ttisfunction(func));
  }
  cl = ttisclosure(func) ? clvalue(func) : NULL;
  status = auxgetinfo(L, what, ar, cl, ci);
  if (strchr(what, 'f')) {
    setobj2s(L, L->top.p, func);
    api_incr_top(L);
  }
  if (strchr(what, 'L'))
    collectvalidlines(L, cl);
  lua_unlock(L);
  return status;
}

static int filterpc (int pc, int jmptarget) {
  if (pc < jmptarget)
    return -1;
  else return pc;
}

static int findsetreg (const Proto *p, int lastpc, int reg) {
  int pc;
  int setreg = -1;
  int jmptarget = 0;
  if (testMMMode(GET_OPCODE(p->code[lastpc])))
    lastpc--;
  for (pc = 0; pc < lastpc; pc++) {
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    int change;
    switch (op) {
      case OP_LOADNIL: {
        int b = GETARG_B(i);
        change = (a <= reg && reg <= a + b);
        break;
      }
      case OP_TFORCALL: {
        change = (reg >= a + 2);
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        change = (reg >= a);
        break;
      }
      case OP_JMP: {
        int b = GETARG_sJ(i);
        int dest = pc + 1 + b;

        if (dest <= lastpc && dest > jmptarget)
          jmptarget = dest;
        change = 0;
        break;
      }
      default:
        change = (testAMode(op) && reg == a);
        break;
    }
    if (change)
      setreg = filterpc(pc, jmptarget);
  }
  return setreg;
}

static const char *kname (const Proto *p, int index, const char **name) {
  TValue *kvalue = &p->k[index];
  if (ttisstring(kvalue)) {
    *name = getstr(tsvalue(kvalue));
    return "constant";
  }
  else {
    *name = "?";
    return NULL;
  }
}

static const char *basicgetobjname (const Proto *p, int *ppc, int reg,
                                    const char **name) {
  int pc = *ppc;
  *name = luaF_getlocalname(p, reg + 1, pc);
  if (*name)
    return strlocal;

  *ppc = pc = findsetreg(p, pc, reg);
  if (pc != -1) {
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    switch (op) {
      case OP_MOVE: {
        int b = GETARG_B(i);
        if (b < GETARG_A(i))
          return basicgetobjname(p, ppc, b, name);
        break;
      }
      case OP_GETUPVAL: {
        *name = upvalname(p, GETARG_B(i));
        return strupval;
      }
      case OP_LOADK: return kname(p, GETARG_Bx(i), name);
      case OP_LOADKX: return kname(p, GETARG_Ax(p->code[pc + 1]), name);
      default: break;
    }
  }
  return NULL;
}

static void rname (const Proto *p, int pc, int c, const char **name) {
  const char *what = basicgetobjname(p, &pc, c, name);
  if (!(what && *what == 'c'))
    *name = "?";
}

static void rkname (const Proto *p, int pc, Instruction i, const char **name) {
  int c = GETARG_C(i);
  if (GETARG_k(i))
    kname(p, c, name);
  else
    rname(p, pc, c, name);
}

static const char *isEnv (const Proto *p, int pc, Instruction i, int isup) {
  int t = GETARG_B(i);
  const char *name;
  if (isup)
    name = upvalname(p, t);
  else {
    const char *what = basicgetobjname(p, &pc, t, &name);
    if (what != strlocal && what != strupval)
      name = NULL;
  }
  return (name && strcmp(name, LUA_ENV) == 0) ? "global" : "field";
}

static const char *getobjname (const Proto *p, int lastpc, int reg,
                               const char **name) {
  const char *kind = basicgetobjname(p, &lastpc, reg, name);
  if (kind != NULL)
    return kind;
  else if (lastpc != -1) {
    Instruction i = p->code[lastpc];
    OpCode op = GET_OPCODE(i);
    switch (op) {
      case OP_GETTABUP: {
        int k = GETARG_C(i);
        kname(p, k, name);
        return isEnv(p, lastpc, i, 1);
      }
      case OP_GETTABLE: {
        int k = GETARG_C(i);
        rname(p, lastpc, k, name);
        return isEnv(p, lastpc, i, 0);
      }
      case OP_GETI: {
        *name = "integer index";
        return "field";
      }
      case OP_GETFIELD: {
        int k = GETARG_C(i);
        kname(p, k, name);
        return isEnv(p, lastpc, i, 0);
      }
      case OP_SELF: {
        rkname(p, lastpc, i, name);
        return "method";
      }
      default: break;
    }
  }
  return NULL;
}

static const char *funcnamefromcode (lua_State *L, const Proto *p,
                                     int pc, const char **name) {
  TMS tm = (TMS)0;
  Instruction i = p->code[pc];
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:
      return getobjname(p, pc, GETARG_A(i), name);
    case OP_TFORCALL: {
      *name = "for iterator";
       return "for iterator";
    }

    case OP_SELF: case OP_GETTABUP: case OP_GETTABLE:
    case OP_GETI: case OP_GETFIELD:
      tm = TM_INDEX;
      break;
    case OP_SETTABUP: case OP_SETTABLE: case OP_SETI: case OP_SETFIELD:
      tm = TM_NEWINDEX;
      break;
    case OP_MMBIN: case OP_MMBINI: case OP_MMBINK: {
      tm = cast(TMS, GETARG_C(i));
      break;
    }
    case OP_UNM: tm = TM_UNM; break;
    case OP_BNOT: tm = TM_BNOT; break;
    case OP_LEN: tm = TM_LEN; break;
    case OP_CONCAT: tm = TM_CONCAT; break;
    case OP_EQ: tm = TM_EQ; break;

    case OP_LT: case OP_LTI: case OP_GTI: tm = TM_LT; break;
    case OP_LE: case OP_LEI: case OP_GEI: tm = TM_LE; break;
    case OP_CLOSE: case OP_RETURN: tm = TM_CLOSE; break;
    default:
      return NULL;
  }
  *name = getshrstr(G(L)->tmname[tm]) + 2;
  return "metamethod";
}

static const char *funcnamefromcall (lua_State *L, CallInfo *ci,
                                                   const char **name) {
  if (ci->callstatus & CIST_HOOKED) {
    *name = "?";
    return "hook";
  }
  else if (ci->callstatus & CIST_FIN) {
    *name = "__gc";
    return "metamethod";
  }
  else if (isLua(ci))
    return funcnamefromcode(L, ci_func(ci)->p, currentpc(ci), name);
  else
    return NULL;
}

static int instack (CallInfo *ci, const TValue *o) {
  int pos;
  StkId base = ci->func.p + 1;
  for (pos = 0; base + pos < ci->top.p; pos++) {
    if (o == s2v(base + pos))
      return pos;
  }
  return -1;
}

static const char *getupvalname (CallInfo *ci, const TValue *o,
                                 const char **name) {
  LClosure *c = ci_func(ci);
  int i;
  for (i = 0; i < c->nupvalues; i++) {
    if (c->upvals[i]->v.p == o) {
      *name = upvalname(c->p, i);
      return strupval;
    }
  }
  return NULL;
}

static const char *formatvarinfo (lua_State *L, const char *kind,
                                                const char *name) {
  if (kind == NULL)
    return "";
  else
    return luaO_pushfstring(L, " (%s '%s')", kind, name);
}

static const char *varinfo (lua_State *L, const TValue *o) {
  CallInfo *ci = L->ci;
  const char *name = NULL;
  const char *kind = NULL;
  if (isLua(ci)) {
    kind = getupvalname(ci, o, &name);
    if (!kind) {
      int reg = instack(ci, o);
      if (reg >= 0)
        kind = getobjname(ci_func(ci)->p, currentpc(ci), reg, &name);
    }
  }
  return formatvarinfo(L, kind, name);
}

static l_noret typeerror (lua_State *L, const TValue *o, const char *op,
                          const char *extra) {
  const char *t = luaT_objtypename(L, o);
  luaG_runerror(L, "attempt to %s a %s value%s", op, t, extra);
}

l_noret luaG_typeerror (lua_State *L, const TValue *o, const char *op) {
  typeerror(L, o, op, varinfo(L, o));
}

l_noret luaG_callerror (lua_State *L, const TValue *o) {
  CallInfo *ci = L->ci;
  const char *name = NULL;
  const char *kind = funcnamefromcall(L, ci, &name);
  const char *extra = kind ? formatvarinfo(L, kind, name) : varinfo(L, o);
  typeerror(L, o, "call", extra);
}

l_noret luaG_forerror (lua_State *L, const TValue *o, const char *what) {
  luaG_runerror(L, "bad 'for' %s (number expected, got %s)",
                   what, luaT_objtypename(L, o));
}

l_noret luaG_concaterror (lua_State *L, const TValue *p1, const TValue *p2) {
  if (ttisstring(p1) || cvt2str(p1)) p1 = p2;
  luaG_typeerror(L, p1, "concatenate");
}

l_noret luaG_opinterror (lua_State *L, const TValue *p1,
                         const TValue *p2, const char *msg) {
  if (!ttisnumber(p1))
    p2 = p1;
  luaG_typeerror(L, p2, msg);
}

l_noret luaG_tointerror (lua_State *L, const TValue *p1, const TValue *p2) {
  lua_Integer temp;
  if (!luaV_tointegerns(p1, &temp, LUA_FLOORN2I))
    p2 = p1;
  luaG_runerror(L, "number%s has no integer representation", varinfo(L, p2));
}

l_noret luaG_ordererror (lua_State *L, const TValue *p1, const TValue *p2) {
  const char *t1 = luaT_objtypename(L, p1);
  const char *t2 = luaT_objtypename(L, p2);
  if (strcmp(t1, t2) == 0)
    luaG_runerror(L, "attempt to compare two %s values", t1);
  else
    luaG_runerror(L, "attempt to compare %s with %s", t1, t2);
}

const char *luaG_addinfo (lua_State *L, const char *msg, TString *src,
                                        int line) {
  char buff[LUA_IDSIZE];
  if (src)
    luaO_chunkid(buff, getstr(src), tsslen(src));
  else {
    buff[0] = '?'; buff[1] = '\0';
  }
  return luaO_pushfstring(L, "%s:%d: %s", buff, line, msg);
}

l_noret luaG_errormsg (lua_State *L) {
  if (L->errfunc != 0) {
    StkId errfunc = restorestack(L, L->errfunc);
    lua_assert(ttisfunction(s2v(errfunc)));
    setobjs2s(L, L->top.p, L->top.p - 1);
    setobjs2s(L, L->top.p - 1, errfunc);
    L->top.p++;
    luaD_callnoyield(L, L->top.p - 2, 1);
  }
  luaD_throw(L, LUA_ERRRUN);
}

l_noret luaG_runerror (lua_State *L, const char *fmt, ...) {
  CallInfo *ci = L->ci;
  const char *msg;
  va_list argp;
  luaC_checkGC(L);
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  if (isLua(ci)) {
    luaG_addinfo(L, msg, ci_func(ci)->p->source, getcurrentline(ci));
    setobjs2s(L, L->top.p - 2, L->top.p - 1);
    L->top.p--;
  }
  luaG_errormsg(L);
}

static int changedline (const Proto *p, int oldpc, int newpc) {
  if (p->lineinfo == NULL)
    return 0;
  if (newpc - oldpc < MAXIWTHABS / 2) {
    int delta = 0;
    int pc = oldpc;
    for (;;) {
      int lineinfo = p->lineinfo[++pc];
      if (lineinfo == ABSLINEINFO)
        break;
      delta += lineinfo;
      if (pc == newpc)
        return (delta != 0);
    }
  }

  return (luaG_getfuncline(p, oldpc) != luaG_getfuncline(p, newpc));
}

int luaG_tracecall (lua_State *L) {
  CallInfo *ci = L->ci;
  Proto *p = ci_func(ci)->p;
  ci->u.l.trap = 1;
  if (ci->u.l.savedpc == p->code) {
    if (p->is_vararg)
      return 0;
    else if (!(ci->callstatus & CIST_HOOKYIELD))
      luaD_hookcall(L, ci);
  }
  return 1;
}

int luaG_traceexec (lua_State *L, const Instruction *pc) {
  CallInfo *ci = L->ci;
  lu_byte mask = L->hookmask;
  const Proto *p = ci_func(ci)->p;
  int counthook;
  if (!(mask & (LUA_MASKLINE | LUA_MASKCOUNT))) {
    ci->u.l.trap = 0;
    return 0;
  }
  pc++;
  ci->u.l.savedpc = pc;
  counthook = (mask & LUA_MASKCOUNT) && (--L->hookcount == 0);
  if (counthook)
    resethookcount(L);
  else if (!(mask & LUA_MASKLINE))
    return 1;
  if (ci->callstatus & CIST_HOOKYIELD) {
    ci->callstatus &= ~CIST_HOOKYIELD;
    return 1;
  }
  if (!isIT(*(ci->u.l.savedpc - 1)))
    L->top.p = ci->top.p;
  if (counthook)
    luaD_hook(L, LUA_HOOKCOUNT, -1, 0, 0);
  if (mask & LUA_MASKLINE) {

    int oldpc = (L->oldpc < p->sizecode) ? L->oldpc : 0;
    int npci = pcRel(pc, p);
    if (npci <= oldpc ||
        changedline(p, oldpc, npci)) {
      int newline = luaG_getfuncline(p, npci);
      luaD_hook(L, LUA_HOOKLINE, newline, 0, 0);
    }
    L->oldpc = npci;
  }
  if (L->status == LUA_YIELD) {
    if (counthook)
      L->hookcount = 1;
    ci->callstatus |= CIST_HOOKYIELD;
    luaD_throw(L, LUA_YIELD);
  }
  return 1;
}
