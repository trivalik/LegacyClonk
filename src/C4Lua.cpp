/*
 * LegacyClonk
 *
 * Copyright (c) 1998-2000, Matthes Bender (RedWolf Design)
 * Copyright (c) 2017-2019, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

#include "C4Lua.h"

bool C4Lua::Init()
{
	if (L != nullptr)
	{
		lua_close(L);
	}
	L = luaL_newstate();
	if (L == nullptr)
	{
		return false;
	}
	luaL_openlibs(L);
	return true;
}

C4Lua::~C4Lua()
{
	if (L != nullptr)
	{
		lua_settop(L, 0);
		lua_close(L);
		L = nullptr;
	}
}

bool C4LuaScriptEngine::Init()
{
	if (!C4Lua::Init())
	{
		return false;
	}
}
