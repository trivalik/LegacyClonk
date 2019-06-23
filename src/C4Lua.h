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

#pragma once

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "LuaBridge/Map.h"
#include "LuaBridge/Vector.h"

class C4Object;

class C4Lua
{
public:
	C4Lua() = default;
	~C4Lua();
	bool Init();
	void Clear();
	lua_State *state() { return L; }
protected:
	lua_State *L = nullptr;
};
