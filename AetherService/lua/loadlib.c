
#define loadlib_c
#define LUA_LIB

#include "lprefix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#if !defined(LUA_CSUBSEP)
#define LUA_CSUBSEP		LUA_DIRSEP
#endif

#if !defined(LUA_LSUBSEP)
#define LUA_LSUBSEP		LUA_DIRSEP
#endif

#define LUA_POF		"luaopen_"

#define LUA_OFSEP	"_"

static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"

#define setprogdir(L)           ((void)0)

typedef void (*voidf)(void);

static void lsys_unloadlib (void *lib);

static void *lsys_load (lua_State *L, const char *path, int seeglb);

static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym);

#if defined(LUA_USE_DLOPEN)

#include <dlfcn.h>

#if defined(__GNUC__)
#define cast_func(p) (__extension__ (lua_CFunction)(p))
#else
#define cast_func(p) ((lua_CFunction)(p))
#endif

static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}

static void *lsys_load (lua_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    lua_pushstring(L, dlerror());
  return lib;
}

static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    lua_pushstring(L, dlerror());
  return f;
}

#elif defined(LUA_DL_DLL)

#include <windows.h>

#if !defined(LUA_LLE_FLAGS)
#define LUA_LLE_FLAGS	0
#endif

#undef setprogdir

static void setprogdir (lua_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    luaL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';
    luaL_gsub(L, lua_tostring(L, -1), LUA_EXEC_DIR, buff);
    lua_remove(L, -2);
  }
}

static void pusherror (lua_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    lua_pushstring(L, buffer);
  else
    lua_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}

static void *lsys_load (lua_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, LUA_LLE_FLAGS);
  (void)(seeglb);
  if (lib == NULL) pusherror(L);
  return lib;
}

static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

#else

#undef LIB_FAIL
#define LIB_FAIL	"absent"

#define DLMSG	"dynamic libraries not enabled; check your Lua installation"

static void lsys_unloadlib (void *lib) {
  (void)(lib);
}

static void *lsys_load (lua_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);
  lua_pushliteral(L, DLMSG);
  return NULL;
}

static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);
  lua_pushliteral(L, DLMSG);
  return NULL;
}

#endif

#if !defined(LUA_PATH_VAR)
#define LUA_PATH_VAR    "LUA_PATH"
#endif

#if !defined(LUA_CPATH_VAR)
#define LUA_CPATH_VAR   "LUA_CPATH"
#endif

static int noenv (lua_State *L) {
  int b;
  lua_getfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
  b = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return b;
}

static void setpath (lua_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = lua_pushfstring(L, "%s%s", envname, LUA_VERSUFFIX);
  const char *path = getenv(nver);
  if (path == NULL)
    path = getenv(envname);
  if (path == NULL || noenv(L))
    lua_pushstring(L, dft);
  else if ((dftmark = strstr(path, LUA_PATH_SEP LUA_PATH_SEP)) == NULL)
    lua_pushstring(L, path);
  else {
    size_t len = strlen(path);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    if (path < dftmark) {
      luaL_addlstring(&b, path, dftmark - path);
      luaL_addchar(&b, *LUA_PATH_SEP);
    }
    luaL_addstring(&b, dft);
    if (dftmark < path + len - 2) {
      luaL_addchar(&b, *LUA_PATH_SEP);
      luaL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    luaL_pushresult(&b);
  }
  setprogdir(L);
  lua_setfield(L, -3, fieldname);
  lua_pop(L, 1);
}

static void *checkclib (lua_State *L, const char *path) {
  void *plib;
  lua_getfield(L, LUA_REGISTRYINDEX, CLIBS);
  lua_getfield(L, -1, path);
  plib = lua_touserdata(L, -1);
  lua_pop(L, 2);
  return plib;
}

static void addtoclib (lua_State *L, const char *path, void *plib) {
  lua_getfield(L, LUA_REGISTRYINDEX, CLIBS);
  lua_pushlightuserdata(L, plib);
  lua_pushvalue(L, -1);
  lua_setfield(L, -3, path);
  lua_rawseti(L, -2, luaL_len(L, -2) + 1);
  lua_pop(L, 1);
}

static int gctm (lua_State *L) {
  lua_Integer n = luaL_len(L, 1);
  for (; n >= 1; n--) {
    lua_rawgeti(L, 1, n);
    lsys_unloadlib(lua_touserdata(L, -1));
    lua_pop(L, 1);
  }
  return 0;
}

#define ERRLIB		1
#define ERRFUNC		2

static int lookforfunc (lua_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);
  if (reg == NULL) {
    reg = lsys_load(L, path, *sym == '*');
    if (reg == NULL) return ERRLIB;
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {
    lua_pushboolean(L, 1);
    return 0;
  }
  else {
    lua_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;
    lua_pushcfunction(L, f);
    return 0;
  }
}

static int ll_loadlib (lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  const char *init = luaL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))
    return 1;
  else {
    luaL_pushfail(L);
    lua_insert(L, -2);
    lua_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;
  }
}

static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");
  if (f == NULL) return 0;
  fclose(f);
  return 1;
}

static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;
  else if (*name == '\0') {
    *name = *LUA_PATH_SEP;
    name++;
  }
  sep = strchr(name, *LUA_PATH_SEP);
  if (sep == NULL)
    sep = end;
  *sep = '\0';
  *path = sep;
  return name;
}

static void pusherrornotfound (lua_State *L, const char *path) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, "no file '");
  luaL_addgsub(&b, path, LUA_PATH_SEP, "'\n\tno file '");
  luaL_addstring(&b, "'");
  luaL_pushresult(&b);
}

static const char *searchpath (lua_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  luaL_Buffer buff;
  char *pathname;
  char *endpathname;
  const char *filename;

  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = luaL_gsub(L, name, sep, dirsep);
  luaL_buffinit(L, &buff);

  luaL_addgsub(&buff, path, LUA_PATH_MARK, name);
  luaL_addchar(&buff, '\0');
  pathname = luaL_buffaddr(&buff);
  endpathname = pathname + luaL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))
      return lua_pushstring(L, filename);
  }
  luaL_pushresult(&buff);
  pusherrornotfound(L, lua_tostring(L, -1));
  return NULL;
}

static int ll_searchpath (lua_State *L) {
  const char *f = searchpath(L, luaL_checkstring(L, 1),
                                luaL_checkstring(L, 2),
                                luaL_optstring(L, 3, "."),
                                luaL_optstring(L, 4, LUA_DIRSEP));
  if (f != NULL) return 1;
  else {
    luaL_pushfail(L);
    lua_insert(L, -2);
    return 2;
  }
}

static const char *findfile (lua_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  lua_getfield(L, lua_upvalueindex(1), pname);
  path = lua_tostring(L, -1);
  if (l_unlikely(path == NULL))
    luaL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}

static int checkload (lua_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {
    lua_pushstring(L, filename);
    return 2;
  }
  else
    return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          lua_tostring(L, 1), filename, lua_tostring(L, -1));
}

static int searcher_Lua (lua_State *L) {
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  filename = findfile(L, name, "path", LUA_LSUBSEP);
  if (filename == NULL) return 1;
  return checkload(L, (luaL_loadfile(L, filename) == LUA_OK), filename);
}

static int loadfunc (lua_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = luaL_gsub(L, modname, ".", LUA_OFSEP);
  mark = strchr(modname, *LUA_IGMARK);
  if (mark) {
    int stat;
    openfunc = lua_pushlstring(L, modname, mark - modname);
    openfunc = lua_pushfstring(L, LUA_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;
  }
  openfunc = lua_pushfstring(L, LUA_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}

static int searcher_C (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", LUA_CSUBSEP);
  if (filename == NULL) return 1;
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}

static int searcher_Croot (lua_State *L) {
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;
  lua_pushlstring(L, name, p - name);
  filename = findfile(L, lua_tostring(L, -1), "cpath", LUA_CSUBSEP);
  if (filename == NULL) return 1;
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);
    else {
      lua_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  lua_pushstring(L, filename);
  return 2;
}

static int searcher_preload (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  if (lua_getfield(L, -1, name) == LUA_TNIL) {
    lua_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    lua_pushliteral(L, ":preload:");
    return 2;
  }
}

static void findloader (lua_State *L, const char *name) {
  int i;
  luaL_Buffer msg;

  if (l_unlikely(lua_getfield(L, lua_upvalueindex(1), "searchers")
                 != LUA_TTABLE))
    luaL_error(L, "'package.searchers' must be a table");
  luaL_buffinit(L, &msg);

  for (i = 1; ; i++) {
    luaL_addstring(&msg, "\n\t");
    if (l_unlikely(lua_rawgeti(L, 3, i) == LUA_TNIL)) {
      lua_pop(L, 1);
      luaL_buffsub(&msg, 2);
      luaL_pushresult(&msg);
      luaL_error(L, "module '%s' not found:%s", name, lua_tostring(L, -1));
    }
    lua_pushstring(L, name);
    lua_call(L, 1, 2);
    if (lua_isfunction(L, -2))
      return;
    else if (lua_isstring(L, -2)) {
      lua_pop(L, 1);
      luaL_addvalue(&msg);
    }
    else {
      lua_pop(L, 2);
      luaL_buffsub(&msg, 2);
    }
  }
}

static int ll_require (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  lua_settop(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_getfield(L, 2, name);
  if (lua_toboolean(L, -1))
    return 1;

  lua_pop(L, 1);
  findloader(L, name);
  lua_rotate(L, -2, 1);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, -3);

  lua_call(L, 2, 1);

  if (!lua_isnil(L, -1))
    lua_setfield(L, 2, name);
  else
    lua_pop(L, 1);
  if (lua_getfield(L, 2, name) == LUA_TNIL) {
    lua_pushboolean(L, 1);
    lua_copy(L, -1, -2);
    lua_setfield(L, 2, name);
  }
  lua_rotate(L, -2, 1);
  return 2;
}

static const luaL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},

  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};

static const luaL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};

static void createsearcherstable (lua_State *L) {
  static const lua_CFunction searchers[] = {
    searcher_preload,
    searcher_Lua,
    searcher_C,
    searcher_Croot,
    NULL
  };
  int i;

  lua_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);

  for (i=0; searchers[i] != NULL; i++) {
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, searchers[i], 1);
    lua_rawseti(L, -2, i+1);
  }
  lua_setfield(L, -2, "searchers");
}

static void createclibstable (lua_State *L) {
  luaL_getsubtable(L, LUA_REGISTRYINDEX, CLIBS);
  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, gctm);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2);
}

LUAMOD_API int luaopen_package (lua_State *L) {
  createclibstable(L);
  luaL_newlib(L, pk_funcs);
  createsearcherstable(L);

  setpath(L, "path", LUA_PATH_VAR, LUA_PATH_DEFAULT);
  setpath(L, "cpath", LUA_CPATH_VAR, LUA_CPATH_DEFAULT);

  lua_pushliteral(L, LUA_DIRSEP "\n" LUA_PATH_SEP "\n" LUA_PATH_MARK "\n"
                     LUA_EXEC_DIR "\n" LUA_IGMARK "\n");
  lua_setfield(L, -2, "config");

  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_setfield(L, -2, "loaded");

  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  lua_setfield(L, -2, "preload");
  lua_pushglobaltable(L);
  lua_pushvalue(L, -2);
  luaL_setfuncs(L, ll_funcs, 1);
  lua_pop(L, 1);
  return 1;
}
