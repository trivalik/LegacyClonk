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

#include "C4Lua.h"
#include "C4LuaDeletableObjectPtr.h"
#include "C4Include.h"
#include "C4StringTable.h"
#include "C4Value.h"
#include "C4ValueList.h"

#include <functional>

namespace std
{
template<> class hash<luabridge::LuaRef>
{
public:
	size_t operator()(luabridge::LuaRef ref, bool recursive = false) const
	{
		switch (ref.type())
		{
		case LUA_TNONE:
		case LUA_TNIL:
			return hash<decltype(NULL)>()(0L);
		case LUA_TNUMBER:
			return hash<int32_t>()(ref.cast<int32_t>());
		case LUA_TSTRING:
			return hash<std::string>()(ref.tostring());
		case LUA_TTABLE:
		{
			if (recursive)
			{
				return hash<decltype(NULL)>()(0L); // FIXME: Invalid key to 'next'
			}
			size_t _hash = 0L;
			hash<string> shash;
			for (const auto &kv : ref.cast<map<string, luabridge::LuaRef>>())
			{
				_hash ^= (shash(kv.first) ^ (*this)(kv.second, true));
			}
			return _hash;
		}
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			return hash<decltype(NULL)>()(0);
		}
	}
};

}

#define LuaNil(x) luabridge::LuaRef(x)

struct C4AulContext;

namespace LuaHelpers
{
template<typename... Args> luabridge::LuaRef error(lua_State *L, const char *error, Args... args)
{
	luaL_error(L, error, args...);
	return LuaNil(L);
}

template<class T> T *meta(lua_State *L, T *obj)
{
	assert(L);
	luabridge::push(L, obj);
	int top = lua_gettop(L);

	lua_getmetatable(L, -1);
	int meta = lua_gettop(L);

	lua_getfield(L, meta, "__index");
	int mark = lua_gettop(L);

	lua_getfield(L, meta, "__index_old");
	if (!lua_isnil(L, -1))
	{
		lua_setfield(L, meta, "__index");
		lua_settop(L, mark);
		lua_setfield(L, meta, "__index_old");

		lua_settop(L, meta);
		lua_setmetatable(L, top);
	}
	lua_settop(L, top - 1);
	return obj;
}

template<class T, typename Ptr = DeletableObjectPtr<T>, typename Ret = luabridge::RefCountedObjectPtr<Ptr>>
inline Ret ref(lua_State *L, T *obj)
{
	obj->wrapper->setState(L);
	return Ret(meta(L, obj->wrapper));
}

C4Object *Number2Object(int number);
C4ID GetIDFromDef(luabridge::LuaRef def);
int32_t GetPlayerNumber(DeletableObjectPtr<C4Player> *player);
}

namespace LuaScriptFn
{
luabridge::LuaRef RegisterDefinition(luabridge::LuaRef table);
}

namespace luabridge
{
template<> struct Stack<C4Value>
{
	static void push(lua_State *L, C4Value value)
	{
		switch (value.GetType())
		{
		case C4V_Any:
			lua_pushnil(L);
			break;

		case C4V_Int:
		case C4V_C4ID:
			lua_pushnumber(L, value.getIntOrID());
			break;

		case C4V_Bool:
			lua_pushboolean(L, value.getBool());
			break;

		case C4V_String:
			lua_pushstring(L, value.getStr()->Data.getData());
			break;

		case C4V_Array:
		{
			LuaRef table = newTable(L);
			C4ValueArray *array = value.getArray();
			for (int32_t i = 0; i < array->GetSize(); ++i)
			{
				table[i + 1] = (*array)[i];
			}
			luabridge::push(L, table);
		}
			break;

		case C4V_pC4Value:
			push(L, value.GetRef());
			break;

		case C4V_C4ObjectEnum:
			luabridge::push(L, LuaHelpers::Number2Object(value.getInt()));
			break;

		case C4V_C4Object:
			luabridge::push(L, value.getObj());
		}
	}

	static C4Value get(lua_State *L, int index)
	{
		LuaRef value(L, index);
		switch (value.type())
		{
		case LUA_TNONE:
		case LUA_TNIL:
			return C4Value();
		case LUA_TNUMBER:
			return C4VInt(value.cast<int32_t>());
		case LUA_TSTRING:
			return C4VString(value.tostring().c_str());
		case LUA_TTABLE:
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
		default:
			return C4Value();
		}
	}
};

/*template<> struct Stack<StdStrBuf>
{
	static void push(lua_State *L, const StdStrBuf &buf)
	{
		lua_pushstring(L, buf.getData());
	}

	static StdStrBuf get(lua_State *L, int index)
	{
		StdStrBuf buf;
		buf.Copy(lua_tostring(L, index));
		return buf;
	}
};*/
#ifdef USE_FIXED
template<> struct Stack<FIXED>
{
	static void push(lua_State *L, const FIXED &fixed)
	{
		static_assert(std::is_floating_point<lua_Number>::value, "lua_Number is not a floating-point type");
		lua_pushnumber(L, static_cast<lua_Number>(fixtof(fixed)));
	}

	static FIXED get(lua_State *L, int index)
	{
		static_assert(std::is_floating_point<lua_Number>::value, "lua_Number is not a floating-point type");
		return ftofix(static_cast<float>(lua_tonumber(L, index)));
	}
};
#endif
}

class C4LuaScriptEngine : public C4Lua
{
public:
	C4LuaScriptEngine() = default;
public:
	bool Init();
	void Clear();
	template<typename... Args> luabridge::LuaRef Call(luabridge::LuaRef context, std::string functionName, Args... args)
	{
		assert(L);
		bool noThrow = functionName[0] == '~';
		if (noThrow)
		{
			functionName = functionName.substr(1);
		}

		luabridge::LuaRef function(L);
		if (context.isNil())
		{
			function = luabridge::getGlobal(L, functionName.c_str());
		}
		else
		{
			assert(context.isTable() || context.isUserdata());
			function = context[functionName];
		}

		if (!function.isFunction())
		{
			if (!noThrow)
			{
				context.push();
				function.push();
				LogErrorF(lua_pushstring(L, FormatString("Function %s.%s not found",
						  (context.isNil() ? "Global" : luaL_tolstring(L, -2, nullptr)), luaL_tolstring(L, -1, nullptr)).getData()));
				throw luabridge::LuaException(L, LUA_ERRRUN);
			}
			return LuaNil(L);
		}
		else
		{
			try
			{
				return Call(function, args...);
			}
			catch (luabridge::LuaException const &e)
			{
				LogErrorF("%s", e.what());
				throw;
			}
		}
	}

	template<typename... Args> luabridge::LuaRef Call(const std::string &context, const std::string &functionName, Args... args)
	{
		assert(L);
		luabridge::LuaRef ref = luabridge::getGlobal(L, context.c_str());
		if (ref.isTable())
		{
			return Call(ref, functionName, args...);
		}
		else
		{
			if (functionName[0] != '~')
			{
				LogErrorF(lua_pushstring(L, FormatString("Table %s not found", context.c_str()).getData()));
				throw luabridge::LuaException(L, LUA_ERRRUN);
			}
			return LuaNil(L);
		}
	}

	template<typename... Args> luabridge::LuaRef Call(luabridge::LuaRef function, Args... args)
	{
		assert(L);
		assert(function.isFunction());
		try
		{
			return function(args...);
		}
		catch (luabridge::LuaException const &e)
		{
			LogErrorF("%s", e.what());
			throw;
		}
	}
	luabridge::LuaRef Evaluate(const std::string &script);
	bool Load(C4Group &group, const char *filename, const char *language, class C4LangStringTable *localTable, bool loadTable = false);
	void Link(C4DefList *defs);

private:
	void LogErrorF(const char *error, ...);

private:
	static constexpr const size_t BUFSIZE = 4096;
	size_t lines = 0;
	size_t warnings = 0;
	size_t errors = 0;
};
