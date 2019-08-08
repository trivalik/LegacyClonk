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
#include "C4Command.h"
#include "C4Components.h"
#include "C4Def.h"
#include "C4Game.h"
#include "C4InfoCore.h"
#include "C4Log.h"
#include "C4Object.h"
#include "C4ObjectCom.h"
#include "C4ObjectInfo.h"
#include "C4Material.h"
#include "C4Player.h"
#include "C4Random.h"
#include "C4Script.h"
#include "C4Wrappers.h"
#endif

#include <numeric>
#include <sstream>
#include <cstring>

size_t std::hash<luabridge::LuaRef>::operator()(luabridge::LuaRef ref, bool recursive) const
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

namespace luabridge
{

void Stack<C4Value>::push(lua_State *L, C4Value value)
{
	switch (value.GetType())
	{
	case C4V_Any:
		//lua_pushnil(L);
		lua_pushinteger(L, 0);
		break;

	case C4V_Int:
		lua_pushinteger(L, value.getIntOrID());
		break;

	case C4V_C4ID:
	{
		C4Def *def = Game.Defs.ID2Def(static_cast<C4ID>(value.getIntOrID()));
		if (def)
		{
			luabridge::push(L, LuaHelpers::ref(L, def));
		}
		else
		{
			lua_pushnil(L);
		}
		break;
	}

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
		LuaHelpers::PushObject(L, LuaHelpers::Number2Object(value.getInt()));
		break;

	case C4V_C4Object:
		LuaHelpers::PushObject(L, value.getObj());
	}
}

C4Value Stack<C4Value>::get(lua_State *L, int index)
{
	switch (lua_type(L, index))
	{
	case LUA_TNONE:
		assert(false);
		return C4VNull;
	case LUA_TNIL:
		return C4VNull;
	case LUA_TNUMBER:
		return C4VInt(static_cast<int32_t>(lua_tointeger(L, index)));
	case LUA_TSTRING:
		return C4VString(lua_tostring(L, index));

	case LUA_TUSERDATA:
	case LUA_TLIGHTUSERDATA:
		return LuaHelpers::HandleUserdata(L, index);

	case LUA_TTABLE:
	case LUA_TFUNCTION:
	case LUA_TTHREAD:
	default:
		return C4VNull;
	}
}

#ifdef USE_FIXED

void Stack<FIXED>::push(lua_State *L, const FIXED &fixed)
{
	static_assert(std::is_floating_point<lua_Number>::value, "lua_Number is not a floating-point type");
	lua_pushnumber(L, static_cast<lua_Number>(fixtof(fixed)));
}

FIXED Stack<FIXED>::get(lua_State *L, int index)
{
	static_assert(std::is_floating_point<lua_Number>::value, "lua_Number is not a floating-point type");
	return ftofix(static_cast<float>(lua_tonumber(L, index)));
}

#endif
}

namespace LuaHelpers
{
C4Object *Number2Object(int number)
{
	return Game.Objects.ObjectPointer(number);
}

int32_t GetPlayerNumber(DeletableObjectPtr<C4Player> *player)
{
	return player ? (*player)->Number : NO_OWNER;
}

C4ID GetIDFromDef(luabridge::LuaRef def)
{
	C4ID id;
	if (def.isNumber())
	{
		id = def;
	}
	else if (def.isString())
	{
		std::string i = def;
		if (LooksLikeID(i.c_str()))
		{
			id = C4Id(i.c_str());
		}
		else
		{
			luaL_error(def.state(), "Definition has invalid ID: %s", i.c_str());
			return 0L;
		}
	}
	else if (def.isTable())
	{
		id = GetIDFromDef(def["ID"]);
	}
	else
	{
		id = std::hash<luabridge::LuaRef>()(def);
	}
	return id;
}

void PushObject(lua_State *L, C4Object *obj)
{
	if (obj && obj->wrapper)
	{
		luabridge::push(L, ref(L, obj));
	}
	else
	{
		lua_pushnil(L);
	}
}

C4Value HandleUserdata(lua_State *L, int32_t index)
{
#define IS(name) luaL_testudata(L, index, #name)
#define TO(name) luabridge::LuaRef::fromStack(L, index).cast<name *>()
	if (IS(C4Action))
	{
		return C4VString(TO(C4Action)->Name.c_str());
	}
	else if (IS(C4AulFunc))
	{
		return C4VString(TO(DeletableObjectPtr<C4AulFunc>)->checkObject()->Name);
	}
	else if (IS(C4Def))
	{
		return C4VID(TO(DeletableObjectPtr<C4Def>)->checkObject()->id);
	}
	else if (IS(C4Material))
	{
		return C4VInt(Game.Material.Get(TO(C4Material)->Name.c_str()));
	}
	else if (IS(C4Object))
	{
		return C4VObj(TO(DeletableObjectPtr<C4Object>)->checkObject());
	}
	else if (IS(C4Player))
	{
		C4Player *player = TO(DeletableObjectPtr<C4Player>)->checkObject();
		return C4VInt(player ? player->Number : NO_OWNER);
	}
	else
	{
		return C4VNull;
	}
}

bool optboolean(lua_State *L, int index, bool defaultValue)
{
	return lua_gettop(L) >= index ? lua_toboolean(L, index) : defaultValue;
}

template<typename Ret, typename... Args> Ret CallC4Script(DeletableObjectPtr<C4Object> *obj, Ret (*function)(C4AulContext *, Args...), Args... args)
{
	C4AulContext context{obj->checkObject(), obj->checkObject()->Def, nullptr};
	return function(&context, args...);
}

template<typename... Args> C4Value CallC4Script(DeletableObjectPtr<C4Object> *obj, C4Value (*function)(C4AulContext *, C4Value *), Args... args)
{
	C4AulContext context{obj->checkObject(), obj->checkObject()->Def, nullptr};
	C4AulParSet pars(args...);
	return function(&context, pars.Par);
}

template<class T> auto *GetRawPointerFromContext(luabridge::LuaRef context)
{
	typedef std::conditional_t<std::is_pointer_v<T>, std::remove_pointer_t<T>, T> Ret;

	LogF("Ret: %s", typeid(Ret).name());

	std::optional<DeletableObjectPtr<Ret> *> opt = cast<DeletableObjectPtr<Ret> *>(context);
	if (opt && *opt)
	{
		return (*opt)->checkObject();
	}
	return static_cast<Ret *>(nullptr);
}
}

extern C4Value FnDeathAnnounce(C4AulContext *, C4Value *);
extern C4Value FnCollect(C4AulContext *, C4Value *);
extern C4Value FnSplit2Components(C4AulContext *, C4Value *);
extern bool    FnSetPosition(C4AulContext *, long, long, C4Object *, bool);
extern bool    FnDoCon(C4AulContext *, long, C4Object *);
extern bool    FnDoEnergy(C4AulContext *, long, C4Object *, bool, long, long);
extern bool    FnDoDamage(C4AulContext *, long, C4Object *, long, long);
extern bool    FnDoMagicEnergy(C4AulContext *, long, C4Object *, bool);
extern bool    FnSetPhysical(C4AulContext *, C4String *, long, long, C4Object *);
extern bool    FnTrainPhysical(C4AulContext *, C4String *, long, long, C4Object *);
extern bool    FnResetPhysical(C4AulContext *, C4Object *, C4String *);
extern long    FnGetPhysical(C4AulContext *, C4String *, long, C4Object *, C4ID);
extern C4Value FnSetCommand(C4AulContext *, C4Value *);
extern C4Value FnAddCommand(C4AulContext *, C4Value *);
extern C4Value FnAppendCommand(C4AulContext *, C4Value *);
extern bool    FnFinishCommand(C4AulContext *, C4Object *, bool, long);
extern bool    FnSetName(C4AulContext *, C4String *, C4Object *, C4ID, bool, bool);

namespace LuaScriptFn
{

#define PTR(x) typedef LuaHelpers::DeletableObjectPtr<x> x##Ptr;
PTR(C4AulFunc)
PTR(C4Def)
PTR(C4Object)
PTR(C4PlayerInfoCore)
PTR(C4Player)
#undef PTR

// https://gist.github.com/5at/3671566
int print(lua_State *L)
{
	StdStrBuf buf;
	int nargs = lua_gettop(L);
	for (int i = 1; i <= nargs; ++i)
	{
		switch (lua_type(L, i))
		{
		case LUA_TNONE:
		case LUA_TNIL:
			buf.Append("nil");
			break;
		case LUA_TNUMBER:
			buf.AppendFormat("%d", static_cast<int32_t>(lua_tointeger(L, i)));
			break;
		case LUA_TBOOLEAN:
			buf.AppendFormat("%s", lua_toboolean(L, i) ? "true" : "false");
			break;
		case LUA_TSTRING:
			buf.AppendFormat("%s", lua_tostring(L, i));
			break;
		case LUA_TTABLE:
			buf.Append("(table)");
			break;
		case LUA_TFUNCTION:
			buf.Append("(function");
			break;
		case LUA_TUSERDATA:
		case LUA_TLIGHTUSERDATA:
			buf.AppendFormat("%s", lua_tolstring(L, i, nullptr));
			break;
		case LUA_TTHREAD:
			buf.Append("(thread)");
			break;
		default:
			buf.AppendFormat("%s", lua_tolstring(L, i, nullptr));
			break;
		}
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

luabridge::LuaRef RegisterDefinition(luabridge::LuaRef context, luabridge::LuaRef table)
{
	(void) context;
	C4ID id = LuaHelpers::GetIDFromDef(table);
	C4Def *def = Game.Defs.ID2Def(id);
	if (def)
	{
		if (def->LuaDef != table)
		{
			return LuaHelpers::error(table.state(), FormatString("Internal error: Definition with the generated ID %ld (%s) already exists",
									   id, C4IdText(id)).getData());
		}
	}
	else
	{
		def = new C4Def;
		if (!def->Compile(table, id))
		{
			delete def;
			return LuaHelpers::error(table.state(), "Definition error: See previous errors for details");
		}

		def->id = id;
		Game.Defs.Add(def, false);
		if (!Game.Defs.ID2Def(id))
		{
			return LuaHelpers::error(table.state(), FormatString("Internal error: Cannot add definition to definition list").getData());
		}
	}
	table["ID"] = id;
	return table;
}

luabridge::LuaRef CreateObject(luabridge::LuaRef context, luabridge::LuaRef arguments, lua_State *L)
{
	luabridge::LuaRef table = arguments["Def"];
	C4ID id = LuaHelpers::GetIDFromDef(table);
	if (table.isTable())
	{
		if (!table["Name"].isString() || table["Name"].tostring().empty())
		{
			return LuaHelpers::error(L, "Definition has no name");
		}

		table = RegisterDefinition(context, table);
	}

	int32_t x = arguments["X"].isNumber() ? arguments["X"] : 0;
	int32_t y = arguments["Y"].isNumber() ? arguments["Y"] : 0;
	int32_t r = arguments["R"].isNumber() ? arguments["R"] : 0;

	FIXED xdir = arguments["XDir"].isNumber() ? ftofix(arguments["XDir"].cast<float>()) : Fix0;
	FIXED ydir = arguments["YDir"].isNumber() ? ftofix(arguments["YDir"].cast<float>()) : Fix0;
	FIXED rdir = arguments["RDir"].isNumber() ? ftofix(arguments["RDir"].cast<float>()) : Fix0;

	int32_t con = arguments["Con"].isNumber() ? arguments["Con"] : 100;

	C4PlayerPtr *owner = !arguments["Owner"].isNil() ? arguments["Owner"] : nullptr;
	C4PlayerPtr *controller = !arguments["Controller"].isNil() ? arguments["Controller"] : owner;

	//auto *objectinfo = !arguments["ObjectInfo"].isNil() ? arguments["ObjectInfo"].cast<C4ObjectInfoPtr *>() : nullptr;

	C4ObjectPtr *creator = !arguments["Creator"].isNil() ? arguments["Creator"] : nullptr;

	C4Object *obj = Game.NewObject(
				Game.Defs.ID2Def(id),
				creator ? *creator : nullptr,
				owner ? (*owner)->Number : NO_OWNER,
				nullptr,
				x, y, r,
				xdir, ydir, rdir,
				FullCon * con / 100,
				controller  ? (*controller)->Number : NO_OWNER
				);

	return obj ? luabridge::LuaRef(L, LuaHelpers::ref(L, obj)) : LuaNil(L);
}

void Explode(C4ObjectPtr *obj, int32_t level, lua_State *L) // opt: luabridge::LuaRef effect, std::string particle
{
	if (!obj) return;
	C4ID id = 0L;
	auto effect = luabridge::LuaRef::fromStack(L, 3);
	if (effect.isTable())
	{
		RegisterDefinition(luabridge::LuaRef(L, LuaHelpers::ref(L, obj->checkObject())), effect);
		id = effect["ID"];
	}
	else if (effect.isNumber())
	{
		id = effect;
	}
	(*obj)->Explode(level, id, luaL_optlstring(L, 4, "", nullptr));
}

bool Incinerate(C4ObjectPtr *obj, lua_State *L) // opt: C4Player *player
{
	if (!obj) return false;
	return (*obj)->Incinerate(lua_gettop(L) >= 2 ?
							   LuaHelpers::GetPlayerNumber(
								   luabridge::LuaRef::fromStack(L, 2).cast<C4PlayerPtr *>()
									  )
							 : NO_OWNER);
}

bool IncinerateLandscape(luabridge::LuaRef context, int32_t x, int32_t y)
{
	(void) context;
	return Game.Landscape.Incinerate(x, y);
}

bool Extinguish(C4ObjectPtr *obj)
{
	if (!obj) return false;
	return (*obj)->Extinguish(0);
}

float GetGravity()
{
	return fixtof(Game.Landscape.Gravity * 500);
}

void SetGravity(float newGravity)
{
	Game.Landscape.Gravity = ftofix(BoundBy<float>(newGravity, -300, 300)) / 500;
}

void DeathAnnounce(C4ObjectPtr *obj)
{
	LuaHelpers::CallC4Script(obj, &FnDeathAnnounce);
}

void GrabContents(C4ObjectPtr *obj, C4ObjectPtr *target)
{
	if (!obj || !target) return;
	(*obj)->GrabContents(*target);
}

bool Punch(C4ObjectPtr *obj, C4ObjectPtr *target, int32_t strength)
{
	if (!obj || !target) return false;
	return ObjectComPunch(obj->checkObject(), target->checkObject(), strength);
}

bool Kill(C4ObjectPtr *obj, lua_State *L) // opt: bool forced, C4Player *player
{
	if (!obj) return false;
	bool forced = LuaHelpers::optboolean(L, 2, false);
	if (lua_gettop(L) >= 3)
	{
		int32_t player = LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 2).cast<C4PlayerPtr *>());
		if (ValidPlr(player))
		{
			(*obj)->UpdatLastEnergyLossCause(player); // typo?!
		}
	}
	(*obj)->AssignDeath(forced);
	return true;
}

void Fling(C4ObjectPtr *obj, float xdir, float ydir, lua_State *L) // opt: bool addSpeed, C4Player *player
{
	if (!obj) return;
	bool addSpeed = LuaHelpers::optboolean(L, 4, false);

	int32_t player = NO_OWNER;
	if (lua_gettop(L) >= 5)
	{
		player = LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 5).cast<C4PlayerPtr *>());
	}

	(*obj)->Fling(ftofix(xdir), ftofix(ydir), addSpeed, player);
	// unstick from ground, because Fling command may be issued in an Action-callback,
	// where attach-values have already been determined for that frame
	(*obj)->Action.t_attach = 0;
}

bool Jump(C4ObjectPtr *obj)
{
	if (!obj) return false;
	return ObjectComJump(obj->checkObject());
}

bool Enter(C4ObjectPtr *obj, C4ObjectPtr target)
{
	if (!obj || !target) return false;
	return (*obj)->Enter(target);
}

bool Exit(C4ObjectPtr *obj, lua_State *L) // opt: int32_t x, int32_t y, int32_t r, float xdir, float ydir, float rdir
{
	if (!obj) return false;

	int32_t x = static_cast<int32_t>(luaL_optinteger(L, 2, 0));
	int32_t y = static_cast<int32_t>(luaL_optinteger(L, 3, 0));
	int32_t r = static_cast<int32_t>(luaL_optinteger(L, 4, 0));
	float xdir = static_cast<float>(luaL_optnumber(L, 5, 0));
	float ydir = static_cast<float>(luaL_optnumber(L, 6, 0));
	float rdir = static_cast<float>(luaL_optnumber(L, 7, 0));

	if (r == -1)
	{
		r = SafeRandom(360);
	}
	ObjectComCancelAttach(obj->checkObject());
	return (*obj)->Exit(x, y, r, ftofix(xdir), ftofix(ydir), ftofix(rdir));
}

bool Collect(C4ObjectPtr *obj, C4ObjectPtr item)
{
	if (!obj) return false;
	return LuaHelpers::CallC4Script(obj, &FnCollect, C4VObj(item)).getBool();
}

void Split2Components(C4ObjectPtr *obj)
{
	if (!obj) return;
	LuaHelpers::CallC4Script(obj, &FnSplit2Components);
}

void RemoveObject(C4ObjectPtr *obj, lua_State *L) // opt: bool ejectContents
{
	if (!obj) return;
	(*obj)->AssignRemoval(LuaHelpers::optboolean(L, 2, false));
}

void SetPosition(C4ObjectPtr *obj, int32_t x, int32_t y, lua_State *L) // opt: bool checkBounds
{
	if (!obj) return;
	LuaHelpers::CallC4Script(obj, &FnSetPosition,
							 static_cast<long>(x),
							 static_cast<long>(y),
							 static_cast<C4Object *>(nullptr),
							 LuaHelpers::optboolean(L, 4, false)
							 );
}

int32_t GetCon(const C4ObjectPtr *obj)
{
	return 100 * (*obj)->GetCon() / FullCon;
}

void SetCon(C4ObjectPtr *obj, int32_t newCon)
{
	(*obj)->DoCon(newCon - GetCon(obj));
}

void DoCon(C4ObjectPtr *obj, int32_t change)
{
	if (!obj) return;
	LuaHelpers::CallC4Script(obj, &FnDoCon,
							 static_cast<long>(change),
							 static_cast<C4Object *>(nullptr)
							 );
}

int32_t GetEnergy(const C4ObjectPtr *obj)
{
	return (*obj)->Energy / C4MaxPhysical;
}

void DoEnergy(C4ObjectPtr *obj, int32_t change, lua_State *L) // opt: bool exact, int32_t type, C4Player *player
{
	if (!obj) return;
	int32_t player = NO_OWNER;
	if (lua_gettop(L) >= 5)
	{
		player = LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 5).cast<C4PlayerPtr *>());
	}

	LuaHelpers::CallC4Script(obj, &FnDoEnergy,
							 static_cast<long>(change),
							 static_cast<C4Object *>(nullptr),
							 LuaHelpers::optboolean(L, 3, false),
							 static_cast<long>(luaL_optinteger(L, 4, 0)),
							 static_cast<long>(player + 1)
							 );
}

int32_t GetBreath(const C4ObjectPtr *obj)
{
	return 100 * (*obj)->Breath / C4MaxPhysical;
}

void SetBreath(C4ObjectPtr *obj, int32_t newBreath)
{
	(*obj)->DoBreath(newBreath - GetBreath(obj));
}

void DoBreath(C4ObjectPtr *obj, int32_t change)
{
	if (!obj) return;
	(*obj)->DoBreath(change);
}

int32_t GetDamage(const C4ObjectPtr *obj)
{
	return (*obj)->Damage;
}

void DoDamage(C4ObjectPtr *obj, int32_t change, lua_State *L) // opt: int32_t type, C4Player *player
{
	if (!obj) return;
	int32_t player = NO_OWNER;
	if (lua_gettop(L) >= 4)
	{
		player = LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 4).cast<C4PlayerPtr *>());
	}

	LuaHelpers::CallC4Script(obj, &FnDoDamage,
							 static_cast<long>(change),
							 static_cast<C4Object *>(nullptr),
							 static_cast<long>(luaL_optinteger(L, 3, 0)),
							 static_cast<long>(player + 1)
							 );
}

bool DoMagicEnergy(C4ObjectPtr *obj, int32_t change, lua_State *L) // opt: bool allowPartial
{
	if (!obj) return false;
	return LuaHelpers::CallC4Script(obj, &FnDoMagicEnergy,
									static_cast<long>(change),
									static_cast<C4Object *>(nullptr),
									LuaHelpers::optboolean(L, 3, false)
									);
}

int32_t GetMagicEnergy(const C4ObjectPtr *obj)
{
	return (*obj)->MagicEnergy / MagicPhysicalFactor;
}

void SetMagicEnergy(C4ObjectPtr *obj, int32_t newMagicEnergy)
{
	(*obj)->MagicEnergy = BoundBy<int32_t>(newMagicEnergy, 0, (*obj)->GetPhysical()->Magic);
}

bool SetPhysical(C4ObjectPtr *obj, std::string physical, int32_t value, lua_State *L) // opt: int32_t mode
{
	return LuaHelpers::CallC4Script(obj, &FnSetPhysical,
									C4VString(physical.c_str()).getStr(),
									static_cast<long>(value),
									static_cast<long>(luaL_optinteger(L, 4, 0)),
									static_cast<C4Object *>(nullptr)
									);
}

bool TrainPhysical(C4ObjectPtr *obj, std::string physical, int32_t value, int32_t maxTrain)
{
	return LuaHelpers::CallC4Script(obj, &FnTrainPhysical,
									C4VString(physical.c_str()).getStr(),
									static_cast<long>(value),
									static_cast<long>(maxTrain),
									static_cast<C4Object *>(nullptr)
									);
}

bool ResetPhysical(C4ObjectPtr *obj, std::string physical)
{
	return LuaHelpers::CallC4Script(obj, &FnResetPhysical,
									static_cast<C4Object *>(nullptr),
									C4VString(physical.c_str()).getStr()
									);
}

int32_t GetPhysical(C4ObjectPtr *obj, std::string physical, lua_State *L) // opt: int32_t mode, luabridge::LuaDef def
{
	return static_cast<int32_t>(LuaHelpers::CallC4Script(obj, &FnGetPhysical,
														 C4VString(physical.c_str()).getStr(),
														 static_cast<long>(luaL_optinteger(L, 3, 0)),
														 static_cast<C4Object *>(nullptr),
														 lua_gettop(L) >= 4
														   ? LuaHelpers::GetIDFromDef(luabridge::LuaRef::fromStack(L, 4))
														   : 0L)
														 );
}

bool GetEntrance(const C4ObjectPtr *obj)
{
	return (*obj)->EntranceStatus;
}

void SetEntrance(C4ObjectPtr *obj, bool newEntranceStatus)
{
	(*obj)->EntranceStatus = newEntranceStatus;
}

FIXED GetXDir(const C4ObjectPtr *obj)
{
	return (*obj)->xdir;
}

void SetXDir(C4ObjectPtr *obj, FIXED xdir)
{
	(*obj)->xdir = xdir;
	(*obj)->Mobile = true;
}

FIXED GetYDir(const C4ObjectPtr *obj)
{
	return (*obj)->ydir;
}

void SetYDir(C4ObjectPtr *obj, FIXED ydir)
{
	(*obj)->ydir = ydir;
	(*obj)->Mobile = true;
}

FIXED GetRDir(const C4ObjectPtr *obj)
{
	return (*obj)->rdir;
}

void SetRDir(C4ObjectPtr *obj, FIXED rdir)
{
	(*obj)->rdir = rdir;
	(*obj)->Mobile = true;
}

int32_t GetR(const C4ObjectPtr *obj)
{
	return (*obj)->r;
}

void SetR(C4ObjectPtr *obj, int32_t r)
{
	(*obj)->SetRotation(r);
}

bool SetAction(C4ObjectPtr *obj, luabridge::LuaRef action, lua_State *L) // opt: C4ObjectPtr *target, C4ObjectPtr *target2, bool direct
{
	if (!obj) return false;

	std::string act;
	if (action.isString())
	{
		act = action.tostring();
	}
	else if (action.isTable() && action["Name"].isString())
	{
		act = action["Name"].tostring();
	}
	else
	{
		return false;
	}

	auto *target  = lua_gettop(L) >= 3 ? luabridge::LuaRef::fromStack(L, 3).cast<C4ObjectPtr *>() : nullptr;
	auto *target2 = lua_gettop(L) >= 4 ? luabridge::LuaRef::fromStack(L, 4).cast<C4ObjectPtr *>() : nullptr;
	return (*obj)->SetActionByName(
				act.c_str(),
				*target,
				*target2,
				C4Object::SAC_StartCall | C4Object::SAC_AbortCall,
				lua_gettop(L) >= 5 ? lua_tointeger(L, 5) : false
				);
}

int SetBridgeActionData(lua_State *L) // C4Action *action, luabridge::LuaRef data
{
	if (lua_gettop(L) < 2) return 0;
	C4Action *action = luabridge::LuaRef::fromStack(L, 1);
	if (!action || action->Act <= ActIdle || action->Procedure != DFA_BRIDGE) return 0;

	auto data = luabridge::LuaRef::fromStack(L, 2);
	int32_t length = data["Length"].isNumber() ? data["Length"] : 0;
	int32_t moveClonk = data["MoveClonk"].isBool() ? data["MoveClonk"] : false;
	bool wall = data["Wall"].isBool() ? data["Wall"] : false;
	std::string material = data["Material"].isString() ? data["Material"].tostring() : "";

	action->SetBridgeData(length, moveClonk, wall, Game.Material.Get(material.c_str()));
	return 0;
}

int GetBridgeActionData(lua_State *L) // const C4Action *action
{
	if (lua_gettop(L) < 1) return 0;

	C4Action *action = luabridge::LuaRef::fromStack(L, 1);
	if (!action || action->Procedure != DFA_BRIDGE)
	{
		return 0;
	}

	int32_t length;
	bool moveClonk;
	bool wall;
	int32_t material;
	action->GetBridgeData(length, moveClonk, wall, material);

	luabridge::LuaRef ret = luabridge::newTable(L);
	ret["Length"] = length;
	ret["MoveClonk"] = moveClonk;
	ret["Wall"] = wall;
	if (MatValid(material))
	{
		ret["Material"] = Game.Material.Map[material].Name;
	}
	ret.push();
	return 1;
}

int GetActionData(lua_State *L) // const C4Action *action
{
	if (lua_gettop(L) < 1) return 0;
	luabridge::push(L, luabridge::LuaRef::fromStack(L, 1).cast<const C4Action *>()->Data);
	return 1;
}

int SetActionData(lua_State *L) // C4Action *action, int32_t data
{
	if (lua_gettop(L) < 2) return 0;

	C4Action *action = luabridge::LuaRef::fromStack(L, 1);
	if (!action) return 0;

	auto data = static_cast<int32_t>(luaL_checkinteger(L, 2));
	if (action->Act > ActIdle)
	{
		if (action->Procedure == DFA_BRIDGE)
		{
			luabridge::LuaRef f(L, &SetBridgeActionData);
			f(action, 0, false, false, data);
			return 0;
		}
		else if (action->Procedure == DFA_ATTACH)
		{
			if ((data & 255) >= C4D_MaxVertex || ((data >> 8) >= C4D_MaxVertex))
			{
				return luaL_error(L, "Invalid attach action data");
			}
		}
	}
	action->Data = data;
	return 0;
}

int32_t GetDir(const C4ObjectPtr *obj)
{
	return (*obj)->Action.Dir;
}

void SetDir(C4ObjectPtr *obj, int32_t newDir)
{
	(*obj)->SetDir(newDir);
}

uint32_t GetCategory(const C4ObjectPtr *obj)
{
	return (*obj)->Category;
}

void SetCategory(C4ObjectPtr *obj, uint32_t newCategory)
{
	if (!(newCategory & C4D_SortLimit))
	{
		newCategory |= ((*obj)->Category & C4D_SortLimit);
	}
	(*obj)->SetCategory(newCategory);
}

int32_t GetPhase(const C4Action *action)
{
	return action->Phase;
}

void SetPhase(C4Action *action, int32_t newPhase)
{
	action->Phase = BoundBy<int32_t>(newPhase, 0, action->Length);
}

bool ExecuteCommand(C4ObjectPtr *obj)
{
	return obj  ? (*obj)->ExecuteCommand() : false;
}

bool SetCommand(C4ObjectPtr *obj, std::string command, lua_State *L) // opt: C4ObjectPtr *target, int32_t x, int32_t y, C4ObjectPtr *target2, luabridge::LuaRef data, int32_t retries
{
	auto *target = lua_gettop(L) >= 3 ? luabridge::LuaRef::fromStack(L, 3).cast<C4ObjectPtr *>() : nullptr;
	auto x = static_cast<int32_t>(luaL_optinteger(L, 4, 0));
	auto y = static_cast<int32_t>(luaL_optinteger(L, 5, 0));
	auto *target2 = lua_gettop(L) >= 6 ? luabridge::LuaRef::fromStack(L, 6).cast<C4ObjectPtr *>() : nullptr;
	C4Value data;
	if (lua_gettop(L) >= 7)
	{
		data = luabridge::Stack<C4Value>::get(L, 7);
	}
	auto retries = static_cast<int32_t>(luaL_optinteger(L, 8, 0));

	return LuaHelpers::CallC4Script(obj, &FnSetCommand,
									C4VObj(nullptr),
									C4VString(command.c_str()),
									C4VObj(target  ? *target : nullptr),
									C4VInt(x),
									C4VInt(y),
									C4VObj(target2  ? *target2 : nullptr),
									data,
									C4VInt(retries)
									).getBool();
}

bool AddCommand(C4ObjectPtr *obj, std::string command, lua_State *L) // opt: C4ObjectPtr *target, int32_t x, int32_t y, C4ObjectPtr *target2, int32_t interval, luabridge::LuaRef data, int32_t retries, int32_t baseMode
{
	auto *target = lua_gettop(L) >= 3 ? luabridge::LuaRef::fromStack(L, 3).cast<C4ObjectPtr *>() : nullptr;
	auto x = static_cast<int32_t>(luaL_optinteger(L, 4, 0));
	auto y = static_cast<int32_t>(luaL_optinteger(L, 5, 0));
	auto *target2 = lua_gettop(L) >= 6 ? luabridge::LuaRef::fromStack(L, 6).cast<C4ObjectPtr *>() : nullptr;
	auto interval = static_cast<int32_t>(luaL_optinteger(L, 7, 0));
	C4Value data;
	if (lua_gettop(L) >= 8)
	{
		data = luabridge::Stack<C4Value>::get(L, 8);
	}
	auto retries = static_cast<int32_t>(luaL_optinteger(L, 9, 0));
	auto baseMode = static_cast<int32_t>(luaL_optinteger(L, 10, 0));

	return LuaHelpers::CallC4Script(obj, &FnAddCommand,
									C4VObj(nullptr),
									C4VString(command.c_str()),
									C4VObj(*target  ? *target : nullptr),
									C4VInt(x),
									C4VInt(y),
									C4VObj(*target2  ? *target2 : nullptr),
									C4VInt(interval),
									data,
									C4VInt(retries),
									C4VInt(baseMode)
									).getBool();
}

bool AppendCommand(C4ObjectPtr *obj, std::string command, lua_State *L) // opt: C4ObjectPtr *target, int32_t x, int32_t y, C4ObjectPtr *target2, int32_t interval, luabridge::LuaRef data, int32_t retries, int32_t baseMode
{
	auto target = lua_gettop(L) >= 3 ? luabridge::LuaRef::fromStack(L, 3).cast<C4ObjectPtr *>() : nullptr;
	auto x = static_cast<int32_t>(luaL_optinteger(L, 4, 0));
	auto y = static_cast<int32_t>(luaL_optinteger(L, 5, 0));
	auto target2 = lua_gettop(L) >= 6 ? luabridge::LuaRef::fromStack(L, 6).cast<C4ObjectPtr *>() : nullptr;
	auto interval = static_cast<int32_t>(luaL_optinteger(L, 7, 0));
	C4Value data;
	if (lua_gettop(L) >= 8)
	{
		data = luabridge::Stack<C4Value>::get(L, 8);
	}
	auto retries = static_cast<int32_t>(luaL_optinteger(L, 9, 0));
	auto baseMode = static_cast<int32_t>(luaL_optinteger(L, 10, 0));

	return LuaHelpers::CallC4Script(obj, &FnAppendCommand,
									C4VObj(nullptr),
									C4VString(command.c_str()),
									C4VObj(*target  ? *target : nullptr),
									C4VInt(x),
									C4VInt(y),
									C4VObj(*target2  ? *target2 : nullptr),
									C4VInt(interval),
									data,
									C4VInt(retries),
									C4VInt(baseMode)
									).getBool();
}

luabridge::LuaRef GetCommand(C4ObjectPtr *obj, lua_State *L) // opt: int32_t commandNum
{
	if (!obj) return LuaNil(L);
	int32_t commandNum = static_cast<int32_t>(luaL_optinteger(L, 3, 0));
	C4Command *command = (*obj)->Command;
	while (command  && commandNum--)
	{
		command = command->Next;
	}
	if (!command)
	{
		return LuaNil(L);
	}
	luabridge::LuaRef ret = luabridge::newTable(L);
	ret["Name"] = CommandName(command->Command);
	ret["Target"] = luabridge::newTable(L);
	ret["Target"]["First"] = LuaHelpers::ref(L, command->Target);
	ret["Target"]["X"] = command->Tx.getInt();
	ret["Target"]["Y"] = command->Ty;
	ret["Target"]["Second"] = LuaHelpers::ref(L, command->Target2);
	ret["Data"] = command->Data;
	return ret;
}

bool FinishCommand(C4ObjectPtr *obj, bool success, int32_t commandNum)
{
	return LuaHelpers::CallC4Script(obj, &FnFinishCommand,
									static_cast<C4Object *>(nullptr),
									success,
									static_cast<long>(commandNum)
									);
}

C4Action *GetAction(const C4ObjectPtr *obj)
{
	return &(*obj)->Action;
}

std::string GetName(const C4ObjectPtr *obj)
{
	return (*obj)->Name.getData();
}

void SetName(C4ObjectPtr *obj, std::string newName)
{
	(*obj)->SetName(newName.c_str());
}

bool FnSetName(C4ObjectPtr *obj, std::string newName, lua_State *L) // opt: luabridge::LuaRef def, bool setInInfo, bool makeValidIfExists
{
	if (!obj) return false;
	C4ID id = 0UL;
	if (lua_gettop(L) >= 3)
	{
		id = LuaHelpers::GetIDFromDef(luabridge::LuaRef::fromStack(L, 3));
	}
	return LuaHelpers::CallC4Script(obj, &::FnSetName,
							 C4VString(newName.c_str()).getStr(),
							 static_cast<C4Object *>(nullptr),
							 id,
							 LuaHelpers::optboolean(L, 5, false),
							 LuaHelpers::optboolean(L, 6, false)
							 );
}

int GetPlayers(lua_State *L)
{
	luabridge::LuaRef players = luabridge::newTable(L);
	for (C4Player *player = Game.Players.First; player ; player = player->Next)
	{
		players.append(LuaHelpers::ref(L, player));
	}
	players.push(L);
	return 1;
}

int32_t GetX(const C4ObjectPtr *obj)
{
	return (*obj)->x;
}

int32_t GetY(const C4ObjectPtr *obj)
{
	return (*obj)->y;
}

int32_t GetMass(const C4ObjectPtr *obj)
{
	return (*obj)->Mass;
}

void SetMass(C4ObjectPtr *obj, int32_t newMass)
{
	(*obj)->Mass = newMass;
}

#define GET(x, var) \
	int Get##x(lua_State *L) \
	{ \
		if (lua_gettop(L) < 1) return 0; \
		const C4ObjectPtr *obj = luabridge::LuaRef::fromStack(L, 1); \
		if (!obj || (*obj)->var == NO_OWNER || !ValidPlr((*obj)->var)) \
		{ \
			return 0; \
		} \
		C4Player *player = Game.Players.Get((*obj)->var); \
		if (!player) \
		{ \
			return 0; \
		} \
		luabridge::push(L, C4PlayerPtr(L, player)); \
		return 1; \
	}

#define SET(x, var) \
	int Set##x(lua_State *L) \
	{ \
		if (lua_gettop(L) < 2) return 0; \
		C4ObjectPtr *obj = luabridge::LuaRef::fromStack(L, 1); \
		if (!obj) \
		{ \
			return 0; \
		} \
		C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 2); \
		(*obj)->var = player  ? (*player)->Number : NO_OWNER; \
		return 0; \
	}

GET(Owner, Owner)

int SetOwner(lua_State *L) // C4ObjectPtr *obj, C4PlayerPtr *newPlayer
{
	if (lua_gettop(L) < 2) return 0;
	const C4ObjectPtr *obj = luabridge::LuaRef::fromStack(L, 1);
	if (!obj)
	{
		return 0;
	}
	C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 2);
	(*obj)->SetOwner(player  ? (*player)->Number : NO_OWNER);
	return 0;
}

GET(Controller, Controller)
SET(Controller, Controller)

GET(Killer, LastEnergyLossCausePlayer)
SET(Killer, LastEnergyLossCausePlayer)

#undef GET
#undef SET

uint32_t GetOCF(const C4ObjectPtr *obj)
{
	return (*obj)->OCF;
}

C4FindObject *CreateCriteriaFromTable(luabridge::LuaRef table)
{
	assert(table.isTable());
	std::vector<std::vector<luabridge::LuaRef>> criteria = table;

	std::vector<C4FindObject *> findCriteria;
	std::vector<C4SortObject *> sortCriteria;

	for (const auto &criterion : criteria)
	{
		if (criterion.size() < 2 || !criterion[0].isNumber() || criterion[1].isNil())
		{
			continue;
		}

		C4ValueArray array(2);
		array.IncRef();
		array[0] = C4VInt(criterion[0].cast<int32_t>());
		array[1] = criterion[1].cast<C4Value>();

		C4SortObject *sortCriterion = nullptr;
		C4FindObject *findCriterion = C4FindObject::CreateByValue(C4VArray(&array), &sortCriterion);

		if (findCriterion)
		{
			findCriteria.push_back(findCriterion);
		}

		if (sortCriterion)
		{
			sortCriteria.push_back(sortCriterion);
		}
	}

	if (findCriteria.empty())
	{
		while (sortCriteria.size())
		{
			delete sortCriteria.back();
			sortCriteria.pop_back();
		}
		return nullptr;
	}
	C4SortObject *sortCriterion = nullptr;
	if (sortCriteria.size())
	{
		if (sortCriteria.size() == 1)
		{
			sortCriterion = sortCriteria[0];
		}
		else
		{
			sortCriterion = new C4SortObjectMultiple(static_cast<int32_t>(sortCriteria.size()), &sortCriteria[0], false);
		}
	}
	C4FindObject *findCriterion = findCriteria.size() == 1 ? findCriteria[0] : new C4FindObjectAnd(static_cast<int32_t>(findCriteria.size()), &findCriteria[0], false);
	if (sortCriterion)
	{
		findCriterion->SetSort(sortCriterion);
	}
	return findCriterion;
}

luabridge::LuaRef FindObjects(luabridge::LuaRef context, luabridge::LuaRef criteria)
{
	(void) context;
	lua_State *L = criteria.state();
	if (criteria.isTable())
	{
		C4FindObject *findCriteria = CreateCriteriaFromTable(criteria);
		if (findCriteria)
		{
			C4ValueArray *result = findCriteria->FindMany(Game.Objects, Game.Objects.Sectors);
			delete findCriteria;

			luabridge::LuaRef ret = luabridge::newTable(L);
			for (int32_t i = 0; i < result->GetSize(); ++i)
			{
				ret.append(LuaHelpers::ref(L,  (*result)[i].getObj()));
			}
			delete result;
			return ret;
		}
	}
	return LuaHelpers::error(L, "FindObject: No valid search criteria specified");
}

luabridge::LuaRef FindObject(luabridge::LuaRef context, luabridge::LuaRef criteria)
{
	(void) context;
	lua_State *L = criteria.state();
	if (criteria.isTable())
	{
		C4FindObject *findCriteria = CreateCriteriaFromTable(criteria);
		if (findCriteria)
		{
			C4Object *result = findCriteria->Find(Game.Objects, Game.Objects.Sectors);
			delete findCriteria;
			if (result)
			{
				return luabridge::LuaRef(L, LuaHelpers::ref(criteria.state(), result));
			}
			else
			{
				return LuaNil(L);
			}
		}
	}
	return LuaHelpers::error(L, "FindObject: No valid search criteria specified");
}

int32_t ObjectCount(luabridge::LuaRef context, luabridge::LuaRef criteria)
{
	(void) context;
	lua_State *L = criteria.state();
	if (criteria.isTable())
	{
		C4FindObject *findCriteria = CreateCriteriaFromTable(criteria);
		if (findCriteria)
		{
			int32_t count = findCriteria->Count(Game.Objects, Game.Objects.Sectors);
			delete findCriteria;
			return count;
		}
	}
	return LuaHelpers::error(L, "ObjectCount: No valid search criteria specified");
}

bool GrabObjectInfo(C4ObjectPtr *obj, C4ObjectPtr *target)
{
	if (!obj || !target) return false;
	return (*obj)->GrabInfo(*target);
}

bool BurnMaterial(luabridge::LuaRef context, int32_t x, int32_t y)
{
	(void) context;
	int32_t mat = GBackMat(x, y);
	return MatValid(mat) && Game.Material.Map[mat].Inflammable && Game.Landscape.ExtractMaterial(x, y) != MNone;
}

void Smoke(luabridge::LuaRef context, int32_t x, int32_t y, int32_t level, lua_State *L) // opt: int32_t dwClr
{
	(void) context;
	auto dwClr = static_cast<uint32_t>(luaL_optinteger(L, 5, 0));
	::Smoke(x, y, level, dwClr);
}

void Bubble(luabridge::LuaRef context, int32_t x, int32_t y)
{
	(void) context;
	BubbleOut(x, y);
}

C4Material *ExtractLiquid(luabridge::LuaRef context, int32_t x, int32_t y, lua_State *L)
{
	(void) context;
	if (GBackLiquid(x, y))
	{
		int32_t index = Game.Landscape.ExtractMaterial(x, y);
		if (MatValid(index))
		{
			return &(Game.Material.Map[index]);
		}
	}
	return LuaNil(L);
}

int32_t GetMaterialIndex(luabridge::LuaRef context, const C4Material *mat)
{
	(void) context;
	if (!mat) return MNone;
	return Game.Material.Get(mat->Name.c_str());
}

C4Material *GetMaterial(luabridge::LuaRef context, int32_t x, int32_t y)
{
	(void) context;
	int32_t index = GBackMat(x, y);
	return MatValid(index) ? &Game.Material.Map[index] : nullptr;
}

luabridge::LuaRef GetTexture(luabridge::LuaRef context, int32_t x, int32_t y, lua_State *L)
{
	(void) context;
	int32_t tex = PixCol2Tex(GBackPix(x, y));
	if (!tex) return LuaNil(L);

	const C4TexMapEntry *texture = Game.TextureMap.GetEntry(tex);
	if (!tex) return LuaNil(L);

	return luabridge::LuaRef(L, std::string{texture->GetTextureName()});
}

#define M(t) bool GBack##t(luabridge::LuaRef context, int32_t x, int32_t y) \
{ \
	(void) context; \
	return ::GBack##t(x, y); \
}

M(Solid)
M(SemiSolid)
M(Liquid)
M(IFT)

#undef M

void BlastObjects(C4ObjectPtr *obj, int32_t x, int32_t y, int32_t level, lua_State *L) // opt: C4ObjectPtr *container, C4Player *causedBy
{
	if (!obj) return;

	auto *container  = lua_gettop(L) >= 5 ? luabridge::LuaRef::fromStack(L, 5).cast<C4ObjectPtr *>() : nullptr;
	auto causedBy = lua_gettop(L) >= 6 ? LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 6).cast<C4PlayerPtr *>()) : NO_OWNER;

	if (causedBy == NO_OWNER)
	{
		causedBy = (*obj)->Controller;
	}

	Game.BlastObjects(x, y, level, *container, causedBy, *obj);
}

void BlastObject(C4ObjectPtr *obj, int32_t level, lua_State *L) // opt: C4PlayerPtr *causedBy
{
	if (!obj || !(*obj)->Status) return;

	auto causedBy = lua_gettop(L) >= 4 ? LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 4).cast<C4PlayerPtr *>()) : NO_OWNER;
	if (causedBy == NO_OWNER)
	{
		causedBy = (*obj)->Controller;
	}

	(*obj)->Blast(level, causedBy);
}

void BlastFree(luabridge::LuaRef context, int32_t x, int32_t y, int32_t level, lua_State *L) // opt: C4PlayerPtr *causedBy
{
	(void) context;

	auto causedBy = lua_gettop(L) >= 4 ? LuaHelpers::GetPlayerNumber(luabridge::LuaRef::fromStack(L, 4).cast<C4PlayerPtr *>()) : NO_OWNER;
	Game.Landscape.BlastFree(x, y, level, BoundBy((level / 10) - 1, 1, 3), causedBy);
}

void Sound(luabridge::LuaRef /* soundNamespace */, luabridge::LuaRef context, luabridge::LuaRef arguments, lua_State *L)
{
	auto *player = !arguments["Player"].isNil() ? arguments["Player"].cast<C4PlayerPtr *>() : nullptr;
	if (player && !(*player)->LocalControl)
	{
		return;
	}

	bool global = arguments["Global"].isBool() ? arguments["Global"] : false;

	C4Object *obj = nullptr;
	if (!global)
	{
		obj = LuaHelpers::GetRawPointerFromContext<decltype(obj)>(context);
	}

	auto loop = !arguments["LoopCount"].isNil() ? BoundBy(arguments["LoopCount"].cast<int32_t>(), -1, 1) : 0;
	bool multiple = arguments["Multiple"].isBool() ? arguments["Multiple"] : false;
	std::string sound = arguments["Name"].isString() ? arguments["Name"].tostring() : "";

	if (loop >= 0)
	{
		if (!multiple && GetSoundInstance(sound.c_str(), obj))
		{
			return;
		}

		auto volume = !arguments["Volume"].isNil() ? BoundBy(arguments["Volume"].cast<int32_t>(), 1, 100) : 100;
		if (!(arguments["X"].isNil() && arguments["Y"].isNil()))
		{
			if (global)
			{
				luaL_error(L, "Global sounds must not have coordinates specified!");
			}

			else if (obj)
			{
				luaL_error(L, "Object sounds must not have coordinates specified!");
			}
			else if (arguments["X"].isNil() || arguments["Y"].isNil())
			{
				luaL_error(L, "Invalid coordinates specified for sound playback!");
			}
			StartSoundEffectAt(sound.c_str(), arguments["X"], arguments["Y"], !!loop, volume);
		}
		else
		{
			int32_t customFalloffDistance = !arguments["CustomFalloffDistance"].isNil() ? arguments["CustomFalloffDistance"] : 0;
			StartSoundEffect(sound.c_str(), !!loop, volume, obj, customFalloffDistance);
		}
	}
	else
	{
		StopSoundEffect(sound.c_str(), obj);
	}
}

void SoundLevel(luabridge::LuaRef context, std::string sound, int32_t level)
{
	::SoundLevel(sound.c_str(), LuaHelpers::GetRawPointerFromContext<C4Object *>(context), level);
}

void Music(lua_State *L) // luabridge::LuaRef musicNamespace, luabridge::LuaRef context, std::string songName (nillable), opt: bool loop
{
	Application.MusicSystem.Stop();
	if (lua_isnoneornil(L, 3))
	{
		Config.Sound.RXMusic = false;
	}
	else
	{
		Config.Sound.RXMusic = Application.MusicSystem.Play(lua_tostring(L, 3), !!luaL_optinteger(L, 4, 0));
	}
}

int32_t MusicLevel(luabridge::LuaRef context, int32_t level)
{
	(void) context;
	Game.SetMusicLevel(level);
	return Application.MusicSystem.SetVolume(level);
}

int32_t SetPlaylist(luabridge::LuaRef context, luabridge::LuaRef playlist, lua_State *L) // opt: bool restartMusic
{
	(void) context;

	auto p = playlist.cast<std::vector<std::string>>();
	std::string l = std::accumulate(std::next(p.begin()), p.end(), p[0], [](const std::string &a, const std::string &b)
	{
		return a + ";" + b;
	});

	int32_t filesInPlaylist = Application.MusicSystem.SetPlayList(l.c_str());
	Game.PlayList.Copy(l.c_str());

	if (!!luaL_optinteger(L, 3, 0) && Config.Sound.RXMusic)
	{
		Application.MusicSystem.Play();
	}
	return Game.Control.SyncMode() ? 0 : filesInPlaylist;
}

bool GameOver(lua_State *L) // luabridge::LuaRef context, opt: int32_t gameOverValue (unused?!)
{
	(void) L;
	return Game.DoGameOver();
}

void GainMissionAccess(luabridge::LuaRef context, std::string password)
{
	(void) context;
	Config.General.MissionAccess.insert(password);
}

void AddMessage(luabridge::LuaRef context, luabridge::LuaRef arguments, lua_State *L)
{
	if (arguments["Message"].isNil())
	{
		luaL_error(L, "No message specified!");
		return;
	}
	std::string message = arguments["Message"];
	int32_t x = !arguments["X"].isNil() ? arguments["X"] : 0;
	int32_t y = !arguments["Y"].isNil() ? arguments["Y"] : 0;
	int32_t player = LuaHelpers::GetPlayerNumber(arguments["Player"]);

	auto *obj = LuaHelpers::GetRawPointerFromContext<C4Object *>(context);
	if (obj)
	{
		Game.Messages.Append(C4GM_Target, message.c_str(), obj, player, x - obj->x, y - obj->y, FWhite);
	}
	else
	{
		Game.Messages.Append(C4GM_Global, message.c_str(), obj, player == NO_OWNER ? ANY_OWNER : player, x, y, FWhite);
	}
}

void ScriptGo(luabridge::LuaRef context, bool go)
{
	(void) context;
	Game.Script.Go = go;
}

void CastPXS(luabridge::LuaRef context, C4Material *material, int32_t amount, int32_t level, int32_t x, int32_t y)
{
	(void) context;
	Game.PXS.Cast(Game.Material.Get(material->Name.c_str()), amount, x, y, level);
}

void CastObjects(luabridge::LuaRef context, C4DefPtr *def, int32_t amount, int32_t level, int32_t x, int32_t y)
{
	if (!def) return;
	auto *obj = LuaHelpers::GetRawPointerFromContext<C4Object *>(context);
	Game.CastObjects((*def)->id, obj, amount, level, x, y, obj ? obj->Owner : NO_OWNER, obj ? obj->Controller : NO_OWNER);
}

luabridge::LuaRef PlaceVegetation(luabridge::LuaRef context, C4DefPtr *def, int32_t x, int32_t y, int32_t width, int32_t height, int32_t growth, lua_State *L)
{
	(void) context;
	if (!def) return LuaNil(L);

	C4Object *obj = Game.PlaceVegetation((*def)->id, x, y, width, height, growth);
	return obj ? luabridge::LuaRef(L, LuaHelpers::ref(L, obj)) : LuaNil(L);
}

luabridge::LuaRef PlaceAnimal(luabridge::LuaRef context, C4DefPtr *def, lua_State *L)
{
	(void) context;
	if (!def) return LuaNil(L);

	C4Object *obj = Game.PlaceAnimal((*def)->id);
	return obj ? luabridge::LuaRef(L, LuaHelpers::ref(L, obj)) : LuaNil(L);
}

// DrawVolcanoBranch - left out


luabridge::LuaRef ObjectCall(C4ObjectPtr *obj, std::string functionName, lua_State *L) // opt: arguments
{
	if (!obj) return LuaNil(L);

	C4AulParSet pars;
	for (int32_t i = 3; i < std::min(lua_gettop(L) + 1, C4AUL_MAX_Par + 3); ++i)
	{
		pars[i - 3] = luabridge::LuaRef::fromStack(L, i);
	}

	try
	{
		return luabridge::LuaRef(L, (*obj)->Call(functionName.c_str(), &pars, true));
	}
	catch (C4AulExecError *e)
	{
		e->show();
		delete e;
		return LuaHelpers::error(L, "");
	}
}

int Call(lua_State *L) // opt: arguments
{
	if (lua_gettop(L) < 2) return 0;

	size_t size;
	const char *functionName = luaL_checklstring(L, 2, &size);
	if (!size)
	{
		return 0;
	}

	luabridge::LuaRef function = luabridge::getGlobal(L, "Game")[functionName];
	if (function.isNil())
	{
		return 0;
	}

	function.push();
	lua_insert(L, 2);
	int top = lua_gettop(L) - 2;
	lua_call(L, lua_gettop(L) - 2, LUA_MULTRET);
	return lua_gettop(L) - top;
}

#define PREFIX

#define CONCAT(a, b) a ## b
#define CONCAT2(a, b) CONCAT(a, b)

#define GET(type, property) type Get##property(const CONCAT2(PREFIX, Ptr)* player) \
{ \
	return (*player)->property; \
}

#define SET(type, property) void Set##property(CONCAT2(PREFIX, Ptr) *player, type property) \
{ \
	(*player)->property = property; \
}

#define PROPERTY(type, property) \
	GET(type, property) \
	SET(type, property)

// C4AulFuncPtr

namespace C4AulFunc
{
#undef PREFIX
#define PREFIX C4AulFunc

GET(std::string, Name)

int32_t GetParCount(const C4AulFuncPtr *func)
{
	return (*func)->GetParCount();
}

std::vector<C4V_Type> GetParTypes(const C4AulFuncPtr *func)
{
	C4V_Type *types = (*func)->GetParType();
	if (types)
	{
		return std::vector<C4V_Type>(types, types + (*func)->GetParCount());
	}
	return {};
}

luabridge::LuaRef __call(C4AulFuncPtr *func, luabridge::LuaRef context, lua_State *L) // opt: arguments
{
	if (!func) return LuaNil(L);
	func->checkObject();

	int32_t top = lua_gettop(L);

	C4ObjectPtr *obj = nullptr;
	std::optional<decltype(obj)> opt = LuaHelpers::cast<decltype(obj)>(context);
	if (opt)
	{
		obj = *opt;

		if (obj)
		{
			LogF("foobar: %s", (*obj)->Name.getData());
		}
	}
	assert(top == lua_gettop(L));

	if (lua_gettop(L) > C4AUL_MAX_Par + 2)
	{
		return LuaHelpers::error(L, "Too many arguments supplied (%d / %d)", lua_gettop(L), C4AUL_MAX_Par);
	}

	C4AulParSet pars;
	for (int32_t arg = 3; arg <= top; ++arg)
	{
		pars[arg - 3] = luabridge::LuaRef::fromStack(L, arg).cast<C4Value>();
	}
	C4Value ret = func->checkObject()->Exec(obj ? obj->checkObject() : nullptr, &pars);
	return luabridge::LuaRef(L, ret);
}
}

// C4DefPtr
namespace C4Def
{
#undef PREFIX
#define PREFIX C4Def

PROPERTY(std::string, Name)
GET(C4Shape, Shape)
GET(C4Rect, Entrance)
GET(C4Rect, Collection)
GET(C4Rect, PictureRect)
GET(C4TargetRect, SolidMask)
GET(C4TargetRect, TopFace)
GET(int32_t, GrowthType)
GET(int32_t, Basement)
GET(bool, CanBeBase)
GET(bool, CrewMember)
GET(bool, NativeCrew)
GET(int32_t, Mass)
GET(int32_t, Value)
GET(bool, Exclusive)
GET(uint32_t, Category)
GET(int32_t, Growth)
GET(bool, Rebuyable)
GET(int32_t, ContactIncinerate)
GET(int32_t, BlastIncinerate)
GET(bool, Constructable)
GET(int32_t, Grab)
GET(bool, Carryable)
GET(bool, Rotateable)
GET(bool, Chopable)
GET(int32_t, Float)
GET(bool, ColorByOwner)
GET(bool, NoHorizontalMove)
GET(int32_t, BorderBound)
GET(int32_t, LiftTop)
GET(int32_t, CollectionLimit)
GET(uint32_t, GrabPutGet)
GET(bool, ContainBlast)
}

// C4IDList
namespace C4IDList
{
void __newindex(::C4IDList *list, C4DefPtr *key, int32_t value, lua_State *L)
{
	if (!list) return;

	if (!key)
	{
		luaL_error(L, "Definition cannot be nil");
	}
	list->SetIDCount((*key)->id, value, true);
}

int32_t __index(::C4IDList *list, C4DefPtr *key, lua_State *L)
{
	if (!list) return LuaNil(L);

	if (!key)
	{
		luaL_error(L, "Definition cannot be nil");
	}

	return list->GetIDCount((*key)->id);
}

int32_t __len(::C4IDList *list)
{
	if (!list) return 0;
	return list->GetNumberOfIDs();
}

}

// C4MaterialCore
namespace C4MaterialCore
{
#define MAT(x) ::C4MaterialCore *Get##x(const ::C4MaterialCore *mat) \
{ \
	if (mat) \
	{ \
		int32_t index = Game.Material.Get(mat->s##x.c_str()); \
		if (MatValid(index)) \
		{ \
			return &Game.Material.Map[index]; \
		} \
	} \
	return nullptr; \
}
MAT(BlastShiftTo)
MAT(InMatConvert)
MAT(InMatConvertTo)
MAT(BelowTempConvertTo)
MAT(AboveTempConvertTo)
#undef MAT
}

// C4Material
namespace C4Material
{
uint32_t GetMaterialCount(::C4Material *mat, lua_State *L) // opt: bool real
{
	if (!mat) return 0;
	int32_t index = GetMaterialIndex(LuaNil(L), mat);
	if (LuaHelpers::optboolean(L, 2, false) || !mat->MinHeightCount)
	{
		return Game.Landscape.MatCount[index];
	}
	return Game.Landscape.EffectiveMatCount[index];
}

bool InsertMaterial(::C4Material *mat, int32_t x, int32_t y, lua_State *L)
{
	if (!mat) return false;
	return Game.Landscape.InsertMaterial(
				GetMaterialIndex(LuaNil(L), mat),
				x, y,
				static_cast<int32_t>(luaL_optinteger(L, 4, 0)),
				static_cast<int32_t>(luaL_optinteger(L, 5, 0))
				);
}

uint32_t ExtractMaterialAmount(::C4Material *mat, int32_t x, int32_t y, uint32_t amount, lua_State *L)
{
	int32_t index = GetMaterialIndex(LuaNil(L), mat);
	if (!MatValid(index))
	{
		return 0;
	}
	uint32_t extracted = 0;
	for(; extracted < amount && GBackMat(x, y) == index && Game.Landscape.ExtractMaterial(x, y) == index; ++extracted);
	return extracted;
}
}

// C4ObjectPtr

namespace C4Object
{

namespace
{

std::optional<luabridge::LuaRef> CheckLuaLocals(::C4Object *obj, const std::string &key)
{
	if (auto i = obj->LuaLocals.find(key); i != obj->LuaLocals.end())
	{
		return i->second;
	}
	return std::nullopt;
}

std::optional<C4Value *> CheckC4ScriptLocals(::C4Object *obj, const std::string &key)
{
	if (C4Value *value = obj->LocalNamed.GetItem(key.c_str()); value)
	{
		return std::move(value);
	}
	return std::nullopt;
}

std::optional<luabridge::LuaRef> CheckDef(lua_State *L, ::C4Object *obj, const std::string &key)
{
	if (luabridge::LuaRef value = luabridge::LuaRef(L, obj->Def->wrapper)[key]; !value.isNil())
	{
		return std::move(value);
	}
	return std::nullopt;
}

std::optional<luabridge::LuaRef> CheckLuaDef(::C4Object *obj, const std::string &key)
{
	if (!obj->Def->LuaDef.isNil() && !obj->Def->LuaDef[key].isNil())
	{
		return obj->Def->LuaDef[key];
	}
	return std::nullopt;
}

std::optional<::C4AulFunc *> CheckScriptFunctions(::C4Object *obj, const std::string &key)
{
	if (::C4AulFunc *f = obj->Def->Script.GetSFunc(key.c_str(), AA_PROTECTED, true); f)
	{
		return std::move(f);
	}
	return std::nullopt;
}

}

void __newindex(C4ObjectPtr *obj, std::string key, luabridge::LuaRef value, lua_State *L)
{
	if (!obj) return;

	if (luabridge::LuaRef v = luabridge::LuaRef(L, &luabridge::CFunc::indexMetaMethod)(obj, key); !v.isNil())
	{
		try
		{
			luabridge::LuaRef(L, &luabridge::CFunc::newindexObjectMetaMethod)(obj, key, value);
			return;
		}

		catch (luabridge::LuaException const &)
		{
		}
	}

	else if (auto opt = CheckC4ScriptLocals(*obj, key); opt)
	{
		(*obj)->LocalNamed[key.c_str()] = value;
	}

	else if (auto opt = CheckDef(L, *obj, key); opt)
	{
		luabridge::LuaRef(L, (*obj)->Def->wrapper)[key] = value;
	}

	else if (auto opt = CheckLuaDef(*obj, key); opt)
	{
		(*obj)->Def->LuaDef[key] = value;
	}

	else
	{
		(*obj)->LuaLocals.insert_or_assign(key, value);
	}
}

luabridge::LuaRef __index(C4ObjectPtr *obj, std::string key, lua_State *L)
{
	if (!obj) return LuaNil(L);

	LogF("key: %s", key.c_str());

	if (luabridge::LuaRef value = luabridge::LuaRef(L, &luabridge::CFunc::indexMetaMethod)(obj, key); !value.isNil())
	{
		return value;
	}

	else if (auto opt = CheckLuaLocals(*obj, key); opt)
	{
		return *opt;
	}

	else if (auto opt = CheckC4ScriptLocals(*obj, key); opt)
	{
		return luabridge::LuaRef(L, **opt);
	}

	else if (auto opt = CheckDef(L, *obj, key); opt)
	{
		return *opt;
	}

	else if (auto opt = CheckLuaDef(*obj, key); opt)
	{
		return *opt;
	}

	else if (auto opt = CheckScriptFunctions(*obj, key); opt)
	{
		return luabridge::LuaRef(L, LuaHelpers::ref(L, *opt));
	}

	return LuaNil(L);
}

std::string __tostring(C4ObjectPtr *obj)
{
	if (!obj) return "";
	return FormatString("%s #%d", (*obj)->Name.getData(), (*obj)->Number).getData();
}
}

// C4PlayerPtr

namespace C4Player
{
#undef PREFIX
#define PREFIX C4Player

GET(std::string, Name)
GET(int32_t, Status)
GET(bool, Eliminated)
GET(bool, Surrendered)
GET(bool, Evaluated)
GET(int32_t, Number)
GET(int32_t, ID)
GET(int32_t, Team) // TODO: C4TeamPtr?
PROPERTY(uint32_t, ColorDw)
GET(int32_t, Control)
PROPERTY(int32_t, MouseControl)
GET(int32_t, PlrStartIndex)

std::string GetAtClientName(const C4PlayerPtr *player)
{
	return (*player)->AtClientName;
}

PROPERTY(int32_t, Wealth)
PROPERTY(int32_t, Points)
PROPERTY(int32_t, Value)
GET(int32_t, InitialValue)
GET(int32_t, ValueGain)
GET(int32_t, ObjectsOwned)
GET(int32_t, ShowControl)
PROPERTY(int32_t, ShowControlPos)
GET(int32_t, FlashCom)

void SetFlashCom(C4PlayerPtr *player, int32_t newFlashCom)
{
	(*player)->FlashCom = newFlashCom;
	Config.Graphics.ShowCommands = true;
}

int GetCaptain(lua_State *L)
{
	if (lua_gettop(L) >= 1)
	{
		const C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 1);
		if ((*player)->Captain)
		{
			luabridge::push(L, LuaHelpers::ref(L, (*player)->Captain));
			return 1;
		}
	}
	return 0;
}

PROPERTY(bool, AutoContextMenu)
PROPERTY(bool, ControlStyle)
GET(int32_t, LastCom)
GET(int32_t, LastComDelay)
GET(int32_t, LastComDownDouble)

int GetCursor(lua_State *L)
{
	if (lua_gettop(L) >= 1)
	{
		const C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 1);
		if ((*player)->Cursor)
		{
			luabridge::push(L, LuaHelpers::ref(L, (*player)->Cursor));
			return 1;
		}
	}
	return 0;
}

int SetCursor(lua_State *L)
{
	if (lua_gettop(L) >= 2)
	{
		C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 1);
		auto newCursor = luabridge::LuaRef::fromStack(L, 2);
		(*player)->SetCursor(!newCursor.isNil() ? *(newCursor.cast<C4ObjectPtr *>()) : nullptr, false, false);
	}
	return 0;
}

std::string GetTaggedPlayerName(const C4PlayerPtr *player)
{
	uint32_t color = (*player)->ColorDw;
	C4GUI::MakeColorReadableOnBlack(color);
	return FormatString("<c %x>%s</c>", color & 0xffffff, (*player)->GetName()).getData();
}

int32_t GetType(const C4PlayerPtr *player)
{
	return (*player)->GetType();
}

int32_t GetActiveCrewCount(const C4PlayerPtr *player)
{
	return (*player)->ActiveCrewCount();
}

int32_t GetSelectedCrewCount(const C4PlayerPtr *player)
{
	return (*player)->GetSelectedCrewCount();
}

void Eliminate(C4PlayerPtr *player, lua_State *L) // opt: bool removeDirect
{
	if (!player || (*player)->Eliminated) return;

	if (LuaHelpers::optboolean(L, 2, false))
	{
		if (Game.Control.isCtrlHost())
		{
			Game.Players.CtrlRemove((*player)->Number, false);
		}
	}
	else
	{
		(*player)->Eliminate();
	}
}

void Surrender(C4PlayerPtr *player)
{
	if (!player || (*player)->Eliminated) return;
	(*player)->Surrender();
}

void DoWealth(C4PlayerPtr *player, int32_t wealthChange)
{
	if (!player || !wealthChange) return;
	(*player)->DoWealth(wealthChange);
}

void SetFoW(C4PlayerPtr *player, bool enabled)
{
	if (!player) return;
	(*player)->SetFoW(enabled);
}

bool MakeCrewMember(C4PlayerPtr *player, C4ObjectPtr *obj)
{
	if (!player || !obj) return false;
	return (*player)->MakeCrewMember((*obj));
}

bool HostileTo(C4PlayerPtr *player1, C4PlayerPtr *player2, lua_State *L) // opt: bool checkOneWayOnly
{
	if (!player1 || !player2) return false;

	if (!!luaL_optinteger(L, 3, 0))
	{
		return Game.Players.HostilityDeclared((*player1)->Number, (*player2)->Number);
	}
	return ::Hostile((*player1)->Number, (*player2)->Number);
}

bool SetHostility(C4PlayerPtr *player1, C4PlayerPtr *player2, bool hostile, lua_State *L) // opt: bool silent, bool noCalls
{
	if (!player1 || !player2) return false;

	if (!luaL_optinteger(L, 5, 0)
			&& !!Game.Script.GRBroadcast(
				PSF_RejectHostilityChange,
				&C4AulParSet(C4VInt((*player1)->Number), C4VInt((*player2)->Number), C4VBool(hostile)),
				true,
				true
				)
			)
	{
		return false;
	}

	bool oldHostility = Game.Players.HostilityDeclared((*player1)->Number, (*player2)->Number);
	if (!(*player1)->SetHostility((*player2)->Number, hostile, !!luaL_optinteger(L, 4, 0)))
	{
		return false;
	}

	Game.Script.GRBroadcast(
				PSF_OnHostilityChange,
				&C4AulParSet(C4VInt((*player1)->Number), C4VInt((*player2)->Number), C4VBool(hostile), C4VBool(oldHostility)),
				true
				);

	return true;
}

int GetPlayerView(lua_State *L) // const C4PlayerPtr *player
{
	if (lua_gettop(L) < 1) return 0;

	const C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 1);
	if ((*player)->ViewMode == C4PVM_Target)
	{
		::C4Object *target = (*player)->ViewTarget;
		if (target)
		{
			luabridge::push(L, LuaHelpers::ref(L, target));
			return 1;
		}
	}
	return 0;
}

int SetPlayerView(lua_State *L) // C4PlayerPtr *player, C4ObjectPtr *obj
{
	if (lua_gettop(L) < 2) return 0;

	C4PlayerPtr *player = luabridge::LuaRef::fromStack(L, 1);
	C4ObjectPtr *obj = luabridge::LuaRef::fromStack(L, 2);
	if (!obj) return 0;

	(*player)->SetViewMode(C4PVM_Target, *obj);
	return 0;
}

void SetPlayerShowControl(C4PlayerPtr *player, int32_t control)
{
	if (!player) return;
	(*player)->ShowControl = control;
}

void SetPlayerShowCommand(C4PlayerPtr *player, int32_t command)
{
	if (!player) return;

	(*player)->FlashCom = command;
	Config.Graphics.ShowCommands = true;
}

void SetPlayerShowControlPos(C4PlayerPtr *player, int32_t pos)
{
	if (!player) return;
	(*player)->ShowControlPos = pos;
}

std::string GetPlayerControlName(C4PlayerPtr *player, int32_t control, lua_State *L) // opt: bool
{
	if (!player) return "";
	return PlrControlKeyName((*player)->Number, control, !!luaL_optinteger(L, 3, 0)).getData();
}
}

#undef GET
#undef SET
#undef PROPERTY

}

bool C4LuaScriptEngine::Init()
{
	if (!C4Lua::Init())
	{
		return false;
	}

#define C(name) .addProperty(#name, const_cast<int32_t *>(CONCAT2(&PREFIX, CONCAT2(_, name))), false)

	luabridge::getGlobalNamespace(L)
		.addFunction("print", &LuaScriptFn::print)
		.addFunction("dofile", &LuaScriptFn::dofile)
		.addFunction("loadfile", &LuaScriptFn::dofile)
		.beginNamespace("Game")

#undef PREFIX
#define PREFIX C4D
			.beginNamespace("Category")
				C(None)
				C(All)

				C(StaticBack)
				C(Structure)
				C(Vehicle)
				C(Living)
				C(Object)

				C(SortLimit)

				C(Goal)
				C(Environment)

				C(SelectBuilding)
				C(SelectVehicle)
				C(SelectMaterial)
				C(SelectKnowledge)
				C(SelectHomebase)
				C(SelectAnimal)
				C(SelectNest)
				C(SelectInEarth)
				C(SelectVegetation)

				C(TradeLiving)
				C(Magic)
				C(CrewMember)

				C(Rule)

				C(Background)
				C(Parallax)
				C(MouseSelect)
				C(Foreground)
				C(MouseIgnore)
				C(IgnoreFoW)

				C(BackgroundOrForeground)
			.endNamespace()

			.addFunction("Call", &LuaScriptFn::Call)
			.addFunction("GameOver", &LuaScriptFn::GameOver)
			.addFunction("ScriptGo", &LuaScriptFn::ScriptGo)

			.beginNamespace("Environment")
				.addProperty("Gravity", &LuaScriptFn::GetGravity, &LuaScriptFn::SetGravity)

				.addFunction("Bubble", &LuaScriptFn::Bubble)
				.addFunction("Smoke", &LuaScriptFn::Smoke)
			.endNamespace()

			.beginNamespace("Landscape")
				.addFunction("BlastFree", &LuaScriptFn::BlastFree)
				.addFunction("BurnMaterial", &LuaScriptFn::BurnMaterial)
				.addFunction("ExtractLiquid", &LuaScriptFn::ExtractLiquid)
				.addFunction("GetMaterial", &LuaScriptFn::GetMaterial)
				.addFunction("GetMaterialIndex", &LuaScriptFn::GetMaterialIndex)
				.addFunction("GetTexture", &LuaScriptFn::GetTexture)
				.addFunction("Incinerate", &LuaScriptFn::IncinerateLandscape)
				.addFunction("IsSolid", &LuaScriptFn::GBackSolid)
				.addFunction("IsSemiSolid", &LuaScriptFn::GBackSemiSolid)
				.addFunction("IsLiquid", &LuaScriptFn::GBackLiquid)
				.addFunction("IsSky", &LuaScriptFn::GBackIFT)
				.addFunction("PlaceAnimal", &LuaScriptFn::PlaceAnimal)
				.addFunction("PlaceVegetation", &LuaScriptFn::PlaceVegetation)
			.endNamespace()

			.beginNamespace("Message")
				.addFunction("Add", &LuaScriptFn::AddMessage)
			.endNamespace()

			.beginNamespace("Music")
				.addFunction("__call", &LuaScriptFn::Music)
				.addFunction("SetLevel", &LuaScriptFn::MusicLevel)
				.addFunction("SetPlaylist", &LuaScriptFn::SetPlaylist)
			.endNamespace()

			.beginNamespace("Objects")
				.addFunction("Count", &LuaScriptFn::ObjectCount)
				.addFunction("Create", &LuaScriptFn::CreateObject)
				.addFunction("FindMany", &LuaScriptFn::FindObjects)
				.addFunction("Find", &LuaScriptFn::FindObject)
			.endNamespace()

			.addProperty("Players", &LuaScriptFn::GetPlayers, nullptr)

			.beginNamespace("PXS")
				.addFunction("Cast", &LuaScriptFn::CastPXS)
			.endNamespace()

			.beginNamespace("Sound")
				.addFunction("__call", &LuaScriptFn::Sound)
				.addFunction("SetLevel", &LuaScriptFn::SoundLevel)
			.endNamespace()

			.beginNamespace("System")
				.addFunction("GainMissionAccss", &LuaScriptFn::GainMissionAccess)
			.endNamespace()


		.endNamespace()

		.beginNamespace("ObjectStatus")
			.addProperty("Normal",   const_cast<int32_t *>(&C4OS_NORMAL),   false)
			.addProperty("Inactive", const_cast<int32_t *>(&C4OS_INACTIVE), false)
			.addProperty("Deleted",  const_cast<int32_t *>(&C4OS_DELETED),  false)
		.endNamespace()


#undef PREFIX
#define PREFIX DIR
		.beginNamespace("Direction")
			C(None)
			C(Left)
			C(Right)
		.endNamespace()


#undef PREFIX
#define PREFIX COMD
		.beginNamespace("ComDir")
			C(None)
			C(Stop)
			C(Up)
			C(UpRight)
			C(Right)
			C(DownRight)
			C(Down)
			C(DownLeft)
			C(Left)
			C(UpLeft)
		.endNamespace()

#undef PREFIX
#define PREFIX VIS
		.beginNamespace("Visibility")
			C(All)
			C(None)
			C(Owner)
			C(Allies)
			C(Enemies)
			C(Local)
			C(God)
			C(LayerToggle)
			C(OverlayOnly)
		.endNamespace()

		// classes
		.beginClass<C4Action>("C4Action")
			.addProperty("Name", &C4Action::Name)
			.addProperty("Direction", &C4Action::Dir, false)
			.addProperty("ComDir", &C4Action::ComDir)
			.addProperty("Target", &C4Action::Target)
			.addProperty("Target2", &C4Action::Target2)
			.addProperty("Phase", &C4Action::Phase)
			.addProperty("PhaseDelay", &C4Action::PhaseDelay)
			.addProperty("Data", &C4Action::Data)

			.addProperty("BridgeData", &LuaScriptFn::GetBridgeActionData, &LuaScriptFn::SetBridgeActionData)
			.addProperty("Data", &LuaScriptFn::GetActionData, &LuaScriptFn::SetActionData)
		.endClass()

		.beginClass<LuaScriptFn::C4AulFuncPtr>("C4AulFunc")
			.addFunction("__call", &LuaScriptFn::C4AulFunc::__call)
		.endClass()

		.beginClass<LuaScriptFn::C4DefPtr>("C4Def")
			.addProperty("Name", &LuaScriptFn::C4Def::GetName, &LuaScriptFn::C4Def::SetName)
			.addProperty("Shape", &LuaScriptFn::C4Def::GetShape)
			.addProperty("Entrance", &LuaScriptFn::C4Def::GetEntrance)
			.addProperty("Collection", &LuaScriptFn::C4Def::GetCollection)
			.addProperty("PictureRect", &LuaScriptFn::C4Def::GetPictureRect)
			.addProperty("SolidMask", &LuaScriptFn::C4Def::GetSolidMask)
			.addProperty("TopFace", &LuaScriptFn::C4Def::GetTopFace)
			.addProperty("GrowthType", &LuaScriptFn::C4Def::GetGrowthType)
			.addProperty("Basement", &LuaScriptFn::C4Def::GetBasement)
			.addProperty("CanBeBase", &LuaScriptFn::C4Def::GetCanBeBase)
		.endClass()

		.beginClass<C4IDList>("C4IDList")
			.addFunction("__index", &LuaScriptFn::C4IDList::__index)
			.addFunction("__newindex", &LuaScriptFn::C4IDList::__newindex)
			.addFunction("__len", &LuaScriptFn::C4IDList::__len)
		.endClass()

		.beginClass<C4MaterialCore>("C4MaterialCore")
			.addProperty("Name", &C4MaterialCore::Name, false)

			.addProperty("MapChunKType", &C4MaterialCore::MapChunkType, false)
			.addProperty("Density", &C4MaterialCore::Density, false)
			.addProperty("Friction", &C4MaterialCore::Friction, false)
			.addProperty("DigFree", &C4MaterialCore::DigFree, false)
			.addProperty("BlastFree", &C4MaterialCore::BlastFree, false)
			.addProperty("Dig2Object", &C4MaterialCore::Dig2Object, false)
			.addProperty("Dig2ObjectRatio", &C4MaterialCore::Dig2ObjectRatio, false)
			.addProperty("Dig2ObjectOnRequestOnly", &C4MaterialCore::Dig2ObjectOnRequestOnly, false)
			.addProperty("Blast2Object", &C4MaterialCore::Blast2Object, false)
			.addProperty("Blast2ObjectRatio", &C4MaterialCore::Blast2ObjectRatio, false)
			.addProperty("Blast2PXSRatio", &C4MaterialCore::Blast2PXSRatio, false)
			.addProperty("Unstable", &C4MaterialCore::Instable, false)
			.addProperty("MaxAirSpeed", &C4MaterialCore::MaxAirSpeed, false)
			.addProperty("MaxSlide", &C4MaterialCore::MaxSlide, false)
			.addProperty("WindDrift", &C4MaterialCore::WindDrift, false)
			.addProperty("Inflammable", &C4MaterialCore::Inflammable, false)
			.addProperty("Incindiary", &C4MaterialCore::Incindiary, false)
			.addProperty("Extinguisher", &C4MaterialCore::Extinguisher, false)
			.addProperty("Corrosive", &C4MaterialCore::Corrosive, false)
			.addProperty("Corrode", &C4MaterialCore::Soil, false)
			.addProperty("Placement", &C4MaterialCore::Placement, false)
			.addProperty("TextureOverlay", &C4MaterialCore::TextureOverlay, false)
			.addProperty("OverlayType", &C4MaterialCore::OverlayType, false)
			.addProperty("PXSGfx", &C4MaterialCore::PXSGfx, false)

			.addProperty("PXSGfxSize", &C4MaterialCore::PXSGfxSize, false)
			.addProperty("BlastShiftTo", &LuaScriptFn::C4MaterialCore::GetBlastShiftTo)
			.addProperty("InMatConvert", &LuaScriptFn::C4MaterialCore::GetInMatConvert)
			.addProperty("InMatConvertTo", &LuaScriptFn::C4MaterialCore::GetInMatConvertTo)
			.addProperty("BelowTempConvertTo", &LuaScriptFn::C4MaterialCore::GetBelowTempConvertTo)
			.addProperty("AboveTempConvertTo", &LuaScriptFn::C4MaterialCore::GetAboveTempConvertTo)
		.endClass()

		.deriveClass<C4Material, C4MaterialCore>("C4Material")
			.addFunction("GetCount", &LuaScriptFn::C4Material::GetMaterialCount)
			.addFunction("Insert", &LuaScriptFn::C4Material::InsertMaterial)
			.addFunction("Extract", &LuaScriptFn::C4Material::ExtractMaterialAmount)
		.endClass()

		.beginClass<LuaScriptFn::C4ObjectPtr>("C4Object")
			.addFunction("__newindex", &LuaScriptFn::C4Object::__newindex)
			.addFunction("__index", &LuaScriptFn::C4Object::__index)
			.addFunction("__tostring", &LuaScriptFn::C4Object::__tostring)

			.addFunction("Explode", &LuaScriptFn::Explode)
			.addFunction("Incinerate", &LuaScriptFn::Incinerate)
			.addFunction("DeathAnnounce", &LuaScriptFn::DeathAnnounce)
			.addFunction("GrabContents", &LuaScriptFn::GrabContents)
			.addFunction("Punch", &LuaScriptFn::Punch)
			.addFunction("Kill", &LuaScriptFn::Kill)
			.addFunction("Fling", &LuaScriptFn::Fling)
			.addFunction("Jump", &LuaScriptFn::Jump)
			.addFunction("Enter", &LuaScriptFn::Enter)
			.addFunction("Exit", &LuaScriptFn::Exit)
			.addFunction("Collect", &LuaScriptFn::Collect)
			.addFunction("Split2Components", &LuaScriptFn::Split2Components)
			.addFunction("Remove", &LuaScriptFn::RemoveObject)
			.addFunction("SetPosition", &LuaScriptFn::SetPosition)
			.addFunction("DoCon", &LuaScriptFn::DoCon)
			.addFunction("DoEnergy", &LuaScriptFn::DoEnergy)
			.addFunction("DoBreath", &LuaScriptFn::DoBreath)
			.addFunction("DoDamage", &LuaScriptFn::DoDamage)
			.addFunction("DoMagicEnergy", &LuaScriptFn::DoEnergy)
			.addFunction("GetMagicEnergy", &LuaScriptFn::GetMagicEnergy)
			.addFunction("SetPhysical", &LuaScriptFn::SetPhysical)
			.addFunction("TrainPhysical", &LuaScriptFn::TrainPhysical)
			.addFunction("ResetPhysical", &LuaScriptFn::ResetPhysical)
			.addFunction("GetPhysical", &LuaScriptFn::GetPhysical)
			.addFunction("ExecuteCommand", &LuaScriptFn::ExecuteCommand)
			.addFunction("SetCommand", &LuaScriptFn::SetCommand)
			.addFunction("AddCommand", &LuaScriptFn::AddCommand)
			.addFunction("AppendCommand", &LuaScriptFn::AppendCommand)
			.addFunction("GetCommand", &LuaScriptFn::GetCommand)
			.addFunction("FinishCommand", &LuaScriptFn::FinishCommand)
			.addFunction("SetName", &LuaScriptFn::FnSetName)
			.addFunction("GrabObjectInfo", &LuaScriptFn::GrabObjectInfo)
			.addFunction("BlastObjects", &LuaScriptFn::BlastObjects)
			.addFunction("BlastObject", &LuaScriptFn::BlastObject)
			.addFunction("Call", &LuaScriptFn::ObjectCall)

			.addProperty("Name", &LuaScriptFn::GetName, &LuaScriptFn::SetName)
			//.addProperty("Description") <- inherited by C4Object::Def.LuaDef
			.addProperty("X", &LuaScriptFn::GetX)
			.addProperty("Y", &LuaScriptFn::GetY)
			.addProperty("R", &LuaScriptFn::GetR, &LuaScriptFn::SetR)
			.addProperty("Con", &LuaScriptFn::GetCon, &LuaScriptFn::SetCon)
			.addProperty("EntranceStatus", &LuaScriptFn::GetEntrance, &LuaScriptFn::SetEntrance)
			.addProperty("XDir", &LuaScriptFn::GetXDir, &LuaScriptFn::SetXDir)
			.addProperty("YDir", &LuaScriptFn::GetYDir, &LuaScriptFn::SetYDir)
			.addProperty("RDir", &LuaScriptFn::GetRDir, &LuaScriptFn::SetRDir)
			.addProperty("Action", &LuaScriptFn::GetAction)
			.addProperty("Direction", &LuaScriptFn::GetDir, &LuaScriptFn::SetDir)
			//.addProperty("Alive", &C4Object::GetAlive, &C4Object::SetAlive)
			.addProperty("Owner", &LuaScriptFn::GetOwner, &LuaScriptFn::SetOwner)
			.addProperty("Controller", &LuaScriptFn::GetController, &LuaScriptFn::SetController)
			.addProperty("Killer", &LuaScriptFn::GetKiller, &LuaScriptFn::SetKiller)
			.addProperty("OCF", &LuaScriptFn::GetOCF)
		.endClass()

		.beginClass<LuaScriptFn::C4PlayerInfoCorePtr>("C4PlayerInfoCore")
		.endClass()

		.deriveClass<LuaScriptFn::C4PlayerPtr, LuaScriptFn::C4PlayerInfoCorePtr>("C4Player")
			.addProperty("Name", &LuaScriptFn::C4Player::GetName)
			.addProperty("TaggedName", &LuaScriptFn::C4Player::GetTaggedPlayerName)
			.addProperty("Status", &LuaScriptFn::C4Player::GetStatus)
			.addProperty("Eliminated", &LuaScriptFn::C4Player::GetEliminated)
			.addProperty("Surrendered", &LuaScriptFn::C4Player::GetSurrendered)
			.addProperty("Evaluated", &LuaScriptFn::C4Player::GetEvaluated)
			.addProperty("Number", &LuaScriptFn::C4Player::GetNumber)
			.addProperty("ID", &LuaScriptFn::C4Player::GetID)
			.addProperty("Team", &LuaScriptFn::C4Player::GetTeam)
			.addProperty("Color", &LuaScriptFn::C4Player::GetColorDw, &LuaScriptFn::C4Player::SetColorDw)
			.addProperty("Control", &LuaScriptFn::C4Player::GetControl)
			.addProperty("MouseControl", &LuaScriptFn::C4Player::GetMouseControl)
			.addProperty("PlayerStartIndex", &LuaScriptFn::C4Player::GetPlrStartIndex)
			.addProperty("ClientName", &LuaScriptFn::C4Player::GetAtClientName)
			.addProperty("Wealth", &LuaScriptFn::C4Player::GetWealth, &LuaScriptFn::C4Player::SetWealth)
			.addProperty("Points", &LuaScriptFn::C4Player::GetPoints, &LuaScriptFn::C4Player::SetPoints)
			.addProperty("Value", &LuaScriptFn::C4Player::GetValue, &LuaScriptFn::C4Player::SetValue)
			.addProperty("InitialValue", &LuaScriptFn::C4Player::GetInitialValue)
			.addProperty("ValueGain", &LuaScriptFn::C4Player::GetValueGain)
			.addProperty("ObjectsOwned", &LuaScriptFn::C4Player::GetObjectsOwned)
			.addProperty("ShowControl", &LuaScriptFn::C4Player::GetShowControl)
			.addProperty("ShowControlPosition", &LuaScriptFn::C4Player::GetShowControlPos, &LuaScriptFn::C4Player::SetShowControlPos)
			.addProperty("FlashCommand", &LuaScriptFn::C4Player::GetFlashCom, &LuaScriptFn::C4Player::SetFlashCom)
			.addProperty("Captain", &LuaScriptFn::C4Player::GetCaptain)
			.addProperty("AutoContextMenu", &LuaScriptFn::C4Player::GetAutoContextMenu, &LuaScriptFn::C4Player::SetAutoContextMenu)
			.addProperty("JumpAndRunControl", &LuaScriptFn::C4Player::GetControlStyle, &LuaScriptFn::C4Player::SetControlStyle)
			.addProperty("LastCommand", &LuaScriptFn::C4Player::GetLastCom)
			.addProperty("LastCommandDelay", &LuaScriptFn::C4Player::GetLastComDelay)
			.addProperty("LastCommandDownDouble", &LuaScriptFn::C4Player::GetLastComDownDouble)
			.addProperty("Type", &LuaScriptFn::C4Player::GetType)
			.addProperty("Cursor", &LuaScriptFn::C4Player::GetCursor, &LuaScriptFn::C4Player::SetCursor)
			.addProperty("ActiveCrewCount", &LuaScriptFn::C4Player::GetActiveCrewCount)
			.addProperty("SelectedCrewCount", &LuaScriptFn::C4Player::GetSelectedCrewCount)
			.addProperty("ViewTarget", &LuaScriptFn::C4Player::GetPlayerView, &LuaScriptFn::C4Player::SetPlayerView)

			.addFunction("Eliminate", &LuaScriptFn::C4Player::Eliminate)
			.addFunction("Surrender", &LuaScriptFn::C4Player::Surrender)
			.addFunction("DoWealth", &LuaScriptFn::C4Player::DoWealth)
			.addFunction("SetFoW", &LuaScriptFn::C4Player::SetFoW)
			.addFunction("MakeCrewMember", &LuaScriptFn::C4Player::MakeCrewMember)
			.addFunction("HostileTo", &LuaScriptFn::C4Player::HostileTo)
			.addFunction("SetHostility", &LuaScriptFn::C4Player::SetHostility)
			.addFunction("GetControlName", &LuaScriptFn::C4Player::GetPlayerControlName)

		.endClass();

		/*.beginClass<C4Rect>("C4Rect")
			.addProperty("X", &C4Rect::x)
			.addProperty("Y", &C4Rect::y)
			.addProperty("Width", &C4Rect::Wdt)
			.addProperty("Height", &C4Rect::Hgt)
			.addProperty("MiddleX", &C4Rect::GetMiddleX)
			.addProperty("MiddleY", &C4Rect::GetMiddleY)
			.addProperty("Bottom", &C4Rect::GetBottom)

			.addFunction("Set", &C4Rect::Set)
			.addFunction("__eq", &C4Rect::operator==)
			.addFunction("Contains", static_cast<bool (C4Rect::*)(const C4Rect &)>(&C4Rect::Contains))
			.addFunction("IntersectsLine", &C4Rect::IntersectsLine)
			.addFunction("Normalize", &C4Rect::Normalize)
			.addFunction("Enlarge", &C4Rect::Enlarge)
		.endClass()

		.deriveClass<C4TargetRect, C4Rect>("C4TargetRect")
			.addProperty("TargetX", &C4TargetRect::tx)
			.addProperty("TargetY", &C4TargetRect::ty)

			.addFunction("Set", &C4TargetRect::Set)
			.addFunction("ClipBy", &C4TargetRect::ClipBy)
		.endClass()

		.deriveClass<C4Shape, C4Rect>("C4Shape")
			.addProperty("FireTop", &C4Shape::FireTop)
			.addProperty("ContactDensity", &C4Shape::ContactDensity)
			.addProperty("ContactCNAT", &C4Shape::ContactCNAT)
			.addProperty("ContactCount", &C4Shape::ContactCount)

			.addFunction("ContactCheck", &C4Shape::ContactCheck)
		.endClass();*/
#undef PREFIX
#undef CONCAT
#undef CONCAT2
#undef C
	return true;
}

void C4LuaScriptEngine::Clear()
{
	C4Lua::Clear();
	lines = warnings = errors = 0;
}

luabridge::LuaRef C4LuaScriptEngine::Evaluate(const std::string &script)
{
	assert(L);
	StdStrBuf line = FormatString("return %s;", script.c_str());
	switch (luaL_loadbufferx(L, line.getData(), line.getSize() - 1, "Evaluate", "t"))
	{
	case LUA_ERRSYNTAX:
		LogErrorF("%s", lua_tostring(L, -1));
		break;
	case LUA_ERRMEM:
		LogFatal("Out of memory");
		return luabridge::LuaRef(L);
	default:
		break;
	}

	if (lua_pcall(L, 0, 1, 0) != LUA_OK)
	{
		LogErrorF("%s", lua_tostring(L, -1));
		lua_pop(L, 1);
		return LuaNil(L);
	}

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

	lines += static_cast<size_t>(SGetLine(buf.getData(), buf.getPtr(buf.getLength())));

	switch (luaL_loadbufferx(L, buf.getData(), buf.getSize() - 1, filename, "t"))
	{
	case LUA_ERRSYNTAX:
		LogErrorF("Syntax error: %s", lua_tostring(L, -1));
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
		LogErrorF("%s", lua_tostring(L, -1));
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
	LogF("C4LuaScriptEngine linked - %zu line%s, %zu warning%s, %zu error%s",
		 lines,    lines    != 1 ? "s" : "",
		 warnings, warnings != 1 ? "s" : "",
		 errors,   errors   != 1 ? "s" : "");
}

void C4LuaScriptEngine::LogErrorF(const char *error, ...)
{
	if (!Game.DebugMode) return;

	va_list args;
	va_start(args, error);
	StdStrBuf buf;
	buf.Copy("ERROR: ");
	buf.AppendFormatV(error, args);
	va_end(args);

	std::istringstream stream(buf.getData());
	std::string line;
	for (auto i = 0; std::getline(stream, line, '\n'); ++i)
	{
		DebugLogF("%s", line.c_str());
		if (i == 0)
		{
			Game.Messages.New(C4GM_Global, StdStrBuf(line.c_str()), nullptr, ANY_OWNER);
		}
	}
}
