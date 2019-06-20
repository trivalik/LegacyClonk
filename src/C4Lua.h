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

using namespace luabridge;

class C4Object;

class C4Lua
{
public:
	C4Lua() = default;
	~C4Lua();
	bool Init();
	lua_State *state() { return L; }
protected:
	lua_State *L = nullptr;
};

class C4LuaScriptEngine : public C4Lua
{
public:
	C4LuaScriptEngine() = default;
public:
	bool Init();
	template<typename... Args> LuaRef Call(LuaRef context, const std::string &functionName, Args... args)
	{
		assert(context.isTable() || context.isUserdata());
		if (!context[functionName].isNil())
		{
			return context[functionName](args...);
		}
	}

	template<typename... Args> LuaRef Call(LuaRef function, Args... args)
	{
		assert(function.isFunction());
		return function(args...);
	}
};
