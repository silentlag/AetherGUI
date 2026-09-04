
#define lobject_c
#define LUA_CORE

#include "lprefix.h"

#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"

int luaO_ceillog2 (unsigned int x) {
  static const lu_byte log_2[256] = {
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}

static lua_Integer intarith (lua_State *L, int op, lua_Integer v1,
                                                   lua_Integer v2) {
  switch (op) {
    case LUA_OPADD: return intop(+, v1, v2);
    case LUA_OPSUB:return intop(-, v1, v2);
    case LUA_OPMUL:return intop(*, v1, v2);
    case LUA_OPMOD: return luaV_mod(L, v1, v2);
    case LUA_OPIDIV: return luaV_idiv(L, v1, v2);
    case LUA_OPBAND: return intop(&, v1, v2);
    case LUA_OPBOR: return intop(|, v1, v2);
    case LUA_OPBXOR: return intop(^, v1, v2);
    case LUA_OPSHL: return luaV_shiftl(v1, v2);
    case LUA_OPSHR: return luaV_shiftr(v1, v2);
    case LUA_OPUNM: return intop(-, 0, v1);
    case LUA_OPBNOT: return intop(^, ~l_castS2U(0), v1);
    default: lua_assert(0); return 0;
  }
}

static lua_Number numarith (lua_State *L, int op, lua_Number v1,
                                                  lua_Number v2) {
  switch (op) {
    case LUA_OPADD: return luai_numadd(L, v1, v2);
    case LUA_OPSUB: return luai_numsub(L, v1, v2);
    case LUA_OPMUL: return luai_nummul(L, v1, v2);
    case LUA_OPDIV: return luai_numdiv(L, v1, v2);
    case LUA_OPPOW: return luai_numpow(L, v1, v2);
    case LUA_OPIDIV: return luai_numidiv(L, v1, v2);
    case LUA_OPUNM: return luai_numunm(L, v1);
    case LUA_OPMOD: return luaV_modf(L, v1, v2);
    default: lua_assert(0); return 0;
  }
}

int luaO_rawarith (lua_State *L, int op, const TValue *p1, const TValue *p2,
                   TValue *res) {
  switch (op) {
    case LUA_OPBAND: case LUA_OPBOR: case LUA_OPBXOR:
    case LUA_OPSHL: case LUA_OPSHR:
    case LUA_OPBNOT: {
      lua_Integer i1; lua_Integer i2;
      if (tointegerns(p1, &i1) && tointegerns(p2, &i2)) {
        setivalue(res, intarith(L, op, i1, i2));
        return 1;
      }
      else return 0;
    }
    case LUA_OPDIV: case LUA_OPPOW: {
      lua_Number n1; lua_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;
    }
    default: {
      lua_Number n1; lua_Number n2;
      if (ttisinteger(p1) && ttisinteger(p2)) {
        setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
        return 1;
      }
      else if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;
    }
  }
}

void luaO_arith (lua_State *L, int op, const TValue *p1, const TValue *p2,
                 StkId res) {
  if (!luaO_rawarith(L, op, p1, p2, s2v(res))) {

    luaT_trybinTM(L, p1, p2, res, cast(TMS, (op - LUA_OPADD) + TM_ADD));
  }
}

int luaO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return (ltolower(c) - 'a') + 10;
}

static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}

#if !defined(lua_strx2number)

#define MAXSIGDIG	30

static lua_Number lua_strx2number (const char *s, char **endptr) {
  int dot = lua_getlocaledecpoint();
  lua_Number r = l_mathop(0.0);
  int sigdig = 0;
  int nosigdig = 0;
  int e = 0;
  int neg;
  int hasdot = 0;
  *endptr = cast_charp(s);
  while (lisspace(cast_uchar(*s))) s++;
  neg = isneg(&s);
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')))
    return l_mathop(0.0);
  for (s += 2; ; s++) {
    if (*s == dot) {
      if (hasdot) break;
      else hasdot = 1;
    }
    else if (lisxdigit(cast_uchar(*s))) {
      if (sigdig == 0 && *s == '0')
        nosigdig++;
      else if (++sigdig <= MAXSIGDIG)
          r = (r * l_mathop(16.0)) + luaO_hexavalue(*s);
      else e++;
      if (hasdot) e--;
    }
    else break;
  }
  if (nosigdig + sigdig == 0)
    return l_mathop(0.0);
  *endptr = cast_charp(s);
  e *= 4;
  if (*s == 'p' || *s == 'P') {
    int exp1 = 0;
    int neg1;
    s++;
    neg1 = isneg(&s);
    if (!lisdigit(cast_uchar(*s)))
      return l_mathop(0.0);
    while (lisdigit(cast_uchar(*s)))
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
    *endptr = cast_charp(s);
  }
  if (neg) r = -r;
  return l_mathop(ldexp)(r, e);
}

#endif

#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM	200
#endif

static const char *l_str2dloc (const char *s, lua_Number *result, int mode) {
  char *endptr;
  *result = (mode == 'x') ? lua_strx2number(s, &endptr)
                          : lua_str2number(s, &endptr);
  if (endptr == s) return NULL;
  while (lisspace(cast_uchar(*endptr))) endptr++;
  return (*endptr == '\0') ? endptr : NULL;
}

static const char *l_str2d (const char *s, lua_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')
    return NULL;
  endptr = l_str2dloc(s, result, mode);
  if (endptr == NULL) {
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (pdot == NULL || strlen(s) > L_MAXLENNUM)
      return NULL;
    strcpy(buff, s);
    buff[pdot - s] = lua_getlocaledecpoint();
    endptr = l_str2dloc(buff, result, mode);
    if (endptr != NULL)
      endptr = s + (endptr - buff);
  }
  return endptr;
}

#define MAXBY10		cast(lua_Unsigned, LUA_MAXINTEGER / 10)
#define MAXLASTD	cast_int(LUA_MAXINTEGER % 10)

static const char *l_str2int (const char *s, lua_Integer *result) {
  lua_Unsigned a = 0;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;
  neg = isneg(&s);
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + luaO_hexavalue(*s);
      empty = 0;
    }
  }
  else {
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))
        return NULL;
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;
  if (empty || *s != '\0') return NULL;
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}

size_t luaO_str2num (const char *s, TValue *o) {
  lua_Integer i; lua_Number n;
  const char *e;
  if ((e = l_str2int(s, &i)) != NULL) {
    setivalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {
    setfltvalue(o, n);
  }
  else
    return 0;
  return (e - s) + 1;
}

int luaO_utf8esc (char *buff, unsigned long x) {
  int n = 1;
  lua_assert(x <= 0x7FFFFFFFu);
  if (x < 0x80)
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else {
    unsigned int mfb = 0x3f;
    do {
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;
      mfb >>= 1;
    } while (x > mfb);
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x);
  }
  return n;
}

#define MAXNUMBER2STR	44

static int tostringbuff (TValue *obj, char *buff) {
  int len;
  lua_assert(ttisnumber(obj));
  if (ttisinteger(obj))
    len = lua_integer2str(buff, MAXNUMBER2STR, ivalue(obj));
  else {
    len = lua_number2str(buff, MAXNUMBER2STR, fltvalue(obj));
    if (buff[strspn(buff, "-0123456789")] == '\0') {
      buff[len++] = lua_getlocaledecpoint();
      buff[len++] = '0';
    }
  }
  return len;
}

void luaO_tostring (lua_State *L, TValue *obj) {
  char buff[MAXNUMBER2STR];
  int len = tostringbuff(obj, buff);
  setsvalue(L, obj, luaS_newlstr(L, buff, len));
}

#define BUFVFS		(LUA_IDSIZE + MAXNUMBER2STR + 95)

typedef struct BuffFS {
  lua_State *L;
  int pushed;
  int blen;
  char space[BUFVFS];
} BuffFS;

static void pushstr (BuffFS *buff, const char *str, size_t lstr) {
  lua_State *L = buff->L;
  setsvalue2s(L, L->top.p, luaS_newlstr(L, str, lstr));
  L->top.p++;
  if (!buff->pushed)
    buff->pushed = 1;
  else
    luaV_concat(L, 2);
}

static void clearbuff (BuffFS *buff) {
  pushstr(buff, buff->space, buff->blen);
  buff->blen = 0;
}

static char *getbuff (BuffFS *buff, int sz) {
  lua_assert(buff->blen <= BUFVFS); lua_assert(sz <= BUFVFS);
  if (sz > BUFVFS - buff->blen)
    clearbuff(buff);
  return buff->space + buff->blen;
}

#define addsize(b,sz)	((b)->blen += (sz))

static void addstr2buff (BuffFS *buff, const char *str, size_t slen) {
  if (slen <= BUFVFS) {
    char *bf = getbuff(buff, cast_int(slen));
    memcpy(bf, str, slen);
    addsize(buff, cast_int(slen));
  }
  else {
    clearbuff(buff);
    pushstr(buff, str, slen);
  }
}

static void addnum2buff (BuffFS *buff, TValue *num) {
  char *numbuff = getbuff(buff, MAXNUMBER2STR);
  int len = tostringbuff(num, numbuff);
  addsize(buff, len);
}

const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp) {
  BuffFS buff;
  const char *e;
  buff.pushed = buff.blen = 0;
  buff.L = L;
  while ((e = strchr(fmt, '%')) != NULL) {
    addstr2buff(&buff, fmt, e - fmt);
    switch (*(e + 1)) {
      case 's': {
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        addstr2buff(&buff, s, strlen(s));
        break;
      }
      case 'c': {
        char c = cast_uchar(va_arg(argp, int));
        addstr2buff(&buff, &c, sizeof(char));
        break;
      }
      case 'd': {
        TValue num;
        setivalue(&num, va_arg(argp, int));
        addnum2buff(&buff, &num);
        break;
      }
      case 'I': {
        TValue num;
        setivalue(&num, cast(lua_Integer, va_arg(argp, l_uacInt)));
        addnum2buff(&buff, &num);
        break;
      }
      case 'f': {
        TValue num;
        setfltvalue(&num, cast_num(va_arg(argp, l_uacNumber)));
        addnum2buff(&buff, &num);
        break;
      }
      case 'p': {
        const int sz = 3 * sizeof(void*) + 8;
        char *bf = getbuff(&buff, sz);
        void *p = va_arg(argp, void *);
        int len = lua_pointer2str(bf, sz, p);
        addsize(&buff, len);
        break;
      }
      case 'U': {
        char bf[UTF8BUFFSZ];
        int len = luaO_utf8esc(bf, va_arg(argp, long));
        addstr2buff(&buff, bf + UTF8BUFFSZ - len, len);
        break;
      }
      case '%': {
        addstr2buff(&buff, "%", 1);
        break;
      }
      default: {
        luaG_runerror(L, "invalid option '%%%c' to 'lua_pushfstring'",
                         *(e + 1));
      }
    }
    fmt = e + 2;
  }
  addstr2buff(&buff, fmt, strlen(fmt));
  clearbuff(&buff);
  lua_assert(buff.pushed == 1);
  return getstr(tsvalue(s2v(L->top.p - 1)));
}

const char *luaO_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}

#define RETS	"..."
#define PRE	"[string \""
#define POS	"\"]"

#define addstr(a,b,l)	( memcpy(a,b,(l) * sizeof(char)), a += (l) )

void luaO_chunkid (char *out, const char *source, size_t srclen) {
  size_t bufflen = LUA_IDSIZE;
  if (*source == '=') {
    if (srclen <= bufflen)
      memcpy(out, source + 1, srclen * sizeof(char));
    else {
      addstr(out, source + 1, bufflen - 1);
      *out = '\0';
    }
  }
  else if (*source == '@') {
    if (srclen <= bufflen)
      memcpy(out, source + 1, srclen * sizeof(char));
    else {
      addstr(out, RETS, LL(RETS));
      bufflen -= LL(RETS);
      memcpy(out, source + 1 + srclen - bufflen, bufflen * sizeof(char));
    }
  }
  else {
    const char *nl = strchr(source, '\n');
    addstr(out, PRE, LL(PRE));
    bufflen -= LL(PRE RETS POS) + 1;
    if (srclen < bufflen && nl == NULL) {
      addstr(out, source, srclen);
    }
    else {
      if (nl != NULL) srclen = nl - source;
      if (srclen > bufflen) srclen = bufflen;
      addstr(out, source, srclen);
      addstr(out, RETS, LL(RETS));
    }
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}
