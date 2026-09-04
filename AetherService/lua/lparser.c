
#define lparser_c
#define LUA_CORE

#include "lprefix.h"

#include <limits.h>
#include <string.h>

#include "lua.h"

#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"

#define MAXVARS		200

#define hasmultret(k)		((k) == VCALL || (k) == VVARARG)

#define eqstr(a,b)	((a) == (b))

typedef struct BlockCnt {
  struct BlockCnt *previous;
  int firstlabel;
  int firstgoto;
  lu_byte nactvar;
  lu_byte upval;
  lu_byte isloop;
  lu_byte insidetbc;
} BlockCnt;

static void statement (LexState *ls);
static void expr (LexState *ls, expdesc *v);

static l_noret error_expected (LexState *ls, int token) {
  luaX_syntaxerror(ls,
      luaO_pushfstring(ls->L, "%s expected", luaX_token2str(ls, token)));
}

static l_noret errorlimit (FuncState *fs, int limit, const char *what) {
  lua_State *L = fs->ls->L;
  const char *msg;
  int line = fs->f->linedefined;
  const char *where = (line == 0)
                      ? "main function"
                      : luaO_pushfstring(L, "function at line %d", line);
  msg = luaO_pushfstring(L, "too many %s (limit is %d) in %s",
                             what, limit, where);
  luaX_syntaxerror(fs->ls, msg);
}

static void checklimit (FuncState *fs, int v, int l, const char *what) {
  if (v > l) errorlimit(fs, l, what);
}

static int testnext (LexState *ls, int c) {
  if (ls->t.token == c) {
    luaX_next(ls);
    return 1;
  }
  else return 0;
}

static void check (LexState *ls, int c) {
  if (ls->t.token != c)
    error_expected(ls, c);
}

static void checknext (LexState *ls, int c) {
  check(ls, c);
  luaX_next(ls);
}

#define check_condition(ls,c,msg)	{ if (!(c)) luaX_syntaxerror(ls, msg); }

static void check_match (LexState *ls, int what, int who, int where) {
  if (l_unlikely(!testnext(ls, what))) {
    if (where == ls->linenumber)
      error_expected(ls, what);
    else {
      luaX_syntaxerror(ls, luaO_pushfstring(ls->L,
             "%s expected (to close %s at line %d)",
              luaX_token2str(ls, what), luaX_token2str(ls, who), where));
    }
  }
}

static TString *str_checkname (LexState *ls) {
  TString *ts;
  check(ls, TK_NAME);
  ts = ls->t.seminfo.ts;
  luaX_next(ls);
  return ts;
}

static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.info = i;
}

static void codestring (expdesc *e, TString *s) {
  e->f = e->t = NO_JUMP;
  e->k = VKSTR;
  e->u.strval = s;
}

static void codename (LexState *ls, expdesc *e) {
  codestring(e, str_checkname(ls));
}

static int registerlocalvar (LexState *ls, FuncState *fs, TString *varname) {
  Proto *f = fs->f;
  int oldsize = f->sizelocvars;
  luaM_growvector(ls->L, f->locvars, fs->ndebugvars, f->sizelocvars,
                  LocVar, SHRT_MAX, "local variables");
  while (oldsize < f->sizelocvars)
    f->locvars[oldsize++].varname = NULL;
  f->locvars[fs->ndebugvars].varname = varname;
  f->locvars[fs->ndebugvars].startpc = fs->pc;
  luaC_objbarrier(ls->L, f, varname);
  return fs->ndebugvars++;
}

static int new_localvar (LexState *ls, TString *name) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Dyndata *dyd = ls->dyd;
  Vardesc *var;
  checklimit(fs, dyd->actvar.n + 1 - fs->firstlocal,
                 MAXVARS, "local variables");
  luaM_growvector(L, dyd->actvar.arr, dyd->actvar.n + 1,
                  dyd->actvar.size, Vardesc, SHRT_MAX, "local variables");
  var = &dyd->actvar.arr[dyd->actvar.n++];
  var->vd.kind = VDKREG;
  var->vd.name = name;
  return dyd->actvar.n - 1 - fs->firstlocal;
}

#define new_localvarliteral(ls,v) \
    new_localvar(ls,  \
      luaX_newstring(ls, "" v, (sizeof(v)/sizeof(char)) - 1));

static Vardesc *getlocalvardesc (FuncState *fs, int vidx) {
  return &fs->ls->dyd->actvar.arr[fs->firstlocal + vidx];
}

static int reglevel (FuncState *fs, int nvar) {
  while (nvar-- > 0) {
    Vardesc *vd = getlocalvardesc(fs, nvar);
    if (vd->vd.kind != RDKCTC)
      return vd->vd.ridx + 1;
  }
  return 0;
}

int luaY_nvarstack (FuncState *fs) {
  return reglevel(fs, fs->nactvar);
}

static LocVar *localdebuginfo (FuncState *fs, int vidx) {
  Vardesc *vd = getlocalvardesc(fs,  vidx);
  if (vd->vd.kind == RDKCTC)
    return NULL;
  else {
    int idx = vd->vd.pidx;
    lua_assert(idx < fs->ndebugvars);
    return &fs->f->locvars[idx];
  }
}

static void init_var (FuncState *fs, expdesc *e, int vidx) {
  e->f = e->t = NO_JUMP;
  e->k = VLOCAL;
  e->u.var.vidx = vidx;
  e->u.var.ridx = getlocalvardesc(fs, vidx)->vd.ridx;
}

static void check_readonly (LexState *ls, expdesc *e) {
  FuncState *fs = ls->fs;
  TString *varname = NULL;
  switch (e->k) {
    case VCONST: {
      varname = ls->dyd->actvar.arr[e->u.info].vd.name;
      break;
    }
    case VLOCAL: {
      Vardesc *vardesc = getlocalvardesc(fs, e->u.var.vidx);
      if (vardesc->vd.kind != VDKREG)
        varname = vardesc->vd.name;
      break;
    }
    case VUPVAL: {
      Upvaldesc *up = &fs->f->upvalues[e->u.info];
      if (up->kind != VDKREG)
        varname = up->name;
      break;
    }
    default:
      return;
  }
  if (varname) {
    const char *msg = luaO_pushfstring(ls->L,
       "attempt to assign to const variable '%s'", getstr(varname));
    luaK_semerror(ls, msg);
  }
}

static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  int reglevel = luaY_nvarstack(fs);
  int i;
  for (i = 0; i < nvars; i++) {
    int vidx = fs->nactvar++;
    Vardesc *var = getlocalvardesc(fs, vidx);
    var->vd.ridx = reglevel++;
    var->vd.pidx = registerlocalvar(ls, fs, var->vd.name);
  }
}

static void removevars (FuncState *fs, int tolevel) {
  fs->ls->dyd->actvar.n -= (fs->nactvar - tolevel);
  while (fs->nactvar > tolevel) {
    LocVar *var = localdebuginfo(fs, --fs->nactvar);
    if (var)
      var->endpc = fs->pc;
  }
}

static int searchupvalue (FuncState *fs, TString *name) {
  int i;
  Upvaldesc *up = fs->f->upvalues;
  for (i = 0; i < fs->nups; i++) {
    if (eqstr(up[i].name, name)) return i;
  }
  return -1;
}

static Upvaldesc *allocupvalue (FuncState *fs) {
  Proto *f = fs->f;
  int oldsize = f->sizeupvalues;
  checklimit(fs, fs->nups + 1, MAXUPVAL, "upvalues");
  luaM_growvector(fs->ls->L, f->upvalues, fs->nups, f->sizeupvalues,
                  Upvaldesc, MAXUPVAL, "upvalues");
  while (oldsize < f->sizeupvalues)
    f->upvalues[oldsize++].name = NULL;
  return &f->upvalues[fs->nups++];
}

static int newupvalue (FuncState *fs, TString *name, expdesc *v) {
  Upvaldesc *up = allocupvalue(fs);
  FuncState *prev = fs->prev;
  if (v->k == VLOCAL) {
    up->instack = 1;
    up->idx = v->u.var.ridx;
    up->kind = getlocalvardesc(prev, v->u.var.vidx)->vd.kind;
    lua_assert(eqstr(name, getlocalvardesc(prev, v->u.var.vidx)->vd.name));
  }
  else {
    up->instack = 0;
    up->idx = cast_byte(v->u.info);
    up->kind = prev->f->upvalues[v->u.info].kind;
    lua_assert(eqstr(name, prev->f->upvalues[v->u.info].name));
  }
  up->name = name;
  luaC_objbarrier(fs->ls->L, fs->f, name);
  return fs->nups - 1;
}

static int searchvar (FuncState *fs, TString *n, expdesc *var) {
  int i;
  for (i = cast_int(fs->nactvar) - 1; i >= 0; i--) {
    Vardesc *vd = getlocalvardesc(fs, i);
    if (eqstr(n, vd->vd.name)) {
      if (vd->vd.kind == RDKCTC)
        init_exp(var, VCONST, fs->firstlocal + i);
      else
        init_var(fs, var, i);
      return var->k;
    }
  }
  return -1;
}

static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl->nactvar > level)
    bl = bl->previous;
  bl->upval = 1;
  fs->needclose = 1;
}

static void marktobeclosed (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  bl->upval = 1;
  bl->insidetbc = 1;
  fs->needclose = 1;
}

static void singlevaraux (FuncState *fs, TString *n, expdesc *var, int base) {
  if (fs == NULL)
    init_exp(var, VVOID, 0);
  else {
    int v = searchvar(fs, n, var);
    if (v >= 0) {
      if (v == VLOCAL && !base)
        markupval(fs, var->u.var.vidx);
    }
    else {
      int idx = searchupvalue(fs, n);
      if (idx < 0) {
        singlevaraux(fs->prev, n, var, 0);
        if (var->k == VLOCAL || var->k == VUPVAL)
          idx  = newupvalue(fs, n, var);
        else
          return;
      }
      init_exp(var, VUPVAL, idx);
    }
  }
}

static void singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  singlevaraux(fs, varname, var, 1);
  if (var->k == VVOID) {
    expdesc key;
    singlevaraux(fs, ls->envn, var, 1);
    lua_assert(var->k != VVOID);
    luaK_exp2anyregup(fs, var);
    codestring(&key, varname);
    luaK_indexed(fs, var, &key);
  }
}

static void adjust_assign (LexState *ls, int nvars, int nexps, expdesc *e) {
  FuncState *fs = ls->fs;
  int needed = nvars - nexps;
  if (hasmultret(e->k)) {
    int extra = needed + 1;
    if (extra < 0)
      extra = 0;
    luaK_setreturns(fs, e, extra);
  }
  else {
    if (e->k != VVOID)
      luaK_exp2nextreg(fs, e);
    if (needed > 0)
      luaK_nil(fs, fs->freereg, needed);
  }
  if (needed > 0)
    luaK_reserveregs(fs, needed);
  else
    fs->freereg += needed;
}

#define enterlevel(ls)	luaE_incCstack(ls->L)

#define leavelevel(ls) ((ls)->L->nCcalls--)

static l_noret jumpscopeerror (LexState *ls, Labeldesc *gt) {
  const char *varname = getstr(getlocalvardesc(ls->fs, gt->nactvar)->vd.name);
  const char *msg = "<goto %s> at line %d jumps into the scope of local '%s'";
  msg = luaO_pushfstring(ls->L, msg, getstr(gt->name), gt->line, varname);
  luaK_semerror(ls, msg);
}

static void solvegoto (LexState *ls, int g, Labeldesc *label) {
  int i;
  Labellist *gl = &ls->dyd->gt;
  Labeldesc *gt = &gl->arr[g];
  lua_assert(eqstr(gt->name, label->name));
  if (l_unlikely(gt->nactvar < label->nactvar))
    jumpscopeerror(ls, gt);
  luaK_patchlist(ls->fs, gt->pc, label->pc);
  for (i = g; i < gl->n - 1; i++)
    gl->arr[i] = gl->arr[i + 1];
  gl->n--;
}

static Labeldesc *findlabel (LexState *ls, TString *name) {
  int i;
  Dyndata *dyd = ls->dyd;

  for (i = ls->fs->firstlabel; i < dyd->label.n; i++) {
    Labeldesc *lb = &dyd->label.arr[i];
    if (eqstr(lb->name, name))
      return lb;
  }
  return NULL;
}

static int newlabelentry (LexState *ls, Labellist *l, TString *name,
                          int line, int pc) {
  int n = l->n;
  luaM_growvector(ls->L, l->arr, n, l->size,
                  Labeldesc, SHRT_MAX, "labels/gotos");
  l->arr[n].name = name;
  l->arr[n].line = line;
  l->arr[n].nactvar = ls->fs->nactvar;
  l->arr[n].close = 0;
  l->arr[n].pc = pc;
  l->n = n + 1;
  return n;
}

static int newgotoentry (LexState *ls, TString *name, int line, int pc) {
  return newlabelentry(ls, &ls->dyd->gt, name, line, pc);
}

static int solvegotos (LexState *ls, Labeldesc *lb) {
  Labellist *gl = &ls->dyd->gt;
  int i = ls->fs->bl->firstgoto;
  int needsclose = 0;
  while (i < gl->n) {
    if (eqstr(gl->arr[i].name, lb->name)) {
      needsclose |= gl->arr[i].close;
      solvegoto(ls, i, lb);
    }
    else
      i++;
  }
  return needsclose;
}

static int createlabel (LexState *ls, TString *name, int line,
                        int last) {
  FuncState *fs = ls->fs;
  Labellist *ll = &ls->dyd->label;
  int l = newlabelentry(ls, ll, name, line, luaK_getlabel(fs));
  if (last) {

    ll->arr[l].nactvar = fs->bl->nactvar;
  }
  if (solvegotos(ls, &ll->arr[l])) {
    luaK_codeABC(fs, OP_CLOSE, luaY_nvarstack(fs), 0, 0);
    return 1;
  }
  return 0;
}

static void movegotosout (FuncState *fs, BlockCnt *bl) {
  int i;
  Labellist *gl = &fs->ls->dyd->gt;

  for (i = bl->firstgoto; i < gl->n; i++) {
    Labeldesc *gt = &gl->arr[i];

    if (reglevel(fs, gt->nactvar) > reglevel(fs, bl->nactvar))
      gt->close |= bl->upval;
    gt->nactvar = bl->nactvar;
  }
}

static void enterblock (FuncState *fs, BlockCnt *bl, lu_byte isloop) {
  bl->isloop = isloop;
  bl->nactvar = fs->nactvar;
  bl->firstlabel = fs->ls->dyd->label.n;
  bl->firstgoto = fs->ls->dyd->gt.n;
  bl->upval = 0;
  bl->insidetbc = (fs->bl != NULL && fs->bl->insidetbc);
  bl->previous = fs->bl;
  fs->bl = bl;
  lua_assert(fs->freereg == luaY_nvarstack(fs));
}

static l_noret undefgoto (LexState *ls, Labeldesc *gt) {
  const char *msg;
  if (eqstr(gt->name, luaS_newliteral(ls->L, "break"))) {
    msg = "break outside loop at line %d";
    msg = luaO_pushfstring(ls->L, msg, gt->line);
  }
  else {
    msg = "no visible label '%s' for <goto> at line %d";
    msg = luaO_pushfstring(ls->L, msg, getstr(gt->name), gt->line);
  }
  luaK_semerror(ls, msg);
}

static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  LexState *ls = fs->ls;
  int hasclose = 0;
  int stklevel = reglevel(fs, bl->nactvar);
  removevars(fs, bl->nactvar);
  lua_assert(bl->nactvar == fs->nactvar);
  if (bl->isloop)
    hasclose = createlabel(ls, luaS_newliteral(ls->L, "break"), 0, 0);
  if (!hasclose && bl->previous && bl->upval)
    luaK_codeABC(fs, OP_CLOSE, stklevel, 0, 0);
  fs->freereg = stklevel;
  ls->dyd->label.n = bl->firstlabel;
  fs->bl = bl->previous;
  if (bl->previous)
    movegotosout(fs, bl);
  else {
    if (bl->firstgoto < ls->dyd->gt.n)
      undefgoto(ls, &ls->dyd->gt.arr[bl->firstgoto]);
  }
}

static Proto *addprototype (LexState *ls) {
  Proto *clp;
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  if (fs->np >= f->sizep) {
    int oldsize = f->sizep;
    luaM_growvector(L, f->p, fs->np, f->sizep, Proto *, MAXARG_Bx, "functions");
    while (oldsize < f->sizep)
      f->p[oldsize++] = NULL;
  }
  f->p[fs->np++] = clp = luaF_newproto(L);
  luaC_objbarrier(L, f, clp);
  return clp;
}

static void codeclosure (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs->prev;
  init_exp(v, VRELOC, luaK_codeABx(fs, OP_CLOSURE, 0, fs->np - 1));
  luaK_exp2nextreg(fs, v);
}

static void open_func (LexState *ls, FuncState *fs, BlockCnt *bl) {
  Proto *f = fs->f;
  fs->prev = ls->fs;
  fs->ls = ls;
  ls->fs = fs;
  fs->pc = 0;
  fs->previousline = f->linedefined;
  fs->iwthabs = 0;
  fs->lasttarget = 0;
  fs->freereg = 0;
  fs->nk = 0;
  fs->nabslineinfo = 0;
  fs->np = 0;
  fs->nups = 0;
  fs->ndebugvars = 0;
  fs->nactvar = 0;
  fs->needclose = 0;
  fs->firstlocal = ls->dyd->actvar.n;
  fs->firstlabel = ls->dyd->label.n;
  fs->bl = NULL;
  f->source = ls->source;
  luaC_objbarrier(ls->L, f, f->source);
  f->maxstacksize = 2;
  enterblock(fs, bl, 0);
}

static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  luaK_ret(fs, luaY_nvarstack(fs), 0);
  leaveblock(fs);
  lua_assert(fs->bl == NULL);
  luaK_finish(fs);
  luaM_shrinkvector(L, f->code, f->sizecode, fs->pc, Instruction);
  luaM_shrinkvector(L, f->lineinfo, f->sizelineinfo, fs->pc, ls_byte);
  luaM_shrinkvector(L, f->abslineinfo, f->sizeabslineinfo,
                       fs->nabslineinfo, AbsLineInfo);
  luaM_shrinkvector(L, f->k, f->sizek, fs->nk, TValue);
  luaM_shrinkvector(L, f->p, f->sizep, fs->np, Proto *);
  luaM_shrinkvector(L, f->locvars, f->sizelocvars, fs->ndebugvars, LocVar);
  luaM_shrinkvector(L, f->upvalues, f->sizeupvalues, fs->nups, Upvaldesc);
  ls->fs = fs->prev;
  luaC_checkGC(L);
}

static int block_follow (LexState *ls, int withuntil) {
  switch (ls->t.token) {
    case TK_ELSE: case TK_ELSEIF:
    case TK_END: case TK_EOS:
      return 1;
    case TK_UNTIL: return withuntil;
    default: return 0;
  }
}

static void statlist (LexState *ls) {

  while (!block_follow(ls, 1)) {
    if (ls->t.token == TK_RETURN) {
      statement(ls);
      return;
    }
    statement(ls);
  }
}

static void fieldsel (LexState *ls, expdesc *v) {

  FuncState *fs = ls->fs;
  expdesc key;
  luaK_exp2anyregup(fs, v);
  luaX_next(ls);
  codename(ls, &key);
  luaK_indexed(fs, v, &key);
}

static void yindex (LexState *ls, expdesc *v) {

  luaX_next(ls);
  expr(ls, v);
  luaK_exp2val(ls->fs, v);
  checknext(ls, ']');
}

typedef struct ConsControl {
  expdesc v;
  expdesc *t;
  int nh;
  int na;
  int tostore;
} ConsControl;

static void recfield (LexState *ls, ConsControl *cc) {

  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc tab, key, val;
  if (ls->t.token == TK_NAME)
    codename(ls, &key);
  else
    yindex(ls, &key);
  checklimit(fs, cc->nh, MAX_INT, "items in a constructor");
  cc->nh++;
  checknext(ls, '=');
  tab = *cc->t;
  luaK_indexed(fs, &tab, &key);
  expr(ls, &val);
  luaK_storevar(fs, &tab, &val);
  fs->freereg = reg;
}

static void closelistfield (FuncState *fs, ConsControl *cc) {
  if (cc->v.k == VVOID) return;
  luaK_exp2nextreg(fs, &cc->v);
  cc->v.k = VVOID;
  if (cc->tostore == LFIELDS_PER_FLUSH) {
    luaK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);
    cc->na += cc->tostore;
    cc->tostore = 0;
  }
}

static void lastlistfield (FuncState *fs, ConsControl *cc) {
  if (cc->tostore == 0) return;
  if (hasmultret(cc->v.k)) {
    luaK_setmultret(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.info, cc->na, LUA_MULTRET);
    cc->na--;
  }
  else {
    if (cc->v.k != VVOID)
      luaK_exp2nextreg(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);
  }
  cc->na += cc->tostore;
}

static void listfield (LexState *ls, ConsControl *cc) {

  expr(ls, &cc->v);
  cc->tostore++;
}

static void field (LexState *ls, ConsControl *cc) {

  switch(ls->t.token) {
    case TK_NAME: {
      if (luaX_lookahead(ls) != '=')
        listfield(ls, cc);
      else
        recfield(ls, cc);
      break;
    }
    case '[': {
      recfield(ls, cc);
      break;
    }
    default: {
      listfield(ls, cc);
      break;
    }
  }
}

static void constructor (LexState *ls, expdesc *t) {

  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  ConsControl cc;
  luaK_code(fs, 0);
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VNONRELOC, fs->freereg);
  luaK_reserveregs(fs, 1);
  init_exp(&cc.v, VVOID, 0);
  checknext(ls, '{');
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == '}') break;
    closelistfield(fs, &cc);
    field(ls, &cc);
  } while (testnext(ls, ',') || testnext(ls, ';'));
  check_match(ls, '}', '{', line);
  lastlistfield(fs, &cc);
  luaK_settablesize(fs, pc, t->u.info, cc.na, cc.nh);
}

static void setvararg (FuncState *fs, int nparams) {
  fs->f->is_vararg = 1;
  luaK_codeABC(fs, OP_VARARGPREP, nparams, 0, 0);
}

static void parlist (LexState *ls) {

  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int nparams = 0;
  int isvararg = 0;
  if (ls->t.token != ')') {
    do {
      switch (ls->t.token) {
        case TK_NAME: {
          new_localvar(ls, str_checkname(ls));
          nparams++;
          break;
        }
        case TK_DOTS: {
          luaX_next(ls);
          isvararg = 1;
          break;
        }
        default: luaX_syntaxerror(ls, "<name> or '...' expected");
      }
    } while (!isvararg && testnext(ls, ','));
  }
  adjustlocalvars(ls, nparams);
  f->numparams = cast_byte(fs->nactvar);
  if (isvararg)
    setvararg(fs, f->numparams);
  luaK_reserveregs(fs, fs->nactvar);
}

static void body (LexState *ls, expdesc *e, int ismethod, int line) {

  FuncState new_fs;
  BlockCnt bl;
  new_fs.f = addprototype(ls);
  new_fs.f->linedefined = line;
  open_func(ls, &new_fs, &bl);
  checknext(ls, '(');
  if (ismethod) {
    new_localvarliteral(ls, "self");
    adjustlocalvars(ls, 1);
  }
  parlist(ls);
  checknext(ls, ')');
  statlist(ls);
  new_fs.f->lastlinedefined = ls->linenumber;
  check_match(ls, TK_END, TK_FUNCTION, line);
  codeclosure(ls, e);
  close_func(ls);
}

static int explist (LexState *ls, expdesc *v) {

  int n = 1;
  expr(ls, v);
  while (testnext(ls, ',')) {
    luaK_exp2nextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}

static void funcargs (LexState *ls, expdesc *f) {
  FuncState *fs = ls->fs;
  expdesc args;
  int base, nparams;
  int line = ls->linenumber;
  switch (ls->t.token) {
    case '(': {
      luaX_next(ls);
      if (ls->t.token == ')')
        args.k = VVOID;
      else {
        explist(ls, &args);
        if (hasmultret(args.k))
          luaK_setmultret(fs, &args);
      }
      check_match(ls, ')', '(', line);
      break;
    }
    case '{': {
      constructor(ls, &args);
      break;
    }
    case TK_STRING: {
      codestring(&args, ls->t.seminfo.ts);
      luaX_next(ls);
      break;
    }
    default: {
      luaX_syntaxerror(ls, "function arguments expected");
    }
  }
  lua_assert(f->k == VNONRELOC);
  base = f->u.info;
  if (hasmultret(args.k))
    nparams = LUA_MULTRET;
  else {
    if (args.k != VVOID)
      luaK_exp2nextreg(fs, &args);
    nparams = fs->freereg - (base+1);
  }
  init_exp(f, VCALL, luaK_codeABC(fs, OP_CALL, base, nparams+1, 2));
  luaK_fixline(fs, line);
  fs->freereg = base+1;

}

static void primaryexp (LexState *ls, expdesc *v) {

  switch (ls->t.token) {
    case '(': {
      int line = ls->linenumber;
      luaX_next(ls);
      expr(ls, v);
      check_match(ls, ')', '(', line);
      luaK_dischargevars(ls->fs, v);
      return;
    }
    case TK_NAME: {
      singlevar(ls, v);
      return;
    }
    default: {
      luaX_syntaxerror(ls, "unexpected symbol");
    }
  }
}

static void suffixedexp (LexState *ls, expdesc *v) {

  FuncState *fs = ls->fs;
  primaryexp(ls, v);
  for (;;) {
    switch (ls->t.token) {
      case '.': {
        fieldsel(ls, v);
        break;
      }
      case '[': {
        expdesc key;
        luaK_exp2anyregup(fs, v);
        yindex(ls, &key);
        luaK_indexed(fs, v, &key);
        break;
      }
      case ':': {
        expdesc key;
        luaX_next(ls);
        codename(ls, &key);
        luaK_self(fs, v, &key);
        funcargs(ls, v);
        break;
      }
      case '(': case TK_STRING: case '{': {
        luaK_exp2nextreg(fs, v);
        funcargs(ls, v);
        break;
      }
      default: return;
    }
  }
}

static void simpleexp (LexState *ls, expdesc *v) {

  switch (ls->t.token) {
    case TK_FLT: {
      init_exp(v, VKFLT, 0);
      v->u.nval = ls->t.seminfo.r;
      break;
    }
    case TK_INT: {
      init_exp(v, VKINT, 0);
      v->u.ival = ls->t.seminfo.i;
      break;
    }
    case TK_STRING: {
      codestring(v, ls->t.seminfo.ts);
      break;
    }
    case TK_NIL: {
      init_exp(v, VNIL, 0);
      break;
    }
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
      break;
    }
    case TK_DOTS: {
      FuncState *fs = ls->fs;
      check_condition(ls, fs->f->is_vararg,
                      "cannot use '...' outside a vararg function");
      init_exp(v, VVARARG, luaK_codeABC(fs, OP_VARARG, 0, 0, 1));
      break;
    }
    case '{': {
      constructor(ls, v);
      return;
    }
    case TK_FUNCTION: {
      luaX_next(ls);
      body(ls, v, 0, ls->linenumber);
      return;
    }
    default: {
      suffixedexp(ls, v);
      return;
    }
  }
  luaX_next(ls);
}

static UnOpr getunopr (int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;
    case '-': return OPR_MINUS;
    case '~': return OPR_BNOT;
    case '#': return OPR_LEN;
    default: return OPR_NOUNOPR;
  }
}

static BinOpr getbinopr (int op) {
  switch (op) {
    case '+': return OPR_ADD;
    case '-': return OPR_SUB;
    case '*': return OPR_MUL;
    case '%': return OPR_MOD;
    case '^': return OPR_POW;
    case '/': return OPR_DIV;
    case TK_IDIV: return OPR_IDIV;
    case '&': return OPR_BAND;
    case '|': return OPR_BOR;
    case '~': return OPR_BXOR;
    case TK_SHL: return OPR_SHL;
    case TK_SHR: return OPR_SHR;
    case TK_CONCAT: return OPR_CONCAT;
    case TK_NE: return OPR_NE;
    case TK_EQ: return OPR_EQ;
    case '<': return OPR_LT;
    case TK_LE: return OPR_LE;
    case '>': return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_AND: return OPR_AND;
    case TK_OR: return OPR_OR;
    default: return OPR_NOBINOPR;
  }
}

static const struct {
  lu_byte left;
  lu_byte right;
} priority[] = {
   {10, 10}, {10, 10},
   {11, 11}, {11, 11},
   {14, 13},
   {11, 11}, {11, 11},
   {6, 6}, {4, 4}, {5, 5},
   {7, 7}, {7, 7},
   {9, 8},
   {3, 3}, {3, 3}, {3, 3},
   {3, 3}, {3, 3}, {3, 3},
   {2, 2}, {1, 1}
};

#define UNARY_PRIORITY	12

static BinOpr subexpr (LexState *ls, expdesc *v, int limit) {
  BinOpr op;
  UnOpr uop;
  enterlevel(ls);
  uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {
    int line = ls->linenumber;
    luaX_next(ls);
    subexpr(ls, v, UNARY_PRIORITY);
    luaK_prefix(ls->fs, uop, v, line);
  }
  else simpleexp(ls, v);

  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    int line = ls->linenumber;
    luaX_next(ls);
    luaK_infix(ls->fs, op, v);

    nextop = subexpr(ls, &v2, priority[op].right);
    luaK_posfix(ls->fs, op, v, &v2, line);
    op = nextop;
  }
  leavelevel(ls);
  return op;
}

static void expr (LexState *ls, expdesc *v) {
  subexpr(ls, v, 0);
}

static void block (LexState *ls) {

  FuncState *fs = ls->fs;
  BlockCnt bl;
  enterblock(fs, &bl, 0);
  statlist(ls);
  leaveblock(fs);
}

struct LHS_assign {
  struct LHS_assign *prev;
  expdesc v;
};

static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v) {
  FuncState *fs = ls->fs;
  int extra = fs->freereg;
  int conflict = 0;
  for (; lh; lh = lh->prev) {
    if (vkisindexed(lh->v.k)) {
      if (lh->v.k == VINDEXUP) {
        if (v->k == VUPVAL && lh->v.u.ind.t == v->u.info) {
          conflict = 1;
          lh->v.k = VINDEXSTR;
          lh->v.u.ind.t = extra;
        }
      }
      else {
        if (v->k == VLOCAL && lh->v.u.ind.t == v->u.var.ridx) {
          conflict = 1;
          lh->v.u.ind.t = extra;
        }

        if (lh->v.k == VINDEXED && v->k == VLOCAL &&
            lh->v.u.ind.idx == v->u.var.ridx) {
          conflict = 1;
          lh->v.u.ind.idx = extra;
        }
      }
    }
  }
  if (conflict) {

    if (v->k == VLOCAL)
      luaK_codeABC(fs, OP_MOVE, extra, v->u.var.ridx, 0);
    else
      luaK_codeABC(fs, OP_GETUPVAL, extra, v->u.info, 0);
    luaK_reserveregs(fs, 1);
  }
}

static void restassign (LexState *ls, struct LHS_assign *lh, int nvars) {
  expdesc e;
  check_condition(ls, vkisvar(lh->v.k), "syntax error");
  check_readonly(ls, &lh->v);
  if (testnext(ls, ',')) {
    struct LHS_assign nv;
    nv.prev = lh;
    suffixedexp(ls, &nv.v);
    if (!vkisindexed(nv.v.k))
      check_conflict(ls, lh, &nv.v);
    enterlevel(ls);
    restassign(ls, &nv, nvars+1);
    leavelevel(ls);
  }
  else {
    int nexps;
    checknext(ls, '=');
    nexps = explist(ls, &e);
    if (nexps != nvars)
      adjust_assign(ls, nvars, nexps, &e);
    else {
      luaK_setoneret(ls->fs, &e);
      luaK_storevar(ls->fs, &lh->v, &e);
      return;
    }
  }
  init_exp(&e, VNONRELOC, ls->fs->freereg-1);
  luaK_storevar(ls->fs, &lh->v, &e);
}

static int cond (LexState *ls) {

  expdesc v;
  expr(ls, &v);
  if (v.k == VNIL) v.k = VFALSE;
  luaK_goiftrue(ls->fs, &v);
  return v.f;
}

static void gotostat (LexState *ls) {
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  TString *name = str_checkname(ls);
  Labeldesc *lb = findlabel(ls, name);
  if (lb == NULL)

    newgotoentry(ls, name, line, luaK_jump(fs));
  else {

    int lblevel = reglevel(fs, lb->nactvar);
    if (luaY_nvarstack(fs) > lblevel)
      luaK_codeABC(fs, OP_CLOSE, lblevel, 0, 0);

    luaK_patchlist(fs, luaK_jump(fs), lb->pc);
  }
}

static void breakstat (LexState *ls) {
  int line = ls->linenumber;
  luaX_next(ls);
  newgotoentry(ls, luaS_newliteral(ls->L, "break"), line, luaK_jump(ls->fs));
}

static void checkrepeated (LexState *ls, TString *name) {
  Labeldesc *lb = findlabel(ls, name);
  if (l_unlikely(lb != NULL)) {
    const char *msg = "label '%s' already defined on line %d";
    msg = luaO_pushfstring(ls->L, msg, getstr(name), lb->line);
    luaK_semerror(ls, msg);
  }
}

static void labelstat (LexState *ls, TString *name, int line) {

  checknext(ls, TK_DBCOLON);
  while (ls->t.token == ';' || ls->t.token == TK_DBCOLON)
    statement(ls);
  checkrepeated(ls, name);
  createlabel(ls, name, line, block_follow(ls, 0));
}

static void whilestat (LexState *ls, int line) {

  FuncState *fs = ls->fs;
  int whileinit;
  int condexit;
  BlockCnt bl;
  luaX_next(ls);
  whileinit = luaK_getlabel(fs);
  condexit = cond(ls);
  enterblock(fs, &bl, 1);
  checknext(ls, TK_DO);
  block(ls);
  luaK_jumpto(fs, whileinit);
  check_match(ls, TK_END, TK_WHILE, line);
  leaveblock(fs);
  luaK_patchtohere(fs, condexit);
}

static void repeatstat (LexState *ls, int line) {

  int condexit;
  FuncState *fs = ls->fs;
  int repeat_init = luaK_getlabel(fs);
  BlockCnt bl1, bl2;
  enterblock(fs, &bl1, 1);
  enterblock(fs, &bl2, 0);
  luaX_next(ls);
  statlist(ls);
  check_match(ls, TK_UNTIL, TK_REPEAT, line);
  condexit = cond(ls);
  leaveblock(fs);
  if (bl2.upval) {
    int exit = luaK_jump(fs);
    luaK_patchtohere(fs, condexit);
    luaK_codeABC(fs, OP_CLOSE, reglevel(fs, bl2.nactvar), 0, 0);
    condexit = luaK_jump(fs);
    luaK_patchtohere(fs, exit);
  }
  luaK_patchlist(fs, condexit, repeat_init);
  leaveblock(fs);
}

static void exp1 (LexState *ls) {
  expdesc e;
  expr(ls, &e);
  luaK_exp2nextreg(ls->fs, &e);
  lua_assert(e.k == VNONRELOC);
}

static void fixforjump (FuncState *fs, int pc, int dest, int back) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  if (back)
    offset = -offset;
  if (l_unlikely(offset > MAXARG_Bx))
    luaX_syntaxerror(fs->ls, "control structure too long");
  SETARG_Bx(*jmp, offset);
}

static void forbody (LexState *ls, int base, int line, int nvars, int isgen) {

  static const OpCode forprep[2] = {OP_FORPREP, OP_TFORPREP};
  static const OpCode forloop[2] = {OP_FORLOOP, OP_TFORLOOP};
  BlockCnt bl;
  FuncState *fs = ls->fs;
  int prep, endfor;
  checknext(ls, TK_DO);
  prep = luaK_codeABx(fs, forprep[isgen], base, 0);
  enterblock(fs, &bl, 0);
  adjustlocalvars(ls, nvars);
  luaK_reserveregs(fs, nvars);
  block(ls);
  leaveblock(fs);
  fixforjump(fs, prep, luaK_getlabel(fs), 0);
  if (isgen) {
    luaK_codeABC(fs, OP_TFORCALL, base, 0, nvars);
    luaK_fixline(fs, line);
  }
  endfor = luaK_codeABx(fs, forloop[isgen], base, 0);
  fixforjump(fs, endfor, prep + 1, 1);
  luaK_fixline(fs, line);
}

static void fornum (LexState *ls, TString *varname, int line) {

  FuncState *fs = ls->fs;
  int base = fs->freereg;
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvar(ls, varname);
  checknext(ls, '=');
  exp1(ls);
  checknext(ls, ',');
  exp1(ls);
  if (testnext(ls, ','))
    exp1(ls);
  else {
    luaK_int(fs, fs->freereg, 1);
    luaK_reserveregs(fs, 1);
  }
  adjustlocalvars(ls, 3);
  forbody(ls, base, line, 1, 0);
}

static void forlist (LexState *ls, TString *indexname) {

  FuncState *fs = ls->fs;
  expdesc e;
  int nvars = 5;
  int line;
  int base = fs->freereg;

  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");

  new_localvar(ls, indexname);
  while (testnext(ls, ',')) {
    new_localvar(ls, str_checkname(ls));
    nvars++;
  }
  checknext(ls, TK_IN);
  line = ls->linenumber;
  adjust_assign(ls, 4, explist(ls, &e), &e);
  adjustlocalvars(ls, 4);
  marktobeclosed(fs);
  luaK_checkstack(fs, 3);
  forbody(ls, base, line, nvars - 4, 1);
}

static void forstat (LexState *ls, int line) {

  FuncState *fs = ls->fs;
  TString *varname;
  BlockCnt bl;
  enterblock(fs, &bl, 1);
  luaX_next(ls);
  varname = str_checkname(ls);
  switch (ls->t.token) {
    case '=': fornum(ls, varname, line); break;
    case ',': case TK_IN: forlist(ls, varname); break;
    default: luaX_syntaxerror(ls, "'=' or 'in' expected");
  }
  check_match(ls, TK_END, TK_FOR, line);
  leaveblock(fs);
}

static void test_then_block (LexState *ls, int *escapelist) {

  BlockCnt bl;
  FuncState *fs = ls->fs;
  expdesc v;
  int jf;
  luaX_next(ls);
  expr(ls, &v);
  checknext(ls, TK_THEN);
  if (ls->t.token == TK_BREAK) {
    int line = ls->linenumber;
    luaK_goiffalse(ls->fs, &v);
    luaX_next(ls);
    enterblock(fs, &bl, 0);
    newgotoentry(ls, luaS_newliteral(ls->L, "break"), line, v.t);
    while (testnext(ls, ';')) {}
    if (block_follow(ls, 0)) {
      leaveblock(fs);
      return;
    }
    else
      jf = luaK_jump(fs);
  }
  else {
    luaK_goiftrue(ls->fs, &v);
    enterblock(fs, &bl, 0);
    jf = v.f;
  }
  statlist(ls);
  leaveblock(fs);
  if (ls->t.token == TK_ELSE ||
      ls->t.token == TK_ELSEIF)
    luaK_concat(fs, escapelist, luaK_jump(fs));
  luaK_patchtohere(fs, jf);
}

static void ifstat (LexState *ls, int line) {

  FuncState *fs = ls->fs;
  int escapelist = NO_JUMP;
  test_then_block(ls, &escapelist);
  while (ls->t.token == TK_ELSEIF)
    test_then_block(ls, &escapelist);
  if (testnext(ls, TK_ELSE))
    block(ls);
  check_match(ls, TK_END, TK_IF, line);
  luaK_patchtohere(fs, escapelist);
}

static void localfunc (LexState *ls) {
  expdesc b;
  FuncState *fs = ls->fs;
  int fvar = fs->nactvar;
  new_localvar(ls, str_checkname(ls));
  adjustlocalvars(ls, 1);
  body(ls, &b, 0, ls->linenumber);

  localdebuginfo(fs, fvar)->startpc = fs->pc;
}

static int getlocalattribute (LexState *ls) {

  if (testnext(ls, '<')) {
    const char *attr = getstr(str_checkname(ls));
    checknext(ls, '>');
    if (strcmp(attr, "const") == 0)
      return RDKCONST;
    else if (strcmp(attr, "close") == 0)
      return RDKTOCLOSE;
    else
      luaK_semerror(ls,
        luaO_pushfstring(ls->L, "unknown attribute '%s'", attr));
  }
  return VDKREG;
}

static void checktoclose (FuncState *fs, int level) {
  if (level != -1) {
    marktobeclosed(fs);
    luaK_codeABC(fs, OP_TBC, reglevel(fs, level), 0, 0);
  }
}

static void localstat (LexState *ls) {

  FuncState *fs = ls->fs;
  int toclose = -1;
  Vardesc *var;
  int vidx, kind;
  int nvars = 0;
  int nexps;
  expdesc e;
  do {
    vidx = new_localvar(ls, str_checkname(ls));
    kind = getlocalattribute(ls);
    getlocalvardesc(fs, vidx)->vd.kind = kind;
    if (kind == RDKTOCLOSE) {
      if (toclose != -1)
        luaK_semerror(ls, "multiple to-be-closed variables in local list");
      toclose = fs->nactvar + nvars;
    }
    nvars++;
  } while (testnext(ls, ','));
  if (testnext(ls, '='))
    nexps = explist(ls, &e);
  else {
    e.k = VVOID;
    nexps = 0;
  }
  var = getlocalvardesc(fs, vidx);
  if (nvars == nexps &&
      var->vd.kind == RDKCONST &&
      luaK_exp2const(fs, &e, &var->k)) {
    var->vd.kind = RDKCTC;
    adjustlocalvars(ls, nvars - 1);
    fs->nactvar++;
  }
  else {
    adjust_assign(ls, nvars, nexps, &e);
    adjustlocalvars(ls, nvars);
  }
  checktoclose(fs, toclose);
}

static int funcname (LexState *ls, expdesc *v) {

  int ismethod = 0;
  singlevar(ls, v);
  while (ls->t.token == '.')
    fieldsel(ls, v);
  if (ls->t.token == ':') {
    ismethod = 1;
    fieldsel(ls, v);
  }
  return ismethod;
}

static void funcstat (LexState *ls, int line) {

  int ismethod;
  expdesc v, b;
  luaX_next(ls);
  ismethod = funcname(ls, &v);
  body(ls, &b, ismethod, line);
  check_readonly(ls, &v);
  luaK_storevar(ls->fs, &v, &b);
  luaK_fixline(ls->fs, line);
}

static void exprstat (LexState *ls) {

  FuncState *fs = ls->fs;
  struct LHS_assign v;
  suffixedexp(ls, &v.v);
  if (ls->t.token == '=' || ls->t.token == ',') {
    v.prev = NULL;
    restassign(ls, &v, 1);
  }
  else {
    Instruction *inst;
    check_condition(ls, v.v.k == VCALL, "syntax error");
    inst = &getinstruction(fs, &v.v);
    SETARG_C(*inst, 1);
  }
}

static void retstat (LexState *ls) {

  FuncState *fs = ls->fs;
  expdesc e;
  int nret;
  int first = luaY_nvarstack(fs);
  if (block_follow(ls, 1) || ls->t.token == ';')
    nret = 0;
  else {
    nret = explist(ls, &e);
    if (hasmultret(e.k)) {
      luaK_setmultret(fs, &e);
      if (e.k == VCALL && nret == 1 && !fs->bl->insidetbc) {
        SET_OPCODE(getinstruction(fs,&e), OP_TAILCALL);
        lua_assert(GETARG_A(getinstruction(fs,&e)) == luaY_nvarstack(fs));
      }
      nret = LUA_MULTRET;
    }
    else {
      if (nret == 1)
        first = luaK_exp2anyreg(fs, &e);
      else {
        luaK_exp2nextreg(fs, &e);
        lua_assert(nret == fs->freereg - first);
      }
    }
  }
  luaK_ret(fs, first, nret);
  testnext(ls, ';');
}

static void statement (LexState *ls) {
  int line = ls->linenumber;
  enterlevel(ls);
  switch (ls->t.token) {
    case ';': {
      luaX_next(ls);
      break;
    }
    case TK_IF: {
      ifstat(ls, line);
      break;
    }
    case TK_WHILE: {
      whilestat(ls, line);
      break;
    }
    case TK_DO: {
      luaX_next(ls);
      block(ls);
      check_match(ls, TK_END, TK_DO, line);
      break;
    }
    case TK_FOR: {
      forstat(ls, line);
      break;
    }
    case TK_REPEAT: {
      repeatstat(ls, line);
      break;
    }
    case TK_FUNCTION: {
      funcstat(ls, line);
      break;
    }
    case TK_LOCAL: {
      luaX_next(ls);
      if (testnext(ls, TK_FUNCTION))
        localfunc(ls);
      else
        localstat(ls);
      break;
    }
    case TK_DBCOLON: {
      luaX_next(ls);
      labelstat(ls, str_checkname(ls), line);
      break;
    }
    case TK_RETURN: {
      luaX_next(ls);
      retstat(ls);
      break;
    }
    case TK_BREAK: {
      breakstat(ls);
      break;
    }
    case TK_GOTO: {
      luaX_next(ls);
      gotostat(ls);
      break;
    }
    default: {
      exprstat(ls);
      break;
    }
  }
  lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
             ls->fs->freereg >= luaY_nvarstack(ls->fs));
  ls->fs->freereg = luaY_nvarstack(ls->fs);
  leavelevel(ls);
}

static void mainfunc (LexState *ls, FuncState *fs) {
  BlockCnt bl;
  Upvaldesc *env;
  open_func(ls, fs, &bl);
  setvararg(fs, 0);
  env = allocupvalue(fs);
  env->instack = 1;
  env->idx = 0;
  env->kind = VDKREG;
  env->name = ls->envn;
  luaC_objbarrier(ls->L, fs->f, env->name);
  luaX_next(ls);
  statlist(ls);
  check(ls, TK_EOS);
  close_func(ls);
}

LClosure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                       Dyndata *dyd, const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  LClosure *cl = luaF_newLclosure(L, 1);
  setclLvalue2s(L, L->top.p, cl);
  luaD_inctop(L);
  lexstate.h = luaH_new(L);
  sethvalue2s(L, L->top.p, lexstate.h);
  luaD_inctop(L);
  funcstate.f = cl->p = luaF_newproto(L);
  luaC_objbarrier(L, cl, cl->p);
  funcstate.f->source = luaS_new(L, name);
  luaC_objbarrier(L, funcstate.f, funcstate.f->source);
  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  luaX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  mainfunc(&lexstate, &funcstate);
  lua_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);

  lua_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
  L->top.p--;
  return cl;
}
