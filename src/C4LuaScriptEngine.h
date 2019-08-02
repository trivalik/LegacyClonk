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

#include "C4Aul.h"
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
		size_t operator()(luabridge::LuaRef ref, bool recursive = false) const;
	};
}

namespace luabridge
{
template<> struct Stack<C4Value>
{
	static void push(lua_State *L, C4Value value);
	static C4Value get(lua_State *L, int index);
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
	static void push(lua_State *L, const FIXED &fixed);
	static FIXED get(lua_State *L, int index);
};

#endif
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

template<typename T> std::optional<T> cast(luabridge::LuaRef ref)
{
	struct Pcall
	{
		luabridge::LuaRef ref;
		T result;
		static int run(lua_State *L)
		{
			auto *p = static_cast<Pcall *>(lua_touserdata(L, 1));
			p->result = p->ref.template cast<T>();
			return 0;
		}
	};

	Pcall p{ref};
	lua_pushcfunction(ref.state(), &Pcall::run);
	lua_pushlightuserdata(ref.state(), &p);

	int result = lua_pcall(ref.state(), 1, 0, 0);
	if (result != LUA_OK)
	{
		lua_pop(ref.state(), 1);
		return {};
	}

	return {p.result};
}

C4Object *Number2Object(int number);
C4ID GetIDFromDef(luabridge::LuaRef def);
int32_t GetPlayerNumber(DeletableObjectPtr<C4Player> *player);
void PushObject(lua_State *L, C4Object *obj);
C4Value HandleUserdata(lua_State *L, int32_t index);
}

namespace LuaScriptFn
{
luabridge::LuaRef RegisterDefinition(luabridge::LuaRef table);
}

class C4LuaScriptEngine : public C4Lua
{
public:
	C4LuaScriptEngine() = default;
public:
	bool Init();
	void Clear();
	enum class CallFlags
	{
		None = 0,
		Log,
		Throw,
		ThrowC4Aul
	};

	template<CallFlags Flags = CallFlags::Throw, typename... Args> luabridge::LuaRef Call(luabridge::LuaRef context, std::string functionName, Args... args)
	{
		assert(L);
		bool noThrow = functionName[0] == '~';
		if (noThrow)
		{
			return Call<CallFlags::None>(context, functionName.substr(1), args...);
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
			if constexpr (Flags != CallFlags::None)
			{
				context.push();
				StdStrBuf s = FormatString("Function %s.%s not found",
						  (context.isNil() ? "Global" : luaL_tolstring(L, -2, nullptr)), functionName.c_str());

				switch (Flags)
				{
				case CallFlags::Log:
					LogErrorF(s.getData());
					break;

				case CallFlags::Throw:
					lua_pushstring(L, s.getData());
					throw luabridge::LuaException(L, LUA_ERRRUN);

				case CallFlags::ThrowC4Aul:
					throw new C4AulExecError(nullptr, s.getData());
				}
			}
			return LuaNil(L);
		}
		else
		{
			try
			{
				return function(args...);
			}
			catch (luabridge::LuaException const &e)
			{
				switch (Flags)
				{
				case CallFlags::Throw:
					throw;
				case CallFlags::ThrowC4Aul:
					throw new class C4AulExecError(nullptr, e.what());
				case CallFlags::Log:
					LogErrorF("%s", e.what());
				default:
					return LuaNil(L);
				}
			}
		}
	}

	template<CallFlags Flags = CallFlags::Throw, typename... Args> luabridge::LuaRef Call(const std::string &context, const std::string &functionName, Args... args)
	{
		assert(L);
		luabridge::LuaRef ref = luabridge::getGlobal(L, context.c_str());
		if (ref.isTable())
		{
			return Call<Flags>(ref, functionName, args...);
		}
		else
		{
			if (functionName[0] != '~')
			{
				if constexpr (Flags != CallFlags::None)
				{
					StdStrBuf s = FormatString("Table %s not found", context.c_str());

					switch (Flags)
					{
					case CallFlags::Log:
						LogErrorF(s.getData());
						break;

					case CallFlags::Throw:
						lua_pushstring(L, s.getData());
						throw luabridge::LuaException(L, LUA_ERRRUN);

					case CallFlags::ThrowC4Aul:
						throw new C4AulExecError(nullptr, s.getData());
					}
				}
			}
			return LuaNil(L);
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
	std::vector<std::string> FunctionNames;
	friend class C4AulParseState;
	friend class C4AulExec;
	friend class C4AulScript;
};
