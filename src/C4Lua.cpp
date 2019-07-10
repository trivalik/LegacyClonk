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

C4Lua::~C4Lua()
{
	Clear();
}

bool C4Lua::Init()
{
	Clear();
	L = luaL_newstate();
	if (!L)
	{
		return false;
	}
	luaopen_base(L);
	luaopen_bit32(L);
	luaopen_coroutine(L);
	luaopen_math(L);
	luaopen_string(L);
	luaopen_table(L);
	return true;
}

void C4Lua::Clear()
{
	if (L)
	{
		lua_settop(L, 0);
		lua_close(L);
		L = nullptr;
	}
}
