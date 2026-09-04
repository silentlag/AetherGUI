
#define ltable_c
#define LUA_CORE

#include "lprefix.h"

#include <math.h>
#include <limits.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"

#define MAXABITS	cast_int(sizeof(int) * CHAR_BIT - 1)

#define MAXASIZE	luaM_limitN(1u << MAXABITS, TValue)

#define MAXHBITS	(MAXABITS - 1)

#define MAXHSIZE	luaM_limitN(1u << MAXHBITS, Node)

#define hashpow2(t,n)		(gnode(t, lmod((n), sizenode(t))))

#define hashmod(t,n)	(gnode(t, ((n) % ((sizenode(t)-1)|1))))

#define hashstr(t,str)		hashpow2(t, (str)->hash)
#define hashboolean(t,p)	hashpow2(t, p)

#define hashpointer(t,p)	hashmod(t, point2uint(p))

#define dummynode		(&dummynode_)

static const Node dummynode_ = {
  {{NULL}, LUA_VEMPTY,
   LUA_VNIL, 0, {NULL}}
};

static const TValue absentkey = {ABSTKEYCONSTANT};

static Node *hashint (const Table *t, lua_Integer i) {
  lua_Unsigned ui = l_castS2U(i);
  if (ui <= cast_uint(INT_MAX))
    return hashmod(t, cast_int(ui));
  else
    return hashmod(t, ui);
}

#if !defined(l_hashfloat)
static int l_hashfloat (lua_Number n) {
  int i;
  lua_Integer ni;
  n = l_mathop(frexp)(n, &i) * -cast_num(INT_MIN);
  if (!lua_numbertointeger(n, &ni)) {
    lua_assert(luai_numisnan(n) || l_mathop(fabs)(n) == cast_num(HUGE_VAL));
    return 0;
  }
  else {
    unsigned int u = cast_uint(i) + cast_uint(ni);
    return cast_int(u <= cast_uint(INT_MAX) ? u : ~u);
  }
}
#endif

static Node *mainpositionTV (const Table *t, const TValue *key) {
  switch (ttypetag(key)) {
    case LUA_VNUMINT: {
      lua_Integer i = ivalue(key);
      return hashint(t, i);
    }
    case LUA_VNUMFLT: {
      lua_Number n = fltvalue(key);
      return hashmod(t, l_hashfloat(n));
    }
    case LUA_VSHRSTR: {
      TString *ts = tsvalue(key);
      return hashstr(t, ts);
    }
    case LUA_VLNGSTR: {
      TString *ts = tsvalue(key);
      return hashpow2(t, luaS_hashlongstr(ts));
    }
    case LUA_VFALSE:
      return hashboolean(t, 0);
    case LUA_VTRUE:
      return hashboolean(t, 1);
    case LUA_VLIGHTUSERDATA: {
      void *p = pvalue(key);
      return hashpointer(t, p);
    }
    case LUA_VLCF: {
      lua_CFunction f = fvalue(key);
      return hashpointer(t, f);
    }
    default: {
      GCObject *o = gcvalue(key);
      return hashpointer(t, o);
    }
  }
}

l_sinline Node *mainpositionfromnode (const Table *t, Node *nd) {
  TValue key;
  getnodekey(cast(lua_State *, NULL), &key, nd);
  return mainpositionTV(t, &key);
}

static int equalkey (const TValue *k1, const Node *n2, int deadok) {
  if ((rawtt(k1) != keytt(n2)) &&
       !(deadok && keyisdead(n2) && iscollectable(k1)))
   return 0;
  switch (keytt(n2)) {
    case LUA_VNIL: case LUA_VFALSE: case LUA_VTRUE:
      return 1;
    case LUA_VNUMINT:
      return (ivalue(k1) == keyival(n2));
    case LUA_VNUMFLT:
      return luai_numeq(fltvalue(k1), fltvalueraw(keyval(n2)));
    case LUA_VLIGHTUSERDATA:
      return pvalue(k1) == pvalueraw(keyval(n2));
    case LUA_VLCF:
      return fvalue(k1) == fvalueraw(keyval(n2));
    case ctb(LUA_VLNGSTR):
      return luaS_eqlngstr(tsvalue(k1), keystrval(n2));
    default:
      return gcvalue(k1) == gcvalueraw(keyval(n2));
  }
}

#define limitequalsasize(t)	(isrealasize(t) || ispow2((t)->alimit))

LUAI_FUNC unsigned int luaH_realasize (const Table *t) {
  if (limitequalsasize(t))
    return t->alimit;
  else {
    unsigned int size = t->alimit;

    size |= (size >> 1);
    size |= (size >> 2);
    size |= (size >> 4);
    size |= (size >> 8);
#if (UINT_MAX >> 14) > 3
    size |= (size >> 16);
#if (UINT_MAX >> 30) > 3
    size |= (size >> 32);
#endif
#endif
    size++;
    lua_assert(ispow2(size) && size/2 < t->alimit && t->alimit < size);
    return size;
  }
}

static int ispow2realasize (const Table *t) {
  return (!isrealasize(t) || ispow2(t->alimit));
}

static unsigned int setlimittosize (Table *t) {
  t->alimit = luaH_realasize(t);
  setrealasize(t);
  return t->alimit;
}

#define limitasasize(t)	check_exp(isrealasize(t), t->alimit)

static const TValue *getgeneric (Table *t, const TValue *key, int deadok) {
  Node *n = mainpositionTV(t, key);
  for (;;) {
    if (equalkey(key, n, deadok))
      return gval(n);
    else {
      int nx = gnext(n);
      if (nx == 0)
        return &absentkey;
      n += nx;
    }
  }
}

static unsigned int arrayindex (lua_Integer k) {
  if (l_castS2U(k) - 1u < MAXASIZE)
    return cast_uint(k);
  else
    return 0;
}

static unsigned int findindex (lua_State *L, Table *t, TValue *key,
                               unsigned int asize) {
  unsigned int i;
  if (ttisnil(key)) return 0;
  i = ttisinteger(key) ? arrayindex(ivalue(key)) : 0;
  if (i - 1u < asize)
    return i;
  else {
    const TValue *n = getgeneric(t, key, 1);
    if (l_unlikely(isabstkey(n)))
      luaG_runerror(L, "invalid key to 'next'");
    i = cast_int(nodefromval(n) - gnode(t, 0));

    return (i + 1) + asize;
  }
}

int luaH_next (lua_State *L, Table *t, StkId key) {
  unsigned int asize = luaH_realasize(t);
  unsigned int i = findindex(L, t, s2v(key), asize);
  for (; i < asize; i++) {
    if (!isempty(&t->array[i])) {
      setivalue(s2v(key), i + 1);
      setobj2s(L, key + 1, &t->array[i]);
      return 1;
    }
  }
  for (i -= asize; cast_int(i) < sizenode(t); i++) {
    if (!isempty(gval(gnode(t, i)))) {
      Node *n = gnode(t, i);
      getnodekey(L, s2v(key), n);
      setobj2s(L, key + 1, gval(n));
      return 1;
    }
  }
  return 0;
}

static void freehash (lua_State *L, Table *t) {
  if (!isdummy(t))
    luaM_freearray(L, t->node, cast_sizet(sizenode(t)));
}

static unsigned int computesizes (unsigned int nums[], unsigned int *pna) {
  int i;
  unsigned int twotoi;
  unsigned int a = 0;
  unsigned int na = 0;
  unsigned int optimal = 0;

  for (i = 0, twotoi = 1;
       twotoi > 0 && *pna > twotoi / 2;
       i++, twotoi *= 2) {
    a += nums[i];
    if (a > twotoi/2) {
      optimal = twotoi;
      na = a;
    }
  }
  lua_assert((optimal == 0 || optimal / 2 < na) && na <= optimal);
  *pna = na;
  return optimal;
}

static int countint (lua_Integer key, unsigned int *nums) {
  unsigned int k = arrayindex(key);
  if (k != 0) {
    nums[luaO_ceillog2(k)]++;
    return 1;
  }
  else
    return 0;
}

static unsigned int numusearray (const Table *t, unsigned int *nums) {
  int lg;
  unsigned int ttlg;
  unsigned int ause = 0;
  unsigned int i = 1;
  unsigned int asize = limitasasize(t);

  for (lg = 0, ttlg = 1; lg <= MAXABITS; lg++, ttlg *= 2) {
    unsigned int lc = 0;
    unsigned int lim = ttlg;
    if (lim > asize) {
      lim = asize;
      if (i > lim)
        break;
    }

    for (; i <= lim; i++) {
      if (!isempty(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}

static int numusehash (const Table *t, unsigned int *nums, unsigned int *pna) {
  int totaluse = 0;
  int ause = 0;
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!isempty(gval(n))) {
      if (keyisinteger(n))
        ause += countint(keyival(n), nums);
      totaluse++;
    }
  }
  *pna += ause;
  return totaluse;
}

static void setnodevector (lua_State *L, Table *t, unsigned int size) {
  if (size == 0) {
    t->node = cast(Node *, dummynode);
    t->lsizenode = 0;
    t->lastfree = NULL;
  }
  else {
    int i;
    int lsize = luaO_ceillog2(size);
    if (lsize > MAXHBITS || (1u << lsize) > MAXHSIZE)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, Node);
    for (i = 0; i < cast_int(size); i++) {
      Node *n = gnode(t, i);
      gnext(n) = 0;
      setnilkey(n);
      setempty(gval(n));
    }
    t->lsizenode = cast_byte(lsize);
    t->lastfree = gnode(t, size);
  }
}

static void reinsert (lua_State *L, Table *ot, Table *t) {
  int j;
  int size = sizenode(ot);
  for (j = 0; j < size; j++) {
    Node *old = gnode(ot, j);
    if (!isempty(gval(old))) {

      TValue k;
      getnodekey(L, &k, old);
      luaH_set(L, t, &k, gval(old));
    }
  }
}

static void exchangehashpart (Table *t1, Table *t2) {
  lu_byte lsizenode = t1->lsizenode;
  Node *node = t1->node;
  Node *lastfree = t1->lastfree;
  t1->lsizenode = t2->lsizenode;
  t1->node = t2->node;
  t1->lastfree = t2->lastfree;
  t2->lsizenode = lsizenode;
  t2->node = node;
  t2->lastfree = lastfree;
}

void luaH_resize (lua_State *L, Table *t, unsigned int newasize,
                                          unsigned int nhsize) {
  unsigned int i;
  Table newt;
  unsigned int oldasize = setlimittosize(t);
  TValue *newarray;

  setnodevector(L, &newt, nhsize);
  if (newasize < oldasize) {
    t->alimit = newasize;
    exchangehashpart(t, &newt);

    for (i = newasize; i < oldasize; i++) {
      if (!isempty(&t->array[i]))
        luaH_setint(L, t, i + 1, &t->array[i]);
    }
    t->alimit = oldasize;
    exchangehashpart(t, &newt);
  }

  newarray = luaM_reallocvector(L, t->array, oldasize, newasize, TValue);
  if (l_unlikely(newarray == NULL && newasize > 0)) {
    freehash(L, &newt);
    luaM_error(L);
  }

  exchangehashpart(t, &newt);
  t->array = newarray;
  t->alimit = newasize;
  for (i = oldasize; i < newasize; i++)
     setempty(&t->array[i]);

  reinsert(L, &newt, t);
  freehash(L, &newt);
}

void luaH_resizearray (lua_State *L, Table *t, unsigned int nasize) {
  int nsize = allocsizenode(t);
  luaH_resize(L, t, nasize, nsize);
}

static void rehash (lua_State *L, Table *t, const TValue *ek) {
  unsigned int asize;
  unsigned int na;
  unsigned int nums[MAXABITS + 1];
  int i;
  int totaluse;
  for (i = 0; i <= MAXABITS; i++) nums[i] = 0;
  setlimittosize(t);
  na = numusearray(t, nums);
  totaluse = na;
  totaluse += numusehash(t, nums, &na);

  if (ttisinteger(ek))
    na += countint(ivalue(ek), nums);
  totaluse++;

  asize = computesizes(nums, &na);

  luaH_resize(L, t, asize, totaluse - na);
}

Table *luaH_new (lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_VTABLE, sizeof(Table));
  Table *t = gco2t(o);
  t->metatable = NULL;
  t->flags = cast_byte(maskflags);
  t->array = NULL;
  t->alimit = 0;
  setnodevector(L, t, 0);
  return t;
}

void luaH_free (lua_State *L, Table *t) {
  freehash(L, t);
  luaM_freearray(L, t->array, luaH_realasize(t));
  luaM_free(L, t);
}

static Node *getfreepos (Table *t) {
  if (!isdummy(t)) {
    while (t->lastfree > t->node) {
      t->lastfree--;
      if (keyisnil(t->lastfree))
        return t->lastfree;
    }
  }
  return NULL;
}

static void luaH_newkey (lua_State *L, Table *t, const TValue *key,
                                                 TValue *value) {
  Node *mp;
  TValue aux;
  if (l_unlikely(ttisnil(key)))
    luaG_runerror(L, "table index is nil");
  else if (ttisfloat(key)) {
    lua_Number f = fltvalue(key);
    lua_Integer k;
    if (luaV_flttointeger(f, &k, F2Ieq)) {
      setivalue(&aux, k);
      key = &aux;
    }
    else if (l_unlikely(luai_numisnan(f)))
      luaG_runerror(L, "table index is NaN");
  }
  if (ttisnil(value))
    return;
  mp = mainpositionTV(t, key);
  if (!isempty(gval(mp)) || isdummy(t)) {
    Node *othern;
    Node *f = getfreepos(t);
    if (f == NULL) {
      rehash(L, t, key);

      luaH_set(L, t, key, value);
      return;
    }
    lua_assert(!isdummy(t));
    othern = mainpositionfromnode(t, mp);
    if (othern != mp) {

      while (othern + gnext(othern) != mp)
        othern += gnext(othern);
      gnext(othern) = cast_int(f - othern);
      *f = *mp;
      if (gnext(mp) != 0) {
        gnext(f) += cast_int(mp - f);
        gnext(mp) = 0;
      }
      setempty(gval(mp));
    }
    else {

      if (gnext(mp) != 0)
        gnext(f) = cast_int((mp + gnext(mp)) - f);
      else lua_assert(gnext(f) == 0);
      gnext(mp) = cast_int(f - mp);
      mp = f;
    }
  }
  setnodekey(L, mp, key);
  luaC_barrierback(L, obj2gco(t), key);
  lua_assert(isempty(gval(mp)));
  setobj2t(L, gval(mp), value);
}

const TValue *luaH_getint (Table *t, lua_Integer key) {
  lua_Unsigned alimit = t->alimit;
  if (l_castS2U(key) - 1u < alimit)
    return &t->array[key - 1];
  else if (!isrealasize(t) &&
           (((l_castS2U(key) - 1u) & ~(alimit - 1u)) < alimit)) {
    t->alimit = cast_uint(key);
    return &t->array[key - 1];
  }
  else {
    Node *n = hashint(t, key);
    for (;;) {
      if (keyisinteger(n) && keyival(n) == key)
        return gval(n);
      else {
        int nx = gnext(n);
        if (nx == 0) break;
        n += nx;
      }
    }
    return &absentkey;
  }
}

const TValue *luaH_getshortstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  lua_assert(key->tt == LUA_VSHRSTR);
  for (;;) {
    if (keyisshrstr(n) && eqshrstr(keystrval(n), key))
      return gval(n);
    else {
      int nx = gnext(n);
      if (nx == 0)
        return &absentkey;
      n += nx;
    }
  }
}

const TValue *luaH_getstr (Table *t, TString *key) {
  if (key->tt == LUA_VSHRSTR)
    return luaH_getshortstr(t, key);
  else {
    TValue ko;
    setsvalue(cast(lua_State *, NULL), &ko, key);
    return getgeneric(t, &ko, 0);
  }
}

const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttypetag(key)) {
    case LUA_VSHRSTR: return luaH_getshortstr(t, tsvalue(key));
    case LUA_VNUMINT: return luaH_getint(t, ivalue(key));
    case LUA_VNIL: return &absentkey;
    case LUA_VNUMFLT: {
      lua_Integer k;
      if (luaV_flttointeger(fltvalue(key), &k, F2Ieq))
        return luaH_getint(t, k);

    }
    default:
      return getgeneric(t, key, 0);
  }
}

void luaH_finishset (lua_State *L, Table *t, const TValue *key,
                                   const TValue *slot, TValue *value) {
  if (isabstkey(slot))
    luaH_newkey(L, t, key, value);
  else
    setobj2t(L, cast(TValue *, slot), value);
}

void luaH_set (lua_State *L, Table *t, const TValue *key, TValue *value) {
  const TValue *slot = luaH_get(t, key);
  luaH_finishset(L, t, key, slot, value);
}

void luaH_setint (lua_State *L, Table *t, lua_Integer key, TValue *value) {
  const TValue *p = luaH_getint(t, key);
  if (isabstkey(p)) {
    TValue k;
    setivalue(&k, key);
    luaH_newkey(L, t, &k, value);
  }
  else
    setobj2t(L, cast(TValue *, p), value);
}

static lua_Unsigned hash_search (Table *t, lua_Unsigned j) {
  lua_Unsigned i;
  if (j == 0) j++;
  do {
    i = j;
    if (j <= l_castS2U(LUA_MAXINTEGER) / 2)
      j *= 2;
    else {
      j = LUA_MAXINTEGER;
      if (isempty(luaH_getint(t, j)))
        break;
      else
        return j;
    }
  } while (!isempty(luaH_getint(t, j)));

  while (j - i > 1u) {
    lua_Unsigned m = (i + j) / 2;
    if (isempty(luaH_getint(t, m))) j = m;
    else i = m;
  }
  return i;
}

static unsigned int binsearch (const TValue *array, unsigned int i,
                                                    unsigned int j) {
  while (j - i > 1u) {
    unsigned int m = (i + j) / 2;
    if (isempty(&array[m - 1])) j = m;
    else i = m;
  }
  return i;
}

lua_Unsigned luaH_getn (Table *t) {
  unsigned int limit = t->alimit;
  if (limit > 0 && isempty(&t->array[limit - 1])) {

    if (limit >= 2 && !isempty(&t->array[limit - 2])) {

      if (ispow2realasize(t) && !ispow2(limit - 1)) {
        t->alimit = limit - 1;
        setnorealasize(t);
      }
      return limit - 1;
    }
    else {
      unsigned int boundary = binsearch(t->array, 0, limit);

      if (ispow2realasize(t) && boundary > luaH_realasize(t) / 2) {
        t->alimit = boundary;
        setnorealasize(t);
      }
      return boundary;
    }
  }

  if (!limitequalsasize(t)) {

    if (isempty(&t->array[limit]))
      return limit;

    limit = luaH_realasize(t);
    if (isempty(&t->array[limit - 1])) {

      unsigned int boundary = binsearch(t->array, t->alimit, limit);
      t->alimit = boundary;
      return boundary;
    }

  }

  lua_assert(limit == luaH_realasize(t) &&
             (limit == 0 || !isempty(&t->array[limit - 1])));
  if (isdummy(t) || isempty(luaH_getint(t, cast(lua_Integer, limit + 1))))
    return limit;
  else
    return hash_search(t, limit);
}

#if defined(LUA_DEBUG)

Node *luaH_mainposition (const Table *t, const TValue *key) {
  return mainpositionTV(t, key);
}

#endif
