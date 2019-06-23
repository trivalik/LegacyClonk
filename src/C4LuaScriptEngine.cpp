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

#include "C4LuaScriptEngine.h"
#ifndef BIG_C4INCLUDE
#include "C4Components.h"
#include "C4Game.h"
#include "C4Log.h"
#include "C4Object.h"
#include "C4Script.h"
#endif

#include <cstring>


namespace LuaScriptFn
{

extern "C"
{
// https://gist.github.com/5at/3671566
int print(lua_State *L)
{
	StdStrBuf buf;
	int nargs = lua_gettop(L);
	for (int i = 1; i <= nargs; ++i)
	{
		buf.AppendFormat("%s", lua_tostring(L, i));
		if (i > 0)
		{
			buf.AppendChar(' ');
		}
	}
	Log(buf.getData());
	return 0;
}

int dofile(lua_State *L)
{
	lua_settop(L, 0);
	return luaL_error(L, "dofile is disabled due to security reasons");
}

int loadfile(lua_State *L)
{
	lua_settop(L, 0);
	return luaL_error(L, "loadfile is disabled due to security reasons");
}

}

int createObject(lua_State *L)
{
	Log(lua_typename(L, lua_type(L, 1)));
	luaL_checktype(L, 1, LUA_TTABLE);
	auto table = luabridge::LuaRef::fromStack(L, 1);
	if (!table.isTable())
	{
		return luaL_error(L, FormatString("Definition is not a table (type: %s)", lua_typename(table.state(), table.type())).getData());
	}
	else if (!table["Name"].isString())
	{
		return luaL_error(L, "Definition has no name");
	}
	int32_t x = static_cast<int32_t>(luaL_checkinteger(L, 2));
	int32_t y = static_cast<int32_t>(luaL_checkinteger(L, 3));
	/*luabridge::LuaRef owner(L);
	if (lua_gettop(L) == 4)
	{
		owner = luabridge::LuaRef::fromStack(L, 4);
	}*/
	luabridge::LuaRef owner(L, -1);

	C4ID id;
	if (table["ID"].isNumber())
	{
		id = table["ID"];
	}
	else if (table["ID"].isString())
	{
		std::string i = table["ID"];
		if (LooksLikeID(i.c_str()))
		{
			id = C4Id(i.c_str());
		}
		else
		{
			return luaL_error(L, "Definition has invalid ID: %s", i.c_str());
		}
	}
	else
	{
		id = std::hash<luabridge::LuaRef>()(table);
	}
	C4Def *def = Game.Defs.ID2Def(id);
	C4Object *obj = nullptr;
	if (def != nullptr)
	{
		if (def->LuaDef.isNil())
		{
			return luaL_error(table.state(), FormatString("Internal error: A definition with the generated ID %ld (%s) already exists.",
									   id, C4IdText(id)).getData());
		}
		//FIXME: controller, creator, other stuff
		obj = Game.CreateObject(id, nullptr, owner.cast<int32_t>(), x, y);
	}
	else
	{
		def = new C4Def;
		if (!def->Compile(table, id))
		{
			delete def;
			return luaL_error(table.state(), "Definition error: See previous errors for details");
		}

		def->id = id;
		Game.Defs.Add(def, false);
		assert(Game.Defs.ID2Def(id));
	}

	//FIXME: controller, creator, other stuff
	obj = Game.CreateObject(id, nullptr, owner.cast<int32_t>(), x, y);

	luabridge::push(L, obj);
	int top = lua_gettop(L);
	lua_getmetatable(L, -1);
	int meta = lua_gettop(L);

	lua_getfield(L, meta, "__index");
	int mark = lua_gettop(L);

	lua_getfield(L, meta, "__index_old");
	lua_setfield(L, meta, "__index");
	lua_settop(L, mark);
	lua_setfield(L, meta, "__index_old");
	lua_settop(L, top);
	return 1;
}

};

bool C4LuaScriptEngine::Init()
{
	if (!C4Lua::Init())
	{
		return false;
	}

	luabridge::getGlobalNamespace(L)
		.addFunction("print", &LuaScriptFn::print)
		.addFunction("dofile", &LuaScriptFn::dofile)
		.addFunction("loadfile", &LuaScriptFn::dofile)
		.beginNamespace("Game")
			.beginNamespace("Category")
			.endNamespace()
			.addCFunction("CreateObject", &LuaScriptFn::createObject)
		.endNamespace()

		.beginNamespace("ObjectStatus")
			.addProperty("Normal",   const_cast<int32_t *>(&C4OS_NORMAL),   false)
			.addProperty("Inactive", const_cast<int32_t *>(&C4OS_INACTIVE), false)
			.addProperty("Deleted",  const_cast<int32_t *>(&C4OS_DELETED),  false)
		.endNamespace()

		.beginNamespace("Direction")
			.addProperty("None",  const_cast<int32_t *>(&DIR_None),  false)
			.addProperty("Left",  const_cast<int32_t *>(&DIR_Left),  false)
			.addProperty("Right", const_cast<int32_t *>(&DIR_Right), false)
		.endNamespace()

		.beginNamespace("ComDir")
			.addProperty("None",      const_cast<int32_t *>(&COMD_None),      false)
			.addProperty("Stop",      const_cast<int32_t *>(&COMD_Stop),      false)
			.addProperty("Up",        const_cast<int32_t *>(&COMD_Up),        false)
			.addProperty("UpRight",   const_cast<int32_t *>(&COMD_UpRight),   false)
			.addProperty("Right",     const_cast<int32_t *>(&COMD_Right),     false)
			.addProperty("DownRight", const_cast<int32_t *>(&COMD_DownRight), false)
			.addProperty("Down",      const_cast<int32_t *>(&COMD_Down),      false)
			.addProperty("DownLeft",  const_cast<int32_t *>(&COMD_DownLeft),  false)
			.addProperty("Left",      const_cast<int32_t *>(&COMD_Left),      false)
			.addProperty("UpLeft",    const_cast<int32_t *>(&COMD_UpLeft),    false)
		.endNamespace()

		.beginNamespace("Visibility")
			.addProperty("All",         const_cast<int32_t *>(&VIS_All),         false)
			.addProperty("None",        const_cast<int32_t *>(&VIS_None),        false)
			.addProperty("Owner",       const_cast<int32_t *>(&VIS_Owner),       false)
			.addProperty("Allies",      const_cast<int32_t *>(&VIS_Allies),      false)
			.addProperty("Enemies",     const_cast<int32_t *>(&VIS_Enemies),     false)
			.addProperty("Local",       const_cast<int32_t *>(&VIS_Local),       false)
			.addProperty("God",         const_cast<int32_t *>(&VIS_God),         false)
			.addProperty("LayerToggle", const_cast<int32_t *>(&VIS_LayerToggle), false)
			.addProperty("OverlayOnly", const_cast<int32_t *>(&VIS_OverlayOnly), false)
		.endNamespace()

		//classes
		.beginClass<C4Action>("C4Action")
			.addProperty("Name", &C4Action::Name)
			.addProperty("Direction", &C4Action::Dir)
			.addProperty("ComDir", &C4Action::ComDir)
			.addProperty("Target", &C4Action::Target)
			.addProperty("Target2", &C4Action::Target2)
			.addProperty("Phase", &C4Action::Phase)
			.addProperty("PhaseDelay", &C4Action::PhaseDelay)
		.endClass()

		.beginClass<C4Object>("C4Object")
			.addFunction("__newindex", &C4Object::__newindex)
			.addFunction("__index_old", &C4Object::__index)
		.endClass();

	return true;
}


luabridge::LuaRef C4LuaScriptEngine::Evaluate(const std::string &script)
{
	assert(L);
	switch (luaL_loadbufferx(L, script.c_str(), script.size(), "Evaluate", "t"))
	{
	case LUA_ERRSYNTAX:
		throw luabridge::LuaException(L, LUA_ERRSYNTAX);
	case LUA_ERRMEM:
		LogFatal("Out of memory");
		return luabridge::LuaRef(L);
	default:
		break;
	}
	luabridge::LuaException::pcall(L, 0, 1, 0);
	return luabridge::LuaRef::fromStack(L);
}

bool C4LuaScriptEngine::Load(C4Group &group, const char *filename, const char *language, C4LangStringTable *localTable, bool loadTable)
{
	assert(L);
	// load it if specified
	if (localTable && loadTable)
	{
		localTable->LoadEx("StringTbl", group, C4CFN_ScriptStringTbl, language);
	}

	StdStrBuf buf;
	if (!group.LoadEntryString(filename, buf))
	{
		return false;
	}
#if 0
	if (strncmp(buf.getData(), LUA_SIGNATURE, strlen(LUA_SIGNATURE)) == 0)
	{
		LogF("Error while loading %s: Loading Lua bytecode is forbidden", filename);
	}
#endif

	if (localTable)
	{
		localTable->ReplaceStrings(buf);
	}
	switch (luaL_loadbufferx(L, buf.getData(), buf.getSize() - 1, filename, "t"))
	{
	case LUA_ERRSYNTAX:
		LogF("ERROR: Syntax error: %s", lua_tostring(L, -1));
		++errors;
		return false;
	case LUA_ERRMEM:
		LogFatal("Out of memory");
		++errors;
		return false;
	default:
		break;
	}

	switch (lua_pcall(L, 0, 0, 0))
	{
	case LUA_ERRRUN:
		LogF("ERROR: %s", lua_tostring(L, -1));
		++errors;
		return false;
	case LUA_ERRMEM:
		LogFatal("Out of memory");
		++errors;
		return false;
	default:
		return true;
	}
}

void C4LuaScriptEngine::Link(C4DefList *defs)
{
	(void) defs;
	assert(L);
	LogF("C4LuaScriptEngine linked - %zu warnings, %zu errors", warnings, errors);
}
