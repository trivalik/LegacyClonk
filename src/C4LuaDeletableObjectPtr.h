/*
 * LegacyClonk
 *
 * Copyright (c) 1998-2000, Matthes Bender (RedWolf Design)
 * Copyright (c) 2019, The LegacyClonk Team and contributors
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
#include "LuaBridge/RefCountedObject.h"
#include "LuaBridge/RefCountedPtr.h"

namespace LuaHelpers
{
template<class T> class DeletableObjectPtr : public luabridge::RefCountedObject
{
public:
	DeletableObjectPtr(lua_State *L = nullptr, T * object = nullptr)
	{
		set(L, object);
	}
	~DeletableObjectPtr() = default;

public:
	T *get() const { return object; }
	lua_State *state() const { return m_L; }
	void set(lua_State *L, T* object) { m_L = L; this->object = object; }
	void setState(lua_State *L) { m_L = L; }
	void reset() { object = nullptr; }

	T *checkObject() const
	{
		if (object )
		{
			return object;
		}
		if (m_L)
		{
			luaL_error(m_L, "Object call: Target is zero!");
		}
		return nullptr;
	}

	operator T*() const { return checkObject(); }
	T *operator->() const { return checkObject(); }

	bool operator==(const T* &t)
	{
		return t == object;
	}
	bool operator==(const std::nullptr_t &)
	{
		return object == nullptr;
	}

protected:
	T* object = nullptr;
	lua_State *m_L;
};
}
