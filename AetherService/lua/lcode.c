
#define lcode_c
#define LUA_CORE

#include "lprefix.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "lua.h"

#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"

#define MAXREGS		255

#define hasjumps(e)	((e)->t != (e)->f)

static int codesJ (FuncState *fs, OpCode o, int sj, int k);

l_noret luaK_semerror (LexState *ls, const char *msg) {
  ls->t.token = 0;
  luaX_syntaxerror(ls, msg);
}

static int tonumeral (const expdesc *e, TValue *v) {
  if (hasjumps(e))
    return 0;
  switch (e->k) {
    case VKINT:
      if (v) setivalue(v, e->u.ival);
      return 1;
    case VKFLT:
      if (v) setfltvalue(v, e->u.nval);
      return 1;
    default: return 0;
  }
}

static TValue *const2val (FuncState *fs, const expdesc *e) {
  lua_assert(e->k == VCONST);
  return &fs->ls->dyd->actvar.arr[e->u.info].k;
}

int luaK_exp2const (FuncState *fs, const expdesc *e, TValue *v) {
  if (hasjumps(e))
    return 0;
  switch (e->k) {
    case VFALSE:
      setbfvalue(v);
      return 1;
    case VTRUE:
      setbtvalue(v);
      return 1;
    case VNIL:
      setnilvalue(v);
      return 1;
    case VKSTR: {
      setsvalue(fs->ls->L, v, e->u.strval);
      return 1;
    }
    case VCONST: {
      setobj(fs->ls->L, v, const2val(fs, e));
      return 1;
    }
    default: return tonumeral(e, v);
  }
}

static Instruction *previousinstruction (FuncState *fs) {
  static const Instruction invalidinstruction = ~(Instruction)0;
  if (fs->pc > fs->lasttarget)
    return &fs->f->code[fs->pc - 1];
  else
    return cast(Instruction*, &invalidinstruction);
}

void luaK_nil (FuncState *fs, int from, int n) {
  int l = from + n - 1;
  Instruction *previous = previousinstruction(fs);
  if (GET_OPCODE(*previous) == OP_LOADNIL) {
    int pfrom = GETARG_A(*previous);
    int pl = pfrom + GETARG_B(*previous);
    if ((pfrom <= from && from <= pl + 1) ||
        (from <= pfrom && pfrom <= l + 1)) {
      if (pfrom < from) from = pfrom;
      if (pl > l) l = pl;
      SETARG_A(*previous, from);
      SETARG_B(*previous, l - from);
      return;
    }
  }
  luaK_codeABC(fs, OP_LOADNIL, from, n - 1, 0);
}

static int getjump (FuncState *fs, int pc) {
  int offset = GETARG_sJ(fs->f->code[pc]);
  if (offset == NO_JUMP)
    return NO_JUMP;
  else
    return (pc+1)+offset;
}

static void fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  lua_assert(dest != NO_JUMP);
  if (!(-OFFSET_sJ <= offset && offset <= MAXARG_sJ - OFFSET_sJ))
    luaX_syntaxerror(fs->ls, "control structure too long");
  lua_assert(GET_OPCODE(*jmp) == OP_JMP);
  SETARG_sJ(*jmp, offset);
}

void luaK_concat (FuncState *fs, int *l1, int l2) {
  if (l2 == NO_JUMP) return;
  else if (*l1 == NO_JUMP)
    *l1 = l2;
  else {
    int list = *l1;
    int next;
    while ((next = getjump(fs, list)) != NO_JUMP)
      list = next;
    fixjump(fs, list, l2);
  }
}

int luaK_jump (FuncState *fs) {
  return codesJ(fs, OP_JMP, NO_JUMP, 0);
}

void luaK_ret (FuncState *fs, int first, int nret) {
  OpCode op;
  switch (nret) {
    case 0: op = OP_RETURN0; break;
    case 1: op = OP_RETURN1; break;
    default: op = OP_RETURN; break;
  }
  luaK_codeABC(fs, op, first, nret + 1, 0);
}

static int condjump (FuncState *fs, OpCode op, int A, int B, int C, int k) {
  luaK_codeABCk(fs, op, A, B, C, k);
  return luaK_jump(fs);
}

int luaK_getlabel (FuncState *fs) {
  fs->lasttarget = fs->pc;
  return fs->pc;
}

static Instruction *getjumpcontrol (FuncState *fs, int pc) {
  Instruction *pi = &fs->f->code[pc];
  if (pc >= 1 && testTMode(GET_OPCODE(*(pi-1))))
    return pi-1;
  else
    return pi;
}

static int patchtestreg (FuncState *fs, int node, int reg) {
  Instruction *i = getjumpcontrol(fs, node);
  if (GET_OPCODE(*i) != OP_TESTSET)
    return 0;
  if (reg != NO_REG && reg != GETARG_B(*i))
    SETARG_A(*i, reg);
  else {

    *i = CREATE_ABCk(OP_TEST, GETARG_B(*i), 0, 0, GETARG_k(*i));
  }
  return 1;
}

static void removevalues (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list))
      patchtestreg(fs, list, NO_REG);
}

static void patchlistaux (FuncState *fs, int list, int vtarget, int reg,
                          int dtarget) {
  while (list != NO_JUMP) {
    int next = getjump(fs, list);
    if (patchtestreg(fs, list, reg))
      fixjump(fs, list, vtarget);
    else
      fixjump(fs, list, dtarget);
    list = next;
  }
}

void luaK_patchlist (FuncState *fs, int list, int target) {
  lua_assert(target <= fs->pc);
  patchlistaux(fs, list, target, NO_REG, target);
}

void luaK_patchtohere (FuncState *fs, int list) {
  int hr = luaK_getlabel(fs);
  luaK_patchlist(fs, list, hr);
}

#define LIMLINEDIFF	0x80

static void savelineinfo (FuncState *fs, Proto *f, int line) {
  int linedif = line - fs->previousline;
  int pc = fs->pc - 1;
  if (abs(linedif) >= LIMLINEDIFF || fs->iwthabs++ >= MAXIWTHABS) {
    luaM_growvector(fs->ls->L, f->abslineinfo, fs->nabslineinfo,
                    f->sizeabslineinfo, AbsLineInfo, MAX_INT, "lines");
    f->abslineinfo[fs->nabslineinfo].pc = pc;
    f->abslineinfo[fs->nabslineinfo++].line = line;
    linedif = ABSLINEINFO;
    fs->iwthabs = 1;
  }
  luaM_growvector(fs->ls->L, f->lineinfo, pc, f->sizelineinfo, ls_byte,
                  MAX_INT, "opcodes");
  f->lineinfo[pc] = linedif;
  fs->previousline = line;
}

static void removelastlineinfo (FuncState *fs) {
  Proto *f = fs->f;
  int pc = fs->pc - 1;
  if (f->lineinfo[pc] != ABSLINEINFO) {
    fs->previousline -= f->lineinfo[pc];
    fs->iwthabs--;
  }
  else {
    lua_assert(f->abslineinfo[fs->nabslineinfo - 1].pc == pc);
    fs->nabslineinfo--;
    fs->iwthabs = MAXIWTHABS + 1;
  }
}

static void removelastinstruction (FuncState *fs) {
  removelastlineinfo(fs);
  fs->pc--;
}

int luaK_code (FuncState *fs, Instruction i) {
  Proto *f = fs->f;

  luaM_growvector(fs->ls->L, f->code, fs->pc, f->sizecode, Instruction,
                  MAX_INT, "opcodes");
  f->code[fs->pc++] = i;
  savelineinfo(fs, f, fs->ls->lastline);
  return fs->pc - 1;
}

int luaK_codeABCk (FuncState *fs, OpCode o, int a, int b, int c, int k) {
  lua_assert(getOpMode(o) == iABC);
  lua_assert(a <= MAXARG_A && b <= MAXARG_B &&
             c <= MAXARG_C && (k & ~1) == 0);
  return luaK_code(fs, CREATE_ABCk(o, a, b, c, k));
}

int luaK_codeABx (FuncState *fs, OpCode o, int a, unsigned int bc) {
  lua_assert(getOpMode(o) == iABx);
  lua_assert(a <= MAXARG_A && bc <= MAXARG_Bx);
  return luaK_code(fs, CREATE_ABx(o, a, bc));
}

static int codeAsBx (FuncState *fs, OpCode o, int a, int bc) {
  unsigned int b = bc + OFFSET_sBx;
  lua_assert(getOpMode(o) == iAsBx);
  lua_assert(a <= MAXARG_A && b <= MAXARG_Bx);
  return luaK_code(fs, CREATE_ABx(o, a, b));
}

static int codesJ (FuncState *fs, OpCode o, int sj, int k) {
  unsigned int j = sj + OFFSET_sJ;
  lua_assert(getOpMode(o) == isJ);
  lua_assert(j <= MAXARG_sJ && (k & ~1) == 0);
  return luaK_code(fs, CREATE_sJ(o, j, k));
}

static int codeextraarg (FuncState *fs, int a) {
  lua_assert(a <= MAXARG_Ax);
  return luaK_code(fs, CREATE_Ax(OP_EXTRAARG, a));
}

static int luaK_codek (FuncState *fs, int reg, int k) {
  if (k <= MAXARG_Bx)
    return luaK_codeABx(fs, OP_LOADK, reg, k);
  else {
    int p = luaK_codeABx(fs, OP_LOADKX, reg, 0);
    codeextraarg(fs, k);
    return p;
  }
}

void luaK_checkstack (FuncState *fs, int n) {
  int newstack = fs->freereg + n;
  if (newstack > fs->f->maxstacksize) {
    if (newstack >= MAXREGS)
      luaX_syntaxerror(fs->ls,
        "function or expression needs too many registers");
    fs->f->maxstacksize = cast_byte(newstack);
  }
}

void luaK_reserveregs (FuncState *fs, int n) {
  luaK_checkstack(fs, n);
  fs->freereg += n;
}

static void freereg (FuncState *fs, int reg) {
  if (reg >= luaY_nvarstack(fs)) {
    fs->freereg--;
    lua_assert(reg == fs->freereg);
  }
}

static void freeregs (FuncState *fs, int r1, int r2) {
  if (r1 > r2) {
    freereg(fs, r1);
    freereg(fs, r2);
  }
  else {
    freereg(fs, r2);
    freereg(fs, r1);
  }
}

static void freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->u.info);
}

static void freeexps (FuncState *fs, expdesc *e1, expdesc *e2) {
  int r1 = (e1->k == VNONRELOC) ? e1->u.info : -1;
  int r2 = (e2->k == VNONRELOC) ? e2->u.info : -1;
  freeregs(fs, r1, r2);
}

static int addk (FuncState *fs, TValue *key, TValue *v) {
  TValue val;
  lua_State *L = fs->ls->L;
  Proto *f = fs->f;
  const TValue *idx = luaH_get(fs->ls->h, key);
  int k, oldsize;
  if (ttisinteger(idx)) {
    k = cast_int(ivalue(idx));

    if (k < fs->nk && ttypetag(&f->k[k]) == ttypetag(v) &&
                      luaV_rawequalobj(&f->k[k], v))
      return k;
  }

  oldsize = f->sizek;
  k = fs->nk;

  setivalue(&val, k);
  luaH_finishset(L, fs->ls->h, key, idx, &val);
  luaM_growvector(L, f->k, k, f->sizek, TValue, MAXARG_Ax, "constants");
  while (oldsize < f->sizek) setnilvalue(&f->k[oldsize++]);
  setobj(L, &f->k[k], v);
  fs->nk++;
  luaC_barrier(L, f, v);
  return k;
}

static int stringK (FuncState *fs, TString *s) {
  TValue o;
  setsvalue(fs->ls->L, &o, s);
  return addk(fs, &o, &o);
}

static int luaK_intK (FuncState *fs, lua_Integer n) {
  TValue o;
  setivalue(&o, n);
  return addk(fs, &o, &o);
}

static int luaK_numberK (FuncState *fs, lua_Number r) {
  TValue o;
  lua_Integer ik;
  setfltvalue(&o, r);
  if (!luaV_flttointeger(r, &ik, F2Ieq))
    return addk(fs, &o, &o);
  else {
    const int nbm = l_floatatt(MANT_DIG);
    const lua_Number q = l_mathop(ldexp)(l_mathop(1.0), -nbm + 1);
    const lua_Number k = (ik == 0) ? q : r + r*q;
    TValue kv;
    setfltvalue(&kv, k);

    lua_assert(!luaV_flttointeger(k, &ik, F2Ieq) ||
                l_mathop(fabs)(r) >= l_mathop(1e6));
    return addk(fs, &kv, &o);
  }
}

static int boolF (FuncState *fs) {
  TValue o;
  setbfvalue(&o);
  return addk(fs, &o, &o);
}

static int boolT (FuncState *fs) {
  TValue o;
  setbtvalue(&o);
  return addk(fs, &o, &o);
}

static int nilK (FuncState *fs) {
  TValue k, v;
  setnilvalue(&v);

  sethvalue(fs->ls->L, &k, fs->ls->h);
  return addk(fs, &k, &v);
}

static int fitsC (lua_Integer i) {
  return (l_castS2U(i) + OFFSET_sC <= cast_uint(MAXARG_C));
}

static int fitsBx (lua_Integer i) {
  return (-OFFSET_sBx <= i && i <= MAXARG_Bx - OFFSET_sBx);
}

void luaK_int (FuncState *fs, int reg, lua_Integer i) {
  if (fitsBx(i))
    codeAsBx(fs, OP_LOADI, reg, cast_int(i));
  else
    luaK_codek(fs, reg, luaK_intK(fs, i));
}

static void luaK_float (FuncState *fs, int reg, lua_Number f) {
  lua_Integer fi;
  if (luaV_flttointeger(f, &fi, F2Ieq) && fitsBx(fi))
    codeAsBx(fs, OP_LOADF, reg, cast_int(fi));
  else
    luaK_codek(fs, reg, luaK_numberK(fs, f));
}

static void const2exp (TValue *v, expdesc *e) {
  switch (ttypetag(v)) {
    case LUA_VNUMINT:
      e->k = VKINT; e->u.ival = ivalue(v);
      break;
    case LUA_VNUMFLT:
      e->k = VKFLT; e->u.nval = fltvalue(v);
      break;
    case LUA_VFALSE:
      e->k = VFALSE;
      break;
    case LUA_VTRUE:
      e->k = VTRUE;
      break;
    case LUA_VNIL:
      e->k = VNIL;
      break;
    case LUA_VSHRSTR:  case LUA_VLNGSTR:
      e->k = VKSTR; e->u.strval = tsvalue(v);
      break;
    default: lua_assert(0);
  }
}

void luaK_setreturns (FuncState *fs, expdesc *e, int nresults) {
  Instruction *pc = &getinstruction(fs, e);
  if (e->k == VCALL)
    SETARG_C(*pc, nresults + 1);
  else {
    lua_assert(e->k == VVARARG);
    SETARG_C(*pc, nresults + 1);
    SETARG_A(*pc, fs->freereg);
    luaK_reserveregs(fs, 1);
  }
}

static void str2K (FuncState *fs, expdesc *e) {
  lua_assert(e->k == VKSTR);
  e->u.info = stringK(fs, e->u.strval);
  e->k = VK;
}

void luaK_setoneret (FuncState *fs, expdesc *e) {
  if (e->k == VCALL) {

    lua_assert(GETARG_C(getinstruction(fs, e)) == 2);
    e->k = VNONRELOC;
    e->u.info = GETARG_A(getinstruction(fs, e));
  }
  else if (e->k == VVARARG) {
    SETARG_C(getinstruction(fs, e), 2);
    e->k = VRELOC;
  }
}

void luaK_dischargevars (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VCONST: {
      const2exp(const2val(fs, e), e);
      break;
    }
    case VLOCAL: {
      int temp = e->u.var.ridx;
      e->u.info = temp;
      e->k = VNONRELOC;
      break;
    }
    case VUPVAL: {
      e->u.info = luaK_codeABC(fs, OP_GETUPVAL, 0, e->u.info, 0);
      e->k = VRELOC;
      break;
    }
    case VINDEXUP: {
      e->u.info = luaK_codeABC(fs, OP_GETTABUP, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXI: {
      freereg(fs, e->u.ind.t);
      e->u.info = luaK_codeABC(fs, OP_GETI, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXSTR: {
      freereg(fs, e->u.ind.t);
      e->u.info = luaK_codeABC(fs, OP_GETFIELD, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXED: {
      freeregs(fs, e->u.ind.t, e->u.ind.idx);
      e->u.info = luaK_codeABC(fs, OP_GETTABLE, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VVARARG: case VCALL: {
      luaK_setoneret(fs, e);
      break;
    }
    default: break;
  }
}

static void discharge2reg (FuncState *fs, expdesc *e, int reg) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      luaK_nil(fs, reg, 1);
      break;
    }
    case VFALSE: {
      luaK_codeABC(fs, OP_LOADFALSE, reg, 0, 0);
      break;
    }
    case VTRUE: {
      luaK_codeABC(fs, OP_LOADTRUE, reg, 0, 0);
      break;
    }
    case VKSTR: {
      str2K(fs, e);
    }
    case VK: {
      luaK_codek(fs, reg, e->u.info);
      break;
    }
    case VKFLT: {
      luaK_float(fs, reg, e->u.nval);
      break;
    }
    case VKINT: {
      luaK_int(fs, reg, e->u.ival);
      break;
    }
    case VRELOC: {
      Instruction *pc = &getinstruction(fs, e);
      SETARG_A(*pc, reg);
      break;
    }
    case VNONRELOC: {
      if (reg != e->u.info)
        luaK_codeABC(fs, OP_MOVE, reg, e->u.info, 0);
      break;
    }
    default: {
      lua_assert(e->k == VJMP);
      return;
    }
  }
  e->u.info = reg;
  e->k = VNONRELOC;
}

static void discharge2anyreg (FuncState *fs, expdesc *e) {
  if (e->k != VNONRELOC) {
    luaK_reserveregs(fs, 1);
    discharge2reg(fs, e, fs->freereg-1);
  }
}

static int code_loadbool (FuncState *fs, int A, OpCode op) {
  luaK_getlabel(fs);
  return luaK_codeABC(fs, op, A, 0, 0);
}

static int need_value (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list)) {
    Instruction i = *getjumpcontrol(fs, list);
    if (GET_OPCODE(i) != OP_TESTSET) return 1;
  }
  return 0;
}

static void exp2reg (FuncState *fs, expdesc *e, int reg) {
  discharge2reg(fs, e, reg);
  if (e->k == VJMP)
    luaK_concat(fs, &e->t, e->u.info);
  if (hasjumps(e)) {
    int final;
    int p_f = NO_JUMP;
    int p_t = NO_JUMP;
    if (need_value(fs, e->t) || need_value(fs, e->f)) {
      int fj = (e->k == VJMP) ? NO_JUMP : luaK_jump(fs);
      p_f = code_loadbool(fs, reg, OP_LFALSESKIP);
      p_t = code_loadbool(fs, reg, OP_LOADTRUE);

      luaK_patchtohere(fs, fj);
    }
    final = luaK_getlabel(fs);
    patchlistaux(fs, e->f, final, reg, p_f);
    patchlistaux(fs, e->t, final, reg, p_t);
  }
  e->f = e->t = NO_JUMP;
  e->u.info = reg;
  e->k = VNONRELOC;
}

void luaK_exp2nextreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  freeexp(fs, e);
  luaK_reserveregs(fs, 1);
  exp2reg(fs, e, fs->freereg - 1);
}

int luaK_exp2anyreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  if (e->k == VNONRELOC) {
    if (!hasjumps(e))
      return e->u.info;
    if (e->u.info >= luaY_nvarstack(fs)) {
      exp2reg(fs, e, e->u.info);
      return e->u.info;
    }

  }
  luaK_exp2nextreg(fs, e);
  return e->u.info;
}

void luaK_exp2anyregup (FuncState *fs, expdesc *e) {
  if (e->k != VUPVAL || hasjumps(e))
    luaK_exp2anyreg(fs, e);
}

void luaK_exp2val (FuncState *fs, expdesc *e) {
  if (e->k == VJMP || hasjumps(e))
    luaK_exp2anyreg(fs, e);
  else
    luaK_dischargevars(fs, e);
}

static int luaK_exp2K (FuncState *fs, expdesc *e) {
  if (!hasjumps(e)) {
    int info;
    switch (e->k) {
      case VTRUE: info = boolT(fs); break;
      case VFALSE: info = boolF(fs); break;
      case VNIL: info = nilK(fs); break;
      case VKINT: info = luaK_intK(fs, e->u.ival); break;
      case VKFLT: info = luaK_numberK(fs, e->u.nval); break;
      case VKSTR: info = stringK(fs, e->u.strval); break;
      case VK: info = e->u.info; break;
      default: return 0;
    }
    if (info <= MAXINDEXRK) {
      e->k = VK;
      e->u.info = info;
      return 1;
    }
  }

  return 0;
}

static int exp2RK (FuncState *fs, expdesc *e) {
  if (luaK_exp2K(fs, e))
    return 1;
  else {
    luaK_exp2anyreg(fs, e);
    return 0;
  }
}

static void codeABRK (FuncState *fs, OpCode o, int a, int b,
                      expdesc *ec) {
  int k = exp2RK(fs, ec);
  luaK_codeABCk(fs, o, a, b, ec->u.info, k);
}

void luaK_storevar (FuncState *fs, expdesc *var, expdesc *ex) {
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, ex);
      exp2reg(fs, ex, var->u.var.ridx);
      return;
    }
    case VUPVAL: {
      int e = luaK_exp2anyreg(fs, ex);
      luaK_codeABC(fs, OP_SETUPVAL, e, var->u.info, 0);
      break;
    }
    case VINDEXUP: {
      codeABRK(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXI: {
      codeABRK(fs, OP_SETI, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXSTR: {
      codeABRK(fs, OP_SETFIELD, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXED: {
      codeABRK(fs, OP_SETTABLE, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    default: lua_assert(0);
  }
  freeexp(fs, ex);
}

void luaK_self (FuncState *fs, expdesc *e, expdesc *key) {
  int ereg;
  luaK_exp2anyreg(fs, e);
  ereg = e->u.info;
  freeexp(fs, e);
  e->u.info = fs->freereg;
  e->k = VNONRELOC;
  luaK_reserveregs(fs, 2);
  codeABRK(fs, OP_SELF, e->u.info, ereg, key);
  freeexp(fs, key);
}

static void negatecondition (FuncState *fs, expdesc *e) {
  Instruction *pc = getjumpcontrol(fs, e->u.info);
  lua_assert(testTMode(GET_OPCODE(*pc)) && GET_OPCODE(*pc) != OP_TESTSET &&
                                           GET_OPCODE(*pc) != OP_TEST);
  SETARG_k(*pc, (GETARG_k(*pc) ^ 1));
}

static int jumponcond (FuncState *fs, expdesc *e, int cond) {
  if (e->k == VRELOC) {
    Instruction ie = getinstruction(fs, e);
    if (GET_OPCODE(ie) == OP_NOT) {
      removelastinstruction(fs);
      return condjump(fs, OP_TEST, GETARG_B(ie), 0, 0, !cond);
    }

  }
  discharge2anyreg(fs, e);
  freeexp(fs, e);
  return condjump(fs, OP_TESTSET, NO_REG, e->u.info, 0, cond);
}

void luaK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {
      negatecondition(fs, e);
      pc = e->u.info;
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      pc = NO_JUMP;
      break;
    }
    default: {
      pc = jumponcond(fs, e, 0);
      break;
    }
  }
  luaK_concat(fs, &e->f, pc);
  luaK_patchtohere(fs, e->t);
  e->t = NO_JUMP;
}

void luaK_goiffalse (FuncState *fs, expdesc *e) {
  int pc;
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {
      pc = e->u.info;
      break;
    }
    case VNIL: case VFALSE: {
      pc = NO_JUMP;
      break;
    }
    default: {
      pc = jumponcond(fs, e, 1);
      break;
    }
  }
  luaK_concat(fs, &e->t, pc);
  luaK_patchtohere(fs, e->f);
  e->f = NO_JUMP;
}

static void codenot (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VNIL: case VFALSE: {
      e->k = VTRUE;
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      e->k = VFALSE;
      break;
    }
    case VJMP: {
      negatecondition(fs, e);
      break;
    }
    case VRELOC:
    case VNONRELOC: {
      discharge2anyreg(fs, e);
      freeexp(fs, e);
      e->u.info = luaK_codeABC(fs, OP_NOT, 0, e->u.info, 0);
      e->k = VRELOC;
      break;
    }
    default: lua_assert(0);
  }

  { int temp = e->f; e->f = e->t; e->t = temp; }
  removevalues(fs, e->f);
  removevalues(fs, e->t);
}

static int isKstr (FuncState *fs, expdesc *e) {
  return (e->k == VK && !hasjumps(e) && e->u.info <= MAXARG_B &&
          ttisshrstring(&fs->f->k[e->u.info]));
}

static int isKint (expdesc *e) {
  return (e->k == VKINT && !hasjumps(e));
}

static int isCint (expdesc *e) {
  return isKint(e) && (l_castS2U(e->u.ival) <= l_castS2U(MAXARG_C));
}

static int isSCint (expdesc *e) {
  return isKint(e) && fitsC(e->u.ival);
}

static int isSCnumber (expdesc *e, int *pi, int *isfloat) {
  lua_Integer i;
  if (e->k == VKINT)
    i = e->u.ival;
  else if (e->k == VKFLT && luaV_flttointeger(e->u.nval, &i, F2Ieq))
    *isfloat = 1;
  else
    return 0;
  if (!hasjumps(e) && fitsC(i)) {
    *pi = int2sC(cast_int(i));
    return 1;
  }
  else
    return 0;
}

void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k) {
  if (k->k == VKSTR)
    str2K(fs, k);
  lua_assert(!hasjumps(t) &&
             (t->k == VLOCAL || t->k == VNONRELOC || t->k == VUPVAL));
  if (t->k == VUPVAL && !isKstr(fs, k))
    luaK_exp2anyreg(fs, t);
  if (t->k == VUPVAL) {
    int temp = t->u.info;
    lua_assert(isKstr(fs, k));
    t->u.ind.t = temp;
    t->u.ind.idx = k->u.info;
    t->k = VINDEXUP;
  }
  else {

    t->u.ind.t = (t->k == VLOCAL) ? t->u.var.ridx: t->u.info;
    if (isKstr(fs, k)) {
      t->u.ind.idx = k->u.info;
      t->k = VINDEXSTR;
    }
    else if (isCint(k)) {
      t->u.ind.idx = cast_int(k->u.ival);
      t->k = VINDEXI;
    }
    else {
      t->u.ind.idx = luaK_exp2anyreg(fs, k);
      t->k = VINDEXED;
    }
  }
}

static int validop (int op, TValue *v1, TValue *v2) {
  switch (op) {
    case LUA_OPBAND: case LUA_OPBOR: case LUA_OPBXOR:
    case LUA_OPSHL: case LUA_OPSHR: case LUA_OPBNOT: {
      lua_Integer i;
      return (luaV_tointegerns(v1, &i, LUA_FLOORN2I) &&
              luaV_tointegerns(v2, &i, LUA_FLOORN2I));
    }
    case LUA_OPDIV: case LUA_OPIDIV: case LUA_OPMOD:
      return (nvalue(v2) != 0);
    default: return 1;
  }
}

static int constfolding (FuncState *fs, int op, expdesc *e1,
                                        const expdesc *e2) {
  TValue v1, v2, res;
  if (!tonumeral(e1, &v1) || !tonumeral(e2, &v2) || !validop(op, &v1, &v2))
    return 0;
  luaO_rawarith(fs->ls->L, op, &v1, &v2, &res);
  if (ttisinteger(&res)) {
    e1->k = VKINT;
    e1->u.ival = ivalue(&res);
  }
  else {
    lua_Number n = fltvalue(&res);
    if (luai_numisnan(n) || n == 0)
      return 0;
    e1->k = VKFLT;
    e1->u.nval = n;
  }
  return 1;
}

l_sinline OpCode binopr2op (BinOpr opr, BinOpr baser, OpCode base) {
  lua_assert(baser <= opr &&
            ((baser == OPR_ADD && opr <= OPR_SHR) ||
             (baser == OPR_LT && opr <= OPR_LE)));
  return cast(OpCode, (cast_int(opr) - cast_int(baser)) + cast_int(base));
}

l_sinline OpCode unopr2op (UnOpr opr) {
  return cast(OpCode, (cast_int(opr) - cast_int(OPR_MINUS)) +
                                       cast_int(OP_UNM));
}

l_sinline TMS binopr2TM (BinOpr opr) {
  lua_assert(OPR_ADD <= opr && opr <= OPR_SHR);
  return cast(TMS, (cast_int(opr) - cast_int(OPR_ADD)) + cast_int(TM_ADD));
}

static void codeunexpval (FuncState *fs, OpCode op, expdesc *e, int line) {
  int r = luaK_exp2anyreg(fs, e);
  freeexp(fs, e);
  e->u.info = luaK_codeABC(fs, op, 0, r, 0);
  e->k = VRELOC;
  luaK_fixline(fs, line);
}

static void finishbinexpval (FuncState *fs, expdesc *e1, expdesc *e2,
                             OpCode op, int v2, int flip, int line,
                             OpCode mmop, TMS event) {
  int v1 = luaK_exp2anyreg(fs, e1);
  int pc = luaK_codeABCk(fs, op, 0, v1, v2, 0);
  freeexps(fs, e1, e2);
  e1->u.info = pc;
  e1->k = VRELOC;
  luaK_fixline(fs, line);
  luaK_codeABCk(fs, mmop, v1, v2, event, flip);
  luaK_fixline(fs, line);
}

static void codebinexpval (FuncState *fs, BinOpr opr,
                           expdesc *e1, expdesc *e2, int line) {
  OpCode op = binopr2op(opr, OPR_ADD, OP_ADD);
  int v2 = luaK_exp2anyreg(fs, e2);

  lua_assert((VNIL <= e1->k && e1->k <= VKSTR) ||
             e1->k == VNONRELOC || e1->k == VRELOC);
  lua_assert(OP_ADD <= op && op <= OP_SHR);
  finishbinexpval(fs, e1, e2, op, v2, 0, line, OP_MMBIN, binopr2TM(opr));
}

static void codebini (FuncState *fs, OpCode op,
                       expdesc *e1, expdesc *e2, int flip, int line,
                       TMS event) {
  int v2 = int2sC(cast_int(e2->u.ival));
  lua_assert(e2->k == VKINT);
  finishbinexpval(fs, e1, e2, op, v2, flip, line, OP_MMBINI, event);
}

static void codebinK (FuncState *fs, BinOpr opr,
                      expdesc *e1, expdesc *e2, int flip, int line) {
  TMS event = binopr2TM(opr);
  int v2 = e2->u.info;
  OpCode op = binopr2op(opr, OPR_ADD, OP_ADDK);
  finishbinexpval(fs, e1, e2, op, v2, flip, line, OP_MMBINK, event);
}

static int finishbinexpneg (FuncState *fs, expdesc *e1, expdesc *e2,
                             OpCode op, int line, TMS event) {
  if (!isKint(e2))
    return 0;
  else {
    lua_Integer i2 = e2->u.ival;
    if (!(fitsC(i2) && fitsC(-i2)))
      return 0;
    else {
      int v2 = cast_int(i2);
      finishbinexpval(fs, e1, e2, op, int2sC(-v2), 0, line, OP_MMBINI, event);

      SETARG_B(fs->f->code[fs->pc - 1], int2sC(v2));
      return 1;
    }
  }
}

static void swapexps (expdesc *e1, expdesc *e2) {
  expdesc temp = *e1; *e1 = *e2; *e2 = temp;
}

static void codebinNoK (FuncState *fs, BinOpr opr,
                        expdesc *e1, expdesc *e2, int flip, int line) {
  if (flip)
    swapexps(e1, e2);
  codebinexpval(fs, opr, e1, e2, line);
}

static void codearith (FuncState *fs, BinOpr opr,
                       expdesc *e1, expdesc *e2, int flip, int line) {
  if (tonumeral(e2, NULL) && luaK_exp2K(fs, e2))
    codebinK(fs, opr, e1, e2, flip, line);
  else
    codebinNoK(fs, opr, e1, e2, flip, line);
}

static void codecommutative (FuncState *fs, BinOpr op,
                             expdesc *e1, expdesc *e2, int line) {
  int flip = 0;
  if (tonumeral(e1, NULL)) {
    swapexps(e1, e2);
    flip = 1;
  }
  if (op == OPR_ADD && isSCint(e2))
    codebini(fs, OP_ADDI, e1, e2, flip, line, TM_ADD);
  else
    codearith(fs, op, e1, e2, flip, line);
}

static void codebitwise (FuncState *fs, BinOpr opr,
                         expdesc *e1, expdesc *e2, int line) {
  int flip = 0;
  if (e1->k == VKINT) {
    swapexps(e1, e2);
    flip = 1;
  }
  if (e2->k == VKINT && luaK_exp2K(fs, e2))
    codebinK(fs, opr, e1, e2, flip, line);
  else
    codebinNoK(fs, opr, e1, e2, flip, line);
}

static void codeorder (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2) {
  int r1, r2;
  int im;
  int isfloat = 0;
  OpCode op;
  if (isSCnumber(e2, &im, &isfloat)) {

    r1 = luaK_exp2anyreg(fs, e1);
    r2 = im;
    op = binopr2op(opr, OPR_LT, OP_LTI);
  }
  else if (isSCnumber(e1, &im, &isfloat)) {

    r1 = luaK_exp2anyreg(fs, e2);
    r2 = im;
    op = binopr2op(opr, OPR_LT, OP_GTI);
  }
  else {
    r1 = luaK_exp2anyreg(fs, e1);
    r2 = luaK_exp2anyreg(fs, e2);
    op = binopr2op(opr, OPR_LT, OP_LT);
  }
  freeexps(fs, e1, e2);
  e1->u.info = condjump(fs, op, r1, r2, isfloat, 1);
  e1->k = VJMP;
}

static void codeeq (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2) {
  int r1, r2;
  int im;
  int isfloat = 0;
  OpCode op;
  if (e1->k != VNONRELOC) {
    lua_assert(e1->k == VK || e1->k == VKINT || e1->k == VKFLT);
    swapexps(e1, e2);
  }
  r1 = luaK_exp2anyreg(fs, e1);
  if (isSCnumber(e2, &im, &isfloat)) {
    op = OP_EQI;
    r2 = im;
  }
  else if (exp2RK(fs, e2)) {
    op = OP_EQK;
    r2 = e2->u.info;
  }
  else {
    op = OP_EQ;
    r2 = luaK_exp2anyreg(fs, e2);
  }
  freeexps(fs, e1, e2);
  e1->u.info = condjump(fs, op, r1, r2, isfloat, (opr == OPR_EQ));
  e1->k = VJMP;
}

void luaK_prefix (FuncState *fs, UnOpr opr, expdesc *e, int line) {
  static const expdesc ef = {VKINT, {0}, NO_JUMP, NO_JUMP};
  luaK_dischargevars(fs, e);
  switch (opr) {
    case OPR_MINUS: case OPR_BNOT:
      if (constfolding(fs, opr + LUA_OPUNM, e, &ef))
        break;

    case OPR_LEN:
      codeunexpval(fs, unopr2op(opr), e, line);
      break;
    case OPR_NOT: codenot(fs, e); break;
    default: lua_assert(0);
  }
}

void luaK_infix (FuncState *fs, BinOpr op, expdesc *v) {
  luaK_dischargevars(fs, v);
  switch (op) {
    case OPR_AND: {
      luaK_goiftrue(fs, v);
      break;
    }
    case OPR_OR: {
      luaK_goiffalse(fs, v);
      break;
    }
    case OPR_CONCAT: {
      luaK_exp2nextreg(fs, v);
      break;
    }
    case OPR_ADD: case OPR_SUB:
    case OPR_MUL: case OPR_DIV: case OPR_IDIV:
    case OPR_MOD: case OPR_POW:
    case OPR_BAND: case OPR_BOR: case OPR_BXOR:
    case OPR_SHL: case OPR_SHR: {
      if (!tonumeral(v, NULL))
        luaK_exp2anyreg(fs, v);

      break;
    }
    case OPR_EQ: case OPR_NE: {
      if (!tonumeral(v, NULL))
        exp2RK(fs, v);

      break;
    }
    case OPR_LT: case OPR_LE:
    case OPR_GT: case OPR_GE: {
      int dummy, dummy2;
      if (!isSCnumber(v, &dummy, &dummy2))
        luaK_exp2anyreg(fs, v);

      break;
    }
    default: lua_assert(0);
  }
}

static void codeconcat (FuncState *fs, expdesc *e1, expdesc *e2, int line) {
  Instruction *ie2 = previousinstruction(fs);
  if (GET_OPCODE(*ie2) == OP_CONCAT) {
    int n = GETARG_B(*ie2);
    lua_assert(e1->u.info + 1 == GETARG_A(*ie2));
    freeexp(fs, e2);
    SETARG_A(*ie2, e1->u.info);
    SETARG_B(*ie2, n + 1);
  }
  else {
    luaK_codeABC(fs, OP_CONCAT, e1->u.info, 2, 0);
    freeexp(fs, e2);
    luaK_fixline(fs, line);
  }
}

void luaK_posfix (FuncState *fs, BinOpr opr,
                  expdesc *e1, expdesc *e2, int line) {
  luaK_dischargevars(fs, e2);
  if (foldbinop(opr) && constfolding(fs, opr + LUA_OPADD, e1, e2))
    return;
  switch (opr) {
    case OPR_AND: {
      lua_assert(e1->t == NO_JUMP);
      luaK_concat(fs, &e2->f, e1->f);
      *e1 = *e2;
      break;
    }
    case OPR_OR: {
      lua_assert(e1->f == NO_JUMP);
      luaK_concat(fs, &e2->t, e1->t);
      *e1 = *e2;
      break;
    }
    case OPR_CONCAT: {
      luaK_exp2nextreg(fs, e2);
      codeconcat(fs, e1, e2, line);
      break;
    }
    case OPR_ADD: case OPR_MUL: {
      codecommutative(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SUB: {
      if (finishbinexpneg(fs, e1, e2, OP_ADDI, line, TM_SUB))
        break;

    }
    case OPR_DIV: case OPR_IDIV: case OPR_MOD: case OPR_POW: {
      codearith(fs, opr, e1, e2, 0, line);
      break;
    }
    case OPR_BAND: case OPR_BOR: case OPR_BXOR: {
      codebitwise(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SHL: {
      if (isSCint(e1)) {
        swapexps(e1, e2);
        codebini(fs, OP_SHLI, e1, e2, 1, line, TM_SHL);
      }
      else if (finishbinexpneg(fs, e1, e2, OP_SHRI, line, TM_SHL)) {
        ;
      }
      else
       codebinexpval(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SHR: {
      if (isSCint(e2))
        codebini(fs, OP_SHRI, e1, e2, 0, line, TM_SHR);
      else
        codebinexpval(fs, opr, e1, e2, line);
      break;
    }
    case OPR_EQ: case OPR_NE: {
      codeeq(fs, opr, e1, e2);
      break;
    }
    case OPR_GT: case OPR_GE: {

      swapexps(e1, e2);
      opr = cast(BinOpr, (opr - OPR_GT) + OPR_LT);
    }
    case OPR_LT: case OPR_LE: {
      codeorder(fs, opr, e1, e2);
      break;
    }
    default: lua_assert(0);
  }
}

void luaK_fixline (FuncState *fs, int line) {
  removelastlineinfo(fs);
  savelineinfo(fs, fs->f, line);
}

void luaK_settablesize (FuncState *fs, int pc, int ra, int asize, int hsize) {
  Instruction *inst = &fs->f->code[pc];
  int rb = (hsize != 0) ? luaO_ceillog2(hsize) + 1 : 0;
  int extra = asize / (MAXARG_C + 1);
  int rc = asize % (MAXARG_C + 1);
  int k = (extra > 0);
  *inst = CREATE_ABCk(OP_NEWTABLE, ra, rb, rc, k);
  *(inst + 1) = CREATE_Ax(OP_EXTRAARG, extra);
}

void luaK_setlist (FuncState *fs, int base, int nelems, int tostore) {
  lua_assert(tostore != 0 && tostore <= LFIELDS_PER_FLUSH);
  if (tostore == LUA_MULTRET)
    tostore = 0;
  if (nelems <= MAXARG_C)
    luaK_codeABC(fs, OP_SETLIST, base, tostore, nelems);
  else {
    int extra = nelems / (MAXARG_C + 1);
    nelems %= (MAXARG_C + 1);
    luaK_codeABCk(fs, OP_SETLIST, base, tostore, nelems, 1);
    codeextraarg(fs, extra);
  }
  fs->freereg = base + 1;
}

static int finaltarget (Instruction *code, int i) {
  int count;
  for (count = 0; count < 100; count++) {
    Instruction pc = code[i];
    if (GET_OPCODE(pc) != OP_JMP)
      break;
     else
       i += GETARG_sJ(pc) + 1;
  }
  return i;
}

void luaK_finish (FuncState *fs) {
  int i;
  Proto *p = fs->f;
  for (i = 0; i < fs->pc; i++) {
    Instruction *pc = &p->code[i];
    lua_assert(i == 0 || isOT(*(pc - 1)) == isIT(*pc));
    switch (GET_OPCODE(*pc)) {
      case OP_RETURN0: case OP_RETURN1: {
        if (!(fs->needclose || p->is_vararg))
          break;

        SET_OPCODE(*pc, OP_RETURN);
      }
      case OP_RETURN: case OP_TAILCALL: {
        if (fs->needclose)
          SETARG_k(*pc, 1);
        if (p->is_vararg)
          SETARG_C(*pc, p->numparams + 1);
        break;
      }
      case OP_JMP: {
        int target = finaltarget(p->code, i);
        fixjump(fs, i, target);
        break;
      }
      default: break;
    }
  }
}
