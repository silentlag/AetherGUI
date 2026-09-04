
#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"

#define LUAC_DATA	"\x19\x93\r\n\x1a\n"

#define LUAC_INT	0x5678
#define LUAC_NUM	cast_num(370.5)

#define LUAC_VERSION  (((LUA_VERSION_NUM / 100) * 16) + LUA_VERSION_NUM % 100)

#define LUAC_FORMAT	0

LUAI_FUNC LClosure* luaU_undump (lua_State* L, ZIO* Z, const char* name);

LUAI_FUNC int luaU_dump (lua_State* L, const Proto* f, lua_Writer w,
                         void* data, int strip);

#endif
