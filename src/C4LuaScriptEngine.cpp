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
#include "C4Aul.h"
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

#include <sstream>
#include <cstring>

namespace LuaHelpers
{
C4Object *Number2Object(int number)
{
	return Game.Objects.ObjectPointer(number);
}

int32_t GetPlayerNumber(DeletableObjectPtr<C4Player> *player)
{
	return player  ? (*player)->Number : NO_OWNER;
}

C4ID GetIDFromDef(luabridge::LuaRef def)
{
	C4ID id;
	if (def["ID"].isNumber())
	{
		id = def["ID"];
	}
	else if (def["ID"].isString())
	{
		std::string i = def["ID"];
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
	else
	{
		id = std::hash<luabridge::LuaRef>()(def);
	}
	return id;
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

luabridge::LuaRef RegisterDefinition(luabridge::LuaRef table)
{
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
			luaL_error(table.state(), "Definition has invalid ID: %s", i.c_str());
			return LuaNil(table.state());
		}
	}
	else
	{
		id = std::hash<luabridge::LuaRef>()(table);
	}
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

luabridge::LuaRef CreateObject(luabridge::LuaRef arguments, lua_State *L)
{
	luabridge::LuaRef table = arguments["Def"];
	if (!table.isTable())
	{
		return LuaHelpers::error(L, FormatString("Definition is not a table (type: %s)", lua_typename(table.state(), table.type())).getData());
	}
	else if (!table["Name"].isString() || table["Name"].tostring().empty())
	{
		return LuaHelpers::error(L, "Definition has no name");
	}

	table = RegisterDefinition(table);

	int32_t x = arguments["X"].isNumber() ? arguments["X"] : 0;
	int32_t y = arguments["Y"].isNumber() ? arguments["Y"] : 0;
	int32_t r = arguments["R"].isNumber() ? arguments["R"] : 0;

	FIXED xdir = arguments["XDir"].isNumber() ? ftofix(arguments["XDir"].cast<float>()) : Fix0;
	FIXED ydir = arguments["YDir"].isNumber() ? ftofix(arguments["YDir"].cast<float>()) : Fix0;
	FIXED rdir = arguments["RDir"].isNumber() ? ftofix(arguments["RDir"].cast<float>()) : Fix0;

	int32_t con = arguments["Con"].isNumber() ? arguments["Con"] : FullCon / 100;

	C4PlayerPtr *owner = !arguments["Owner"].isNil() ? arguments["Owner"] : nullptr;
	C4PlayerPtr *controller = !arguments["Controller"].isNil() ? arguments["Controller"] : owner;

	//auto *objectinfo = !arguments["ObjectInfo"].isNil() ? arguments["ObjectInfo"].cast<C4ObjectInfoPtr *>() : nullptr;

	C4ObjectPtr *creator = !arguments["Creator"].isNil() ? arguments["Creator"] : nullptr;

	C4Object *obj = Game.NewObject(
				Game.Defs.ID2Def(table["ID"].cast<C4ID>()),
				creator  ? *creator : nullptr,
				owner  ? (*owner)->Number : NO_OWNER,
				nullptr,
				x, y, r,
				xdir, ydir, rdir,
				FullCon * con / 100,
				controller  ? (*controller)->Number : NO_OWNER
				);

	return luabridge::LuaRef(L, LuaHelpers::ref(L, obj));
}

void Explode(C4ObjectPtr *obj, int32_t level, lua_State *L) // opt: luabridge::LuaRef effect, std::string particle
{
	if (!obj) return;
	C4ID id = 0L;
	auto effect = luabridge::LuaRef::fromStack(L, 3);
	if (effect.isTable())
	{
		RegisterDefinition(effect);
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

bool IncinerateLandscape(int32_t x, int32_t y)
{
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
	bool forced = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) : false;
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
	bool addSpeed = lua_gettop(L) > 3 ? lua_toboolean(L, 4) : false;

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
	(*obj)->AssignRemoval(lua_gettop(L) >= 2 ? lua_toboolean(L, 2) : false);
}

void SetPosition(C4ObjectPtr *obj, int32_t x, int32_t y, lua_State *L) // opt: bool checkBounds
{
	if (!obj) return;
	LuaHelpers::CallC4Script(obj, &FnSetPosition,
							 static_cast<long>(x),
							 static_cast<long>(y),
							 static_cast<C4Object *>(nullptr),
							 static_cast<bool>(lua_gettop(L) >= 4 ? lua_toboolean(L, 4) : false)
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
							 static_cast<bool>(lua_gettop(L) >= 3 ? lua_toboolean(L, 3) : false),
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
									static_cast<bool>(lua_gettop(L) >= 3 ? lua_toboolean(L, 3) : false)
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
							 lua_gettop(L) >= 4 ? static_cast<bool>(lua_toboolean(L, 4)) : false,
							 lua_gettop(L) >= 5 ? static_cast<bool>(lua_toboolean(L, 5)) : false
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

luabridge::LuaRef FindObjects(luabridge::LuaRef criteria)
{
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

luabridge::LuaRef FindObject(luabridge::LuaRef criteria)
{
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

int32_t ObjectCount(luabridge::LuaRef criteria)
{
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

bool BurnMaterial(C4ObjectPtr *obj, int32_t x, int32_t y)
{
	if (!obj) return false;
	int32_t mat = GBackMat(x, y);
	return MatValid(mat) && Game.Material.Map[mat].Inflammable && Game.Landscape.ExtractMaterial(x, y) != MNone;
}

int32_t ExtractLiquid(int32_t x, int32_t y)
{
	return GBackLiquid(x, y) ? Game.Landscape.ExtractMaterial(x, y) : MNone;
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

luabridge::LuaRef __call(C4AulFuncPtr *func, lua_State *L)
{
	if (!func) return LuaNil(L);
	func->checkObject();
	int32_t argstart = 0;
	C4ObjectPtr *obj = nullptr;
	if (luaL_testudata(L, 1, "C4Object"))
	{
		obj = luabridge::LuaRef::fromStack(L, 1);
		++argstart;
	}
	if (lua_gettop(L) > C4AUL_MAX_Par + argstart)
	{
		return LuaHelpers::error(L, "Too many arguments supplied (%d / %d)", lua_gettop(L), C4AUL_MAX_Par);
	}
	C4AulParSet pars;
	for (int32_t arg = argstart + 1; arg <= lua_gettop(L); ++arg)
	{
		pars[arg - argstart - 1] = luabridge::Stack<C4Value>::get(L, arg);
	}
	return luabridge::LuaRef(L, (*func)->Exec(obj  ? *obj : nullptr, &pars));
}
}

// C4Material
namespace C4Material
{
#define MAT(x) C4MaterialCore *Get##x(const C4MaterialCore *mat) \
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

// C4ObjectPtr

namespace C4Object
{
void __newindex(C4ObjectPtr *obj, std::string key, luabridge::LuaRef value)
{
	if (!obj) return;
	(*obj)->LuaLocals.emplace(key, value);
}

luabridge::LuaRef __index(C4ObjectPtr *obj, std::string key, lua_State *L)
{
	if (!obj) return LuaNil(L);
	{
		int index = lua_gettop(L);
		luabridge::push(L, obj);
		lua_getmetatable(L, -1);
		lua_getfield(L, -1, "__index_old");
		auto ref = luabridge::LuaRef::fromStack(L);
		lua_settop(L, index);
		luabridge::LuaRef value = ref(obj, key);
		if (!value.isNil())
		{
			return value;
		}
	}

	auto i = (*obj)->LuaLocals.find(key);
	if (i != (*obj)->LuaLocals.end())
	{
		return i->second;
	}

	C4Value *value = (*obj)->LocalNamed.GetItem(key.c_str());
	if (value)
	{
		return luabridge::LuaRef(L, value);
	}

	if (!(*obj)->Def->LuaDef.isNil() && !(*obj)->Def->LuaDef[key].isNil())
	{
		return (*obj)->Def->LuaDef[key];
	}
	else
	{
		::C4AulFunc *f = (*obj)->Def->Script.GetSFunc(key.c_str(), AA_PROTECTED, true);
		if (f)
		{
			return luabridge::LuaRef(L, LuaHelpers::ref(L, f));
		}
	}
	return LuaNil(L);
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
GET(uint32_t, ColorDw)
GET(int32_t, Control)
GET(int32_t, MouseControl)
GET(int32_t, PlrStartIndex)
PROPERTY(int32_t, Wealth)
PROPERTY(int32_t, Points)
PROPERTY(int32_t, Value)
GET(int32_t, InitialValue)
GET(int32_t, ValueGain)
GET(int32_t, ObjectsOwned)

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

GET(bool, AutoContextMenu)
GET(int32_t, ControlStyle)

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

bool MakeCrewMember(C4PlayerPtr *player, C4ObjectPtr *obj)
{
	if (!player || !obj) return false;
	return (*player)->MakeCrewMember((*obj));
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

			.addFunction("CreateObject", &LuaScriptFn::CreateObject)
			.addFunction("FindObjects", &LuaScriptFn::FindObjects)
			.addFunction("FindObject", &LuaScriptFn::FindObject)
			.addFunction("ObjectCount", &LuaScriptFn::ObjectCount)

			.beginNamespace("Environment")
				.addProperty("Gravity", &LuaScriptFn::GetGravity, &LuaScriptFn::SetGravity)
				.addFunction("Smoke", &Smoke)
				.addFunction("Bubble", &BubbleOut)
			.endNamespace()

			.beginNamespace("Landscape")
				.addFunction("BurnMaterial", &LuaScriptFn::BurnMaterial)
				.addFunction("Incinerate", &LuaScriptFn::IncinerateLandscape)
			.endNamespace()

			.addProperty("Players", &LuaScriptFn::GetPlayers, nullptr)
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
			.addProperty("BlastShiftTo", &LuaScriptFn::C4Material::GetBlastShiftTo)
			.addProperty("InMatConvert", &LuaScriptFn::C4Material::GetInMatConvert)
			.addProperty("InMatConvertTo", &LuaScriptFn::C4Material::GetInMatConvertTo)
			.addProperty("BelowTempConvertTo", &LuaScriptFn::C4Material::GetBelowTempConvertTo)
			.addProperty("AboveTempConvertTo", &LuaScriptFn::C4Material::GetAboveTempConvertTo)
		.endClass()

		.beginClass<LuaScriptFn::C4ObjectPtr>("C4Object")
			.addFunction("__newindex", &LuaScriptFn::C4Object::__newindex)
			.addFunction("__index_old", &LuaScriptFn::C4Object::__index)

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
			.addProperty("Color", &LuaScriptFn::C4Player::GetColorDw)
			.addProperty("Control", &LuaScriptFn::C4Player::GetControl)
			.addProperty("MouseControl", &LuaScriptFn::C4Player::GetMouseControl)
			.addProperty("PlayerStartIndex", &LuaScriptFn::C4Player::GetPlrStartIndex)
			//.addProperty("Client", &LuaScriptFn::C4Player::GetAtClientName)
			.addProperty("Wealth", &LuaScriptFn::C4Player::GetWealth, &LuaScriptFn::C4Player::SetWealth)
			.addProperty("Points", &LuaScriptFn::C4Player::GetPoints, &LuaScriptFn::C4Player::SetPoints)
			.addProperty("Value", &LuaScriptFn::C4Player::GetValue, &LuaScriptFn::C4Player::SetValue)
			.addProperty("InitialValue", &LuaScriptFn::C4Player::GetInitialValue)
			.addProperty("ValueGain", &LuaScriptFn::C4Player::GetValueGain)
			.addProperty("ObjectsOwned", &LuaScriptFn::C4Player::GetObjectsOwned)
			.addProperty("Captain", &LuaScriptFn::C4Player::GetCaptain)
			.addProperty("AutoContextMenu", &LuaScriptFn::C4Player::GetAutoContextMenu)
			.addProperty("ControlStyle", &LuaScriptFn::C4Player::GetControlStyle)
			.addProperty("Type", &LuaScriptFn::C4Player::GetType)
			.addProperty("Cursor", &LuaScriptFn::C4Player::GetCursor, &LuaScriptFn::C4Player::SetCursor)
			//.addProperty("ActiveCrewCount", &LuaScriptFn::C4Player::GetActiveCrewCount)
			//.addProperty("SelectedCrewCount", &LuaScriptFn::C4Player::GetGetSelectedCrewCount)

			/*.addFunction("Eliminate", &LuaScriptFn::C4Player::GetEliminate)
			.addFunction("Surrender", &LuaScriptFn::C4Player::GetSurrender)
			.addFunction("DoWealth", &LuaScriptFn::C4Player::GetDoWealth)
			.addFunction("SetFoW", &LuaScriptFn::C4Player::GetSetFoW)*/
			.addFunction("MakeCrewMember", &LuaScriptFn::C4Player::MakeCrewMember)
		.endClass();
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
	switch (luaL_loadbufferx(L, script.c_str(), script.size(), "Evaluate", "t"))
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
	try
	{
		luabridge::LuaException::pcall(L, 0, 1, 0);
	}
	catch (luabridge::LuaException const &e)
	{
		LogErrorF("%s", e.what());
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
