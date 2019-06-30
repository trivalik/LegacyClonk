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

/* Object definition */

#include <C4Include.h>
#include <C4Def.h>
#include <C4Version.h>
#include <C4GameVersion.h>
#include <C4FileMonitor.h>

#include <C4SurfaceFile.h>
#include <C4Log.h>
#include <C4Components.h>
#include <C4Config.h>
#include <C4ValueList.h>
#include <C4Wrappers.h>

#ifdef C4ENGINE
#include <C4Object.h>
#include "C4Network2Res.h"
#endif

#ifdef C4GROUP
#include "C4Group.h"
#include "C4Scenario.h"
#include "C4CompilerWrapper.h"
#endif

// Default Action Procedures

const char *ProcedureName[C4D_MaxDFA] =
{
	"WALK",
	"FLIGHT",
	"KNEEL",
	"SCALE",
	"HANGLE",
	"DIG",
	"SWIM",
	"THROW",
	"BRIDGE",
	"BUILD",
	"PUSH",
	"CHOP",
	"LIFT",
	"FLOAT",
	"ATTACH",
	"FIGHT",
	"CONNECT",
	"PULL"
};

// C4ActionDef

C4ActionDef::C4ActionDef()
{
	Default();
}

void C4ActionDef::Default()
{
	Name = ProcedureName = NextActionName = InLiquidAction = TurnAction = Sound = "";
	Procedure = DFA_NONE;
	NextActionName = "";
	NextAction = ActIdle;
	Directions = 1;
	FlipDir = 0;
	Length = 1;
	Delay = 0;
	Attach = 0;
	FacetBase = 0;
	FacetTopFace = 0;
	NoOtherAction = 0;
	Disabled = 0;
	DigFree = 0;
	FacetTargetStretch = 0;
	EnergyUsage = 0;
	Reverse = 0;
	Step = 1;
	SStartCall = SPhaseCall = SEndCall = SAbortCall = "";
	StartCall = PhaseCall = EndCall = AbortCall = nullptr;
}

void C4ActionDef::CompileFunc(StdCompiler *pComp)
{
	pComp->Value(mkNamingAdapt(mkStringAdaptA(Name),          "Name",       ""));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(ProcedureName), "Procedure",  ""));
	pComp->Value(mkNamingAdapt(Directions,                    "Directions", 1));
	pComp->Value(mkNamingAdapt(FlipDir,                       "FlipDir",    0));
	pComp->Value(mkNamingAdapt(Length,                        "Length",     1));

	StdBitfieldEntry<int32_t> CNATs[] =
	{
		{ "CNAT_None",        CNAT_None },
		{ "CNAT_Left",        CNAT_Left },
		{ "CNAT_Right",       CNAT_Right },
		{ "CNAT_Top",         CNAT_Top },
		{ "CNAT_Bottom",      CNAT_Bottom },
		{ "CNAT_Center",      CNAT_Center },
		{ "CNAT_MultiAttach", CNAT_MultiAttach },
		{ "CNAT_NoCollision", CNAT_NoCollision },

		{ nullptr, 0 }
	};

	pComp->Value(mkNamingAdapt(mkBitfieldAdapt(Attach, CNATs),
		"Attach", 0));

	pComp->Value(mkNamingAdapt(Delay,                                      "Delay",              0));
	pComp->Value(mkNamingAdapt(Facet,                                      "Facet",              TargetRect0));
	pComp->Value(mkNamingAdapt(FacetBase,                                  "FacetBase",          0));
	pComp->Value(mkNamingAdapt(FacetTopFace,                               "FacetTopFace",       0));
	pComp->Value(mkNamingAdapt(FacetTargetStretch,                         "FacetTargetStretch", 0));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(NextActionName),             "NextAction",         ""));
	pComp->Value(mkNamingAdapt(NoOtherAction,                              "NoOtherAction",      0));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(SStartCall),                 "StartCall",          ""));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(SEndCall),                   "EndCall",            ""));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(SAbortCall),                 "AbortCall",          ""));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(SPhaseCall),                 "PhaseCall",          ""));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(Sound),                      "Sound",              ""));
	pComp->Value(mkNamingAdapt(Disabled,                                   "ObjectDisabled",     0));
	pComp->Value(mkNamingAdapt(DigFree,                                    "DigFree",            0));
	pComp->Value(mkNamingAdapt(EnergyUsage,                                "EnergyUsage",        0));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(InLiquidAction),             "InLiquidAction",     ""));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(TurnAction),                 "TurnAction",         ""));
	pComp->Value(mkNamingAdapt(Reverse,                                    "Reverse",            0));
	pComp->Value(mkNamingAdapt(Step,                                       "Step",               1));
}

// C4DefCore

C4DefCore::C4DefCore() : LuaDef(Game.LuaEngine.state())
{
	Default();
}

void C4DefCore::Default()
{
	rC4XVer[0] = rC4XVer[1] = rC4XVer[2] = rC4XVer[3] = 0;
	RequireDef.Clear();
	LuaDef = luabridge::LuaRef(Game.LuaEngine.state());
	Name = "Undefined";
	Physical.Default();
	Shape.Default();
	Entrance.Default();
	Collection.Default();
	PictureRect.Default();
	SolidMask.Default();
	TopFace.Default();
	Component.Default();
	BurnTurnTo = C4ID_None;
	BuildTurnTo = C4ID_None;
	STimerCall.clear();
	Timer = 35;
	ColorByMaterial.clear();
	GrowthType = 0;
	Basement = 0;
	CanBeBase = 0;
	CrewMember = 0;
	NativeCrew = 0;
	Mass = 0;
	Value = 0;
	Exclusive = 0;
	Category = 0;
	Growth = 0;
	Rebuyable = 0;
	ContactIncinerate = 0;
	BlastIncinerate = 0;
	Constructable = 0;
	Grab = 0;
	Carryable = 0;
	Rotateable = 0;
	RotatedEntrance = 0;
	Chopable = 0;
	Float = 0;
	ColorByOwner = 0;
	NoHorizontalMove = 0;
	BorderBound = 0;
	LiftTop = 0;
	CollectionLimit = 0;
	GrabPutGet = 0;
	ContainBlast = 0;
	UprightAttach = 0;
	ContactFunctionCalls = 0;
	MaxUserSelect = 0;
	Line = 0;
	LineConnect = 0;
	LineIntersect = 0;
	NoBurnDecay = 0;
	IncompleteActivity = 0;
	Placement = 0;
	Prey = 0;
	Edible = 0;
	AttractLightning = 0;
	Oversize = 0;
	Fragile = 0;
	NoPushEnter = 0;
	Explosive = 0;
	Projectile = 0;
	DragImagePicture = 0;
	VehicleControl = 0;
	Pathfinder = 0;
	NoComponentMass = 0;
	MoveToRange = 0;
	NoStabilize = 0;
	ClosedContainer = 0;
	SilentCommands = 0;
	NoBurnDamage = 0;
	TemporaryCrew = 0;
	SmokeRate = 100;
	BlitMode = C4D_Blit_Normal;
	NoBreath = 0;
	ConSizeOff = 0;
	NoSell = NoGet = 0;
	NoFight = 0;
	RotatedSolidmasks = 0;
	NeededGfxMode = 0;
	NoTransferZones = 0;
}

bool C4DefCore::Load(C4Group &hGroup)
{
	StdStrBuf Source;
	if (hGroup.LoadEntryString(C4CFN_DefCore, Source))
	{
		StdStrBuf Name = hGroup.GetFullName() + (const StdStrBuf &)FormatString("%cDefCore.txt", DirectorySeparator);
		if (!Compile<StdCompilerINIRead>(Source.getData(), Name.getData()))
			return false;

		Source.Clear();
		return true;
	}
	return false;
}

bool C4DefCore::Compile(luabridge::LuaRef def)
{
	LuaDef = luabridge::LuaRef(def);
	StdCompilerLuaRead comp;
	comp.setInput(def);
	try
	{
		comp.Compile(*this);
		return true;
	}
	catch (StdCompiler::Exception *e)
	{
		delete e;
		return false;
	}
}

void C4DefCore::CompileFunc(StdCompiler *pComp)
{
	pComp->Value(mkNamingAdapt(mkC4IDAdapt(id),               "id",         C4ID_None));
	pComp->Value(mkNamingAdapt(toC4CArr(rC4XVer),             "Version"));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(Name),          "Name",       "Undefined"));
	pComp->Value(mkNamingAdapt(mkParAdapt(RequireDef, false), "RequireDef", C4IDList()));

	const StdBitfieldEntry<uint32_t> Categories[] =
	{
		{ "C4D_StaticBack", C4D_StaticBack },
		{ "C4D_Structure",  C4D_Structure },
		{ "C4D_Vehicle",    C4D_Vehicle },
		{ "C4D_Living",     C4D_Living },
		{ "C4D_Object",     C4D_Object },

		{ "C4D_Goal",             C4D_Goal },
		{ "C4D_Environment",      C4D_Environment },
		{ "C4D_SelectBuilding",   C4D_SelectBuilding },
		{ "C4D_SelectVehicle",    C4D_SelectVehicle },
		{ "C4D_SelectMaterial",   C4D_SelectMaterial },
		{ "C4D_SelectKnowledge",  C4D_SelectKnowledge },
		{ "C4D_SelectHomebase",   C4D_SelectHomebase },
		{ "C4D_SelectAnimal",     C4D_SelectAnimal },
		{ "C4D_SelectNest",       C4D_SelectNest },
		{ "C4D_SelectInEarth",    C4D_SelectInEarth },
		{ "C4D_SelectVegetation", C4D_SelectVegetation },

		{ "C4D_TradeLiving", C4D_TradeLiving },
		{ "C4D_Magic",       C4D_Magic },
		{ "C4D_CrewMember",  C4D_CrewMember },

		{ "C4D_Rule", C4D_Rule },

		{ "C4D_Background",  C4D_Background },
		{ "C4D_Parallax",    C4D_Parallax },
		{ "C4D_MouseSelect", C4D_MouseSelect },
		{ "C4D_Foreground",  C4D_Foreground },
		{ "C4D_MouseIgnore", C4D_MouseIgnore },
		{ "C4D_IgnoreFoW",   C4D_IgnoreFoW },

		{ nullptr, 0 }
	};

	if (dynamic_cast<StdCompilerLuaRead *>(pComp))
	{
		bool b = pComp->Name("Category");
		pComp->NameEnd();
		asm("nop");
		asm("nop");
	}

	pComp->Value(mkNamingAdapt(mkBitfieldAdapt<uint32_t>(Category, Categories),
		"Category", 0));
	if (dynamic_cast<StdCompilerLuaRead *>(pComp))
	{
		asm("nop");
		asm("nop");
	}

	pComp->Value(mkNamingAdapt(MaxUserSelect,                 "MaxUserSelect",     0));
	pComp->Value(mkNamingAdapt(Timer,                         "Timer",             35));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(STimerCall),    "TimerCall",         ""));
	pComp->Value(mkNamingAdapt(ContactFunctionCalls,          "ContactCalls",      0));
	pComp->Value(mkParAdapt(Shape,                            false));
	pComp->Value(mkNamingAdapt(Value,                         "Value",             0));
	pComp->Value(mkNamingAdapt(Mass,                          "Mass",              0));
	pComp->Value(mkNamingAdapt(Component,                     "Components",        C4IDList()));
	pComp->Value(mkNamingAdapt(SolidMask,                     "SolidMask",         TargetRect0));
	pComp->Value(mkNamingAdapt(TopFace,                       "TopFace",           TargetRect0));
#ifdef C4ENGINE
	pComp->Value(mkNamingAdapt(PictureRect,                   "Picture",           Rect0));
#endif
	pComp->Value(mkNamingAdapt(Entrance,                      "Entrance",          Rect0));
	pComp->Value(mkNamingAdapt(Collection,                    "Collection",        Rect0));
	pComp->Value(mkNamingAdapt(CollectionLimit,               "CollectionLimit",   0));
	pComp->Value(mkNamingAdapt(Placement,                     "Placement",         0));
	pComp->Value(mkNamingAdapt(Exclusive,                     "Exclusive",         0));
	pComp->Value(mkNamingAdapt(ContactIncinerate,             "ContactIncinerate", 0));
	pComp->Value(mkNamingAdapt(BlastIncinerate,               "BlastIncinerate",   0));
	pComp->Value(mkNamingAdapt(mkC4IDAdapt(BurnTurnTo),       "BurnTo",            C4ID_None));
	pComp->Value(mkNamingAdapt(CanBeBase,                     "Base",              0));

	const StdBitfieldEntry<uint32_t> LineTypes[] =
	{
		{ "C4D_LinePower",     C4D_Line_Power },
		{ "C4D_LineSource",    C4D_Line_Source },
		{ "C4D_LineDrain",     C4D_Line_Drain },
		{ "C4D_LineLightning", C4D_Line_Lightning },
		{ "C4D_LineVolcano",   C4D_Line_Volcano },
		{ "C4D_LineRope",      C4D_Line_Rope },
		{ "C4D_LineColored",   C4D_Line_Colored },
		{ "C4D_LineVertex",    C4D_Line_Vertex },

		{ nullptr, 0 }
	};

	pComp->Value(mkNamingAdapt(mkBitfieldAdapt(Line, LineTypes), "Line", 0));

	const StdBitfieldEntry<uint32_t> LineConnectTypes[] =
	{
		{ "C4D_PowerInput",     C4D_Power_Input },
		{ "C4D_PowerOutput",    C4D_Power_Output },
		{ "C4D_LiquidInput",    C4D_Liquid_Input },
		{ "C4D_LiquidOutput",   C4D_Liquid_Output },
		{ "C4D_PowerGenerator", C4D_Power_Generator },
		{ "C4D_PowerConsumer",  C4D_Power_Consumer },
		{ "C4D_LiquidPump",     C4D_Liquid_Pump },
		{ "C4D_ConnectRope",    C4D_Connect_Rope },
		{ "C4D_EnergyHolder",   C4D_EnergyHolder },

		{ nullptr, 0 }
	};

	pComp->Value(mkNamingAdapt(mkBitfieldAdapt(LineConnect, LineConnectTypes),
		"LineConnect", 0));

	pComp->Value(mkNamingAdapt(LineIntersect,            "LineIntersect",  0));
	pComp->Value(mkNamingAdapt(Prey,                     "Prey",           0));
	pComp->Value(mkNamingAdapt(Edible,                   "Edible",         0));
	pComp->Value(mkNamingAdapt(CrewMember,               "CrewMember",     0));
	pComp->Value(mkNamingAdapt(NativeCrew,               "NoStandardCrew", 0));
	pComp->Value(mkNamingAdapt(Growth,                   "Growth",         0));
	pComp->Value(mkNamingAdapt(Rebuyable,                "Rebuy",          0));
	pComp->Value(mkNamingAdapt(Constructable,            "Construction",   0));
	pComp->Value(mkNamingAdapt(mkC4IDAdapt(BuildTurnTo), "ConstructTo",    0));
	pComp->Value(mkNamingAdapt(Grab,                     "Grab",           0));

	const StdBitfieldEntry<uint32_t> GrabPutGetTypes[] =
	{
		{ "C4D_GrabGet", C4D_Grab_Get },
		{ "C4D_GrabPut", C4D_Grab_Put },

		{ nullptr, 0 }
	};

	pComp->Value(mkNamingAdapt(mkBitfieldAdapt(GrabPutGet, GrabPutGetTypes),
		"GrabPutGet", 0));

	pComp->Value(mkNamingAdapt(Carryable,                       "Collectible",        0));
	pComp->Value(mkNamingAdapt(Rotateable,                      "Rotate",             0));
	pComp->Value(mkNamingAdapt(RotatedEntrance,                 "RotatedEntrance",    0));
	pComp->Value(mkNamingAdapt(Chopable,                        "Chop",               0));
	pComp->Value(mkNamingAdapt(Float,                           "Float",              0));
	pComp->Value(mkNamingAdapt(ContainBlast,                    "ContainBlast",       0));
	pComp->Value(mkNamingAdapt(ColorByOwner,                    "ColorByOwner",       0));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(ColorByMaterial), "ColorByMaterial",    ""));
	pComp->Value(mkNamingAdapt(NoHorizontalMove,                "HorizontalFix",      0));
	pComp->Value(mkNamingAdapt(BorderBound,                     "BorderBound",        0));
	pComp->Value(mkNamingAdapt(LiftTop,                         "LiftTop",            0));
	pComp->Value(mkNamingAdapt(UprightAttach,                   "UprightAttach",      0));
	pComp->Value(mkNamingAdapt(GrowthType,                      "StretchGrowth",      0));
	pComp->Value(mkNamingAdapt(Basement,                        "Basement",           0));
	pComp->Value(mkNamingAdapt(NoBurnDecay,                     "NoBurnDecay",        0));
	pComp->Value(mkNamingAdapt(IncompleteActivity,              "IncompleteActivity", 0));
	pComp->Value(mkNamingAdapt(AttractLightning,                "AttractLightning",   0));
	pComp->Value(mkNamingAdapt(Oversize,                        "Oversize",           0));
	pComp->Value(mkNamingAdapt(Fragile,                         "Fragile",            0));
	pComp->Value(mkNamingAdapt(Explosive,                       "Explosive",          0));
	pComp->Value(mkNamingAdapt(Projectile,                      "Projectile",         0));
	pComp->Value(mkNamingAdapt(NoPushEnter,                     "NoPushEnter",        0));
	pComp->Value(mkNamingAdapt(DragImagePicture,                "DragImagePicture",   0));
	pComp->Value(mkNamingAdapt(VehicleControl,                  "VehicleControl",     0));
	pComp->Value(mkNamingAdapt(Pathfinder,                      "Pathfinder",         0));
	pComp->Value(mkNamingAdapt(MoveToRange,                     "MoveToRange",        0));
	pComp->Value(mkNamingAdapt(NoComponentMass,                 "NoComponentMass",    0));
	pComp->Value(mkNamingAdapt(NoStabilize,                     "NoStabilize",        0));
	pComp->Value(mkNamingAdapt(ClosedContainer,                 "ClosedContainer",    0));
	pComp->Value(mkNamingAdapt(SilentCommands,                  "SilentCommands",     0));
	pComp->Value(mkNamingAdapt(NoBurnDamage,                    "NoBurnDamage",       0));
	pComp->Value(mkNamingAdapt(TemporaryCrew,                   "TemporaryCrew",      0));
	pComp->Value(mkNamingAdapt(SmokeRate,                       "SmokeRate",          100));
	pComp->Value(mkNamingAdapt(BlitMode,                        "BlitMode",           C4D_Blit_Normal));
	pComp->Value(mkNamingAdapt(NoBreath,                        "NoBreath",           0));
	pComp->Value(mkNamingAdapt(ConSizeOff,                      "ConSizeOff",         0));
	pComp->Value(mkNamingAdapt(NoSell,                          "NoSell",             0));
	pComp->Value(mkNamingAdapt(NoGet,                           "NoGet",              0));
	pComp->Value(mkNamingAdapt(NoFight,                         "NoFight",            0));
	pComp->Value(mkNamingAdapt(RotatedSolidmasks,               "RotatedSolidmasks",  0));
	pComp->Value(mkNamingAdapt(NoTransferZones,                 "NoTransferZones",    0));
	pComp->Value(mkNamingAdapt(AutoContextMenu,                 "AutoContextMenu",    0));
	pComp->Value(mkNamingAdapt(NeededGfxMode,                   "NeededGfxMode",      0));

	const StdBitfieldEntry<uint32_t> AllowPictureStackModes[] =
	{
		{ "APS_Color",    APS_Color },
		{ "APS_Graphics", APS_Graphics },
		{ "APS_Name",     APS_Name },
		{ "APS_Overlay",  APS_Overlay },
		{ nullptr,        0 }
	};

	pComp->Value(mkNamingAdapt(mkBitfieldAdapt<uint32_t>(AllowPictureStack, AllowPictureStackModes),
		"AllowPictureStack", 0));

	if (dynamic_cast<StdCompilerLuaRead *>(pComp))
	{
		pComp->Value(mkNamingAdapt(Physical, "Physical"));
	}
	else
	{
		pComp->FollowName("Physical");
		pComp->Value(Physical);
	}
}

void C4DefCore::UpdateValues(C4Group &hGroup)
{
	// Adjust category: C4D_CrewMember by CrewMember flag
	if (CrewMember) Category |= C4D_CrewMember;

	// Adjust picture rect
	if ((PictureRect.Wdt == 0) || (PictureRect.Hgt == 0))
		PictureRect.Set(0, 0, Shape.Wdt, Shape.Hgt);

	// Check category
#ifdef C4ENGINE
	if (!(Category & C4D_SortLimit))
	{
		// special: Allow this for spells
		if (~Category & C4D_Magic)
			DebugLogF("WARNING: Def %s (%s) at %s has invalid category!", GetName(), C4IdText(id), hGroup.GetFullName().getData());
		// assign a default category here
		Category = (Category & ~C4D_SortLimit) | 1;
	}
	// Check mass
	if (Mass < 0)
	{
		DebugLogF("WARNING: Def %s (%s) at %s has invalid mass!", GetName(), C4IdText(id), hGroup.GetFullName().getData());
		Mass = 0;
	}
#endif
}

// C4Def

C4Def::C4Def()
{
#ifdef C4ENGINE
	Graphics.pDef = this;
#endif
	Default();
}

void C4Def::Default()
{
	C4DefCore::Default();

#if !defined(C4ENGINE) && !defined(C4GROUP)
	Picture = nullptr;
	Image = nullptr;
#endif
	ActMap.clear();
	Next = nullptr;
	Temporary = false;
	Maker.clear();
	Filename.clear();
	Desc.clear();
	Creation = 0;
	Count = 0;
	TimerCall = nullptr;
#ifdef C4ENGINE
	MainFace.Set(nullptr, 0, 0, 0, 0);
	Script.Default();
	StringTable.Default();
	pClonkNames = nullptr;
	pRankNames = nullptr;
	pRankSymbols = nullptr;
	fClonkNamesOwned = fRankNamesOwned = fRankSymbolsOwned = false;
	iNumRankSymbols = 1;
	PortraitCount = 0;
	Portraits = nullptr;
	pFairCrewPhysical = nullptr;
#endif
}

C4Def::~C4Def()
{
	Clear();
}

void C4Def::Clear()
{
#ifdef C4ENGINE
	Graphics.Clear();

	LuaDef = luabridge::LuaRef(Game.LuaEngine.state());
	Script.Clear();
	StringTable.Clear();
	if (fClonkNamesOwned)  delete pClonkNames;  pClonkNames  = nullptr;
	if (fRankNamesOwned)   delete pRankNames;   pRankNames   = nullptr;
	if (fRankSymbolsOwned) delete pRankSymbols; pRankSymbols = nullptr;
	delete pFairCrewPhysical; pFairCrewPhysical = nullptr;
	fClonkNamesOwned = fRankNamesOwned = fRankSymbolsOwned = false;

	PortraitCount = 0;
	Portraits = nullptr;

#endif

	ActMap.clear();
	Desc.clear();
}

bool C4Def::Load(C4Group &hGroup,
	uint32_t dwLoadWhat,
	const char *szLanguage,
	C4SoundSystem *pSoundSystem)
{
	bool fSuccess = true;

#ifdef C4ENGINE
	bool AddFileMonitoring = false;
	if (Game.pFileMonitor && Filename != hGroup.GetFullName().getData() && !hGroup.IsPacked())
		AddFileMonitoring = true;
#endif

	// Store filename, maker, creation
	Filename = hGroup.GetFullName().getData();
	Maker = hGroup.GetMaker();
	Creation = hGroup.GetCreation();

#ifdef C4ENGINE
	// Verbose log filename
	if (Config.Graphics.VerboseObjectLoading >= 3)
		Log(hGroup.GetFullName().getData());

	if (AddFileMonitoring) Game.pFileMonitor->AddDirectory(Filename.c_str());

	// particle def?
	if (hGroup.AccessEntry(C4CFN_ParticleCore))
	{
		// def loading not successful; abort after reading sounds
		fSuccess = false;
		// create new particle def
		C4ParticleDef *pParticleDef = new C4ParticleDef();
		// load it
		if (!pParticleDef->Load(hGroup))
		{
			// not successful :( - destroy it again
			delete pParticleDef;
		}
		// done
	}

	char filename[_MAX_FNAME];
	bool isLuaDefinition = false;

	if (hGroup.AccessEntry(C4CFN_Lua, nullptr, filename) && !SEqual(filename, C4CFN_ScenarioLua))
	{
		isLuaDefinition = true;
		hGroup.ResetSearch();
		fSuccess = false;
		do
		{
			fSuccess = Game.LuaEngine.Load(hGroup, filename, szLanguage, &StringTable);
		}
		while (hGroup.AccessNextEntry(C4CFN_Lua, nullptr, filename));
	}

#endif

	if (!isLuaDefinition)
	{
		// Read DefCore
		if (fSuccess) fSuccess = C4DefCore::Load(hGroup);
		// check id
		if (fSuccess) if (!LooksLikeID(id))
		{
	#ifdef C4ENGINE
			// wie geth ID?????ßßßß
			if (Name.empty()) Name = GetFilename(hGroup.GetName());
			LogF(LoadResStr("IDS_ERR_INVALIDID"), Name.c_str());
	#endif
			fSuccess = false;
		}

	#ifdef C4ENGINE
		// skip def: don't even read sounds!
		if (fSuccess && Game.C4S.Definitions.SkipDefs.GetIDCount(id, 1)) return false;

		// OldGfx is no longer supported
		if (NeededGfxMode == C4DGFXMODE_OLDGFX) return false;
#endif
	}

	if (!fSuccess)
	{
#ifdef C4ENGINE
		// Read sounds even if not a valid def (for pure c4d sound folders)
		if (dwLoadWhat & C4D_Load_Sounds)
			if (pSoundSystem)
				pSoundSystem->LoadEffects(hGroup);
#endif
		if (!isLuaDefinition)
		{
			return false;
		}
	}

#ifdef C4ENGINE
	// Read surface bitmap
	if (dwLoadWhat & C4D_Load_Bitmap)
	{
		if (isLuaDefinition)
		{
			C4DefGraphics graphics(this);
			std::memset(filename, 0, sizeof(filename));
			hGroup.ResetSearch();
			while (hGroup.FindNextEntry(C4CFN_LuaGraphics, filename, nullptr, nullptr, !!*filename))
			{
				if (WildcardMatch(C4CFN_DefGraphicsExPNG, filename))
				{
					continue;
				}
				if (Game.LuaGraphics.find(filename) != Game.LuaGraphics.end())
				{
					DebugLogF("  Error loading graphics %s as another one with the same name already exists", hGroup.GetFullName().getData());
					continue;
				}
				if (!graphics.LoadBitmap(hGroup, nullptr, filename, nullptr, false))
				{
					DebugLogF("  Error loading graphics of %s", hGroup.GetFullName().getData());
					continue;
				}
				Game.LuaGraphics[filename] = graphics.GetBitmap();
				graphics.Bitmap = graphics.BitmapClr = nullptr; // gets deleted otherwise
			}

			/*// link surfaces
			for (auto it = Game.LuaGraphics.begin(); it != Game.LuaGraphics.end(); )
			{
				if (SEqual2NoCase("Overlay", it->first) && SEqualNoCase("png", GetExtension(it->first.c_str())))
				{
					auto base = Game.LuaGraphics.find(it->first.substr(std::strlen("Overlay"), it->first.size() - std::strlen("Overlay") - std::strlen(".png")));
					if (base == Game.LuaGraphics.end())
					{
						DebugLogF("  Error matching overlay %s: No base graphics", it->first.c_str());
						continue;
					}
					if (!it->second->SetAsClrByOwnerOf(base->second))
					{

					}
				}
			}*/
		}
		else
		{
			if (!Graphics.LoadBitmaps(hGroup, !!ColorByOwner))
			{
				DebugLogF("  Error loading graphics of %s (%s)", hGroup.GetFullName().getData(), C4IdText(id));
				return false;
			}
			if (!LoadPortraits())
			{
				DebugLogF("  Error loading portrait graphics of %s (%s)", hGroup.GetFullName().getData(), C4IdText(id));
				return false;
			}
		}
	}
#endif
	if (!isLuaDefinition)
	{
#ifdef C4ENGINE
		// Read ActMap
		if (dwLoadWhat & C4D_Load_ActMap)
			if (!LoadActMap(hGroup))
			{
				DebugLogF("  Error loading ActMap of %s (%s)", hGroup.GetFullName().getData(), C4IdText(id));
				return false;
			}

		// Read script
		if (dwLoadWhat & C4D_Load_Script)
		{
			// reg script to engine
			Script.Reg2List(&Game.ScriptEngine, &Game.ScriptEngine);
			// Load script - loads string table as well, because that must be done after script load
			// for downwards compatibility with packing order
			Script.Load("Script", hGroup, C4CFN_Script, szLanguage, this, &StringTable, true);
		}
#endif

		// Read name
		C4ComponentHost DefNames;
		if (DefNames.LoadEx("Names", hGroup, C4CFN_DefNames, szLanguage))
		{
			StdStrBuf buf;
			DefNames.GetLanguageString(szLanguage, buf);
			Name = buf.getData();
		}
		DefNames.Close();

#ifdef C4ENGINE
		// read clonknames
		if (dwLoadWhat & C4D_Load_ClonkNames)
		{
			// clear any previous
			delete pClonkNames; pClonkNames = nullptr;
			if (hGroup.FindEntry(C4CFN_ClonkNameFiles))
			{
				// create new
				pClonkNames = new C4ComponentHost();
				if (!pClonkNames->LoadEx(LoadResStr("IDS_CNS_NAMES"), hGroup, C4CFN_ClonkNames, szLanguage))
				{
					delete pClonkNames; pClonkNames = nullptr;
				}
				else
					fClonkNamesOwned = true;
			}
		}

		// read clonkranks
		if (dwLoadWhat & C4D_Load_RankNames)
		{
			// clear any previous
			delete pRankNames; pRankNames = nullptr;
			if (hGroup.FindEntry(C4CFN_RankNameFiles))
			{
				// create new
				pRankNames = new C4RankSystem();
				// load from group
				if (!pRankNames->Load(hGroup, C4CFN_RankNames, 1000, szLanguage))
				{
					delete pRankNames; pRankNames = nullptr;
				}
				else
					fRankNamesOwned = true;
			}
		}

		// read rankfaces
		if (dwLoadWhat & C4D_Load_RankFaces)
		{
			// clear any previous
			delete pRankSymbols; pRankSymbols = nullptr;
			// load new: try png first
			if (hGroup.AccessEntry(C4CFN_RankFacesPNG))
			{
				pRankSymbols = new C4FacetExSurface();
				if (!pRankSymbols->GetFace().ReadPNG(hGroup)) { delete pRankSymbols; pRankSymbols = nullptr; }
			}
			else if (hGroup.AccessEntry(C4CFN_RankFaces))
			{
				pRankSymbols = new C4FacetExSurface();
				if (!pRankSymbols->GetFace().Read(hGroup)) { delete pRankSymbols; pRankSymbols = nullptr; }
			}
		}

#endif

		// Read desc
		if (dwLoadWhat & C4D_Load_Desc)
		{
			C4ComponentHost desc;
			if (desc.LoadEx("Desc", hGroup, C4CFN_DefDesc, szLanguage))
			{
				desc.TrimSpaces();
				Desc = desc.GetData();
			}
		}

#ifdef C4ENGINE
		// Read sounds
		if (dwLoadWhat & C4D_Load_Sounds)
			if (pSoundSystem)
				pSoundSystem->LoadEffects(hGroup);
#endif
	}
	UpdateValues();

	// Temporary flag
	if (dwLoadWhat & C4D_Load_Temporary) Temporary = true;

	return true;
}

bool C4Def::Compile(luabridge::LuaRef def, C4ID newID)
{
	C4DefCore::Compile(def);
	id = newID;
	Desc = def["Description"].isString() ? def["Description"].tostring() : "";
	std::map<std::string, luabridge::LuaRef> test = def.cast<decltype(test)>();
	if (def["ActMap"].isTable())
	{
		auto actions = def["ActMap"].cast<std::vector<std::map<std::string, luabridge::LuaRef>>>();
		ActMap.resize(actions.size());

		StdCompilerLuaRead comp;
		comp.setInput(def);

		comp.Begin();
		assert(comp.Name("ActMap"));
		for (auto &action : ActMap)
		{
			try
			{
				action.CompileFunc(&comp);
				if (!comp.Separator())
				{
					break;
				}
			}
			catch (StdCompiler::Exception *e)
			{
				DebugLogF("ERROR: Definition with name %s has invalid ActMap entry: %s: %s", Name.c_str(), e->Pos.getData(), e->Msg.getData());
				action.Default();
				delete e;
				break;
			}
		}
		comp.End();
		CrossMapActMap();
	}

	luabridge::LuaRef graphics = def["Graphics"];
	if (!graphics.isTable())
	{
		DebugLogF("ERROR: Definition with name %s has invalid graphics specified", Name.c_str());
		return false;
	}
	if (graphics["Default"].isNil() || !graphics["Default"].isTable() || !graphics["Default"]["Base"].isString())
	{
		DebugLogF("ERROR: Definition with name %s is missing default graphics", Name.c_str());
	}
	else
	{
		Graphics.Clear();
		LoadGraphics(graphics["Default"]["Base"], graphics["Default"]["Overlay"].isString() ? graphics["Default"]["Overlay"].tostring() : "");

		for (const auto &pair : graphics.cast<std::map<std::string, luabridge::LuaRef>>())
		{
			if (pair.second.isString())
			{
				LoadGraphics(pair.second.tostring(), "", true);
			}
			else if (pair.second.isTable())
			{
				if (!pair.second["Base"].isString())
				{
					DebugLogF("Definition with name %s has invalid graphics set %s", Name.c_str(), pair.first.c_str());
					continue;
				}
				LoadGraphics(pair.second["Base"].tostring(), pair.second["Overlay"].isString() ? pair.second["Overlay"].tostring() : "");
			}
		}
	}
	if (def["Portrait"].isTable() && def["Portrait"]["Base"].isString())
	{
		LoadGraphics(def["Portrait"]["Base"], def["Portrait"]["Overlay"].isString() ? def["Portrait"]["Overlay"].tostring() : "", true, true);
		LoadPortraits();
	}
	UpdateValues();
	return true;
}

void C4Def::LoadGraphics(const std::string &base, const std::string &overlay, bool additional, bool portrait)
{
	static auto getGraphics = [this](const std::string &name) -> C4Surface *
	{
		auto i = Game.LuaGraphics.find(name);
		if (i == Game.LuaGraphics.end())
		{
			DebugLogF("ERROR: Definition with name %s specifies missing graphics: %s", Name.c_str(), name.c_str());
			return nullptr;
		}
		return i->second;
	};

	auto *surface = getGraphics(base);
	if (surface != nullptr)
	{
		C4Surface *overlaySurface = nullptr;
		if (ColorByOwner)
		{
			if (overlay.size())
			{
				overlaySurface = getGraphics(overlay);
				if (overlaySurface)
				{
					if (!overlaySurface->SetAsClrByOwnerOf(surface))
					{
						DebugLogF("Gfx loading error: %s (%d x %d) doesn't match overlay %s (%d x %d) - invalid file or size mismatch",
							base.c_str(), surface->Wdt, surface->Hgt,
							overlay.c_str(), overlaySurface->Wdt, overlaySurface->Hgt);
						delete overlaySurface;
						overlaySurface = nullptr;
					}
				}
			}
			else
			{
				overlaySurface = new C4Surface;
				if (!overlaySurface->CreateColorByOwner(surface))
				{
					DebugLogF("Gfx error: cannot create overlay by ColorByOwner");
					delete overlaySurface;
					overlaySurface = nullptr;
				}
			}
		}

		if (!additional)
		{
			Graphics.Bitmap = surface;
			Graphics.BitmapClr = overlaySurface;
		}
		else
		{
			auto *next = Graphics.pNext;
			Graphics.pNext = portrait ? new C4PortraitGraphics(this, base.c_str()) : new C4AdditionalDefGraphics(this, base.c_str());
			Graphics.pNext->pNext = next;
			Graphics.pNext->Bitmap = surface;
			Graphics.pNext->BitmapClr = overlaySurface;
		}
	}
}

bool C4Def::LoadActMap(C4Group &hGroup)
{
	// New format
	StdStrBuf Data;
	if (hGroup.LoadEntryString(C4CFN_DefActMap, Data))
	{
		// Compile
		if (!CompileFromBuf_LogWarn<StdCompilerINIRead>(
			mkNamingAdapt(mkSTLContainerAdapt(ActMap), "Action"),
			Data,
			(hGroup.GetFullName() + DirSep C4CFN_DefActMap).getData()))
			return false;
		// Process map
		CrossMapActMap();
		return true;
	}

	// No act map in group: okay
	return true;
}

void C4Def::CrossMapActMap()
{
	for (auto &action : ActMap)
	{
		// Map standard procedures
		action.Procedure = DFA_NONE;
		for (int32_t i = 0; i < C4D_MaxDFA; ++i)
		{
			if (action.ProcedureName == ProcedureName[i])
			{
				action.Procedure = i;
				break;
			}
		}
		// Map next action
		if (action.NextActionName.size())
		{
			if (SEqualNoCase(action.NextActionName, "Hold"))
			{
				action.NextAction = ActHold;
			}
			else
			{
				for (size_t i = 0; i < ActMap.size(); ++i)
				{
					if (action.NextActionName == ActMap[i].Name)
					{
						action.NextAction = static_cast<int32_t>(i);
						break;
					}
				}
			}
		}
		// Check act calls
		if (SEqualNoCase(action.SStartCall, "None")) action.SStartCall.clear();
		if (SEqualNoCase(action.SPhaseCall, "None")) action.SPhaseCall.clear();
		if (SEqualNoCase(action.SEndCall,   "None")) action.SEndCall.clear();
		if (SEqualNoCase(action.SAbortCall, "None")) action.SAbortCall.clear();
	}
}

bool C4Def::ColorizeByMaterial(C4MaterialMap &rMats, uint8_t bGBM)
{
#ifdef C4ENGINE
	if (ColorByMaterial.size())
	{
		int32_t mat = rMats.Get(ColorByMaterial.c_str());
		if (mat == MNone) { LogF("C4Def::ColorizeByMaterial: mat %s not defined", ColorByMaterial.c_str()); return false; }
		if (!Graphics.ColorizeByMaterial(mat, rMats, bGBM)) return false;
	}
#endif
	// success
	return true;
}

void C4Def::Draw(C4Facet &cgo, bool fSelected, uint32_t iColor, C4Object *pObj, int32_t iPhaseX, int32_t iPhaseY)
{
#ifdef C4ENGINE

	// default: def picture rect
	C4Rect fctPicRect = PictureRect;
	C4Facet fctPicture;

	// if assigned: use object specific rect and graphics
	if (pObj) if (pObj->PictureRect.Wdt) fctPicRect = pObj->PictureRect;

	fctPicture.Set((pObj ? *pObj->GetGraphics() : Graphics).GetBitmap(iColor), fctPicRect.x, fctPicRect.y, fctPicRect.Wdt, fctPicRect.Hgt);

	if (fSelected)
		Application.DDraw->DrawBox(cgo.Surface, cgo.X, cgo.Y, cgo.X + cgo.Wdt - 1, cgo.Y + cgo.Hgt - 1, CRed);

	// specific object color?
	if (pObj) pObj->PrepareDrawing();
	fctPicture.Draw(cgo, true, iPhaseX, iPhaseY, true);
	if (pObj) pObj->FinishedDrawing();

	// draw overlays
	if (pObj && pObj->pGfxOverlay)
		for (C4GraphicsOverlay *pGfxOvrl = pObj->pGfxOverlay; pGfxOvrl; pGfxOvrl = pGfxOvrl->GetNext())
			if (pGfxOvrl->IsPicture())
				pGfxOvrl->DrawPicture(cgo, pObj);
#endif
}

void C4Def::UpdateValues()
{
	// set size
	if (pRankSymbols)
	{
		pRankSymbols->Set(&pRankSymbols->GetFace(), 0, 0, pRankSymbols->GetFace().Hgt, pRankSymbols->GetFace().Hgt);
		int32_t Q; pRankSymbols->GetPhaseNum(iNumRankSymbols, Q);
		if (!iNumRankSymbols) { delete pRankSymbols; pRankSymbols = nullptr; }
		else
		{
			if (pRankNames)
			{
				// if extended rank names are defined, subtract those from the symbol count. The last symbols are used as overlay
				iNumRankSymbols = std::max<int32_t>(1, iNumRankSymbols - pRankNames->GetExtendedRankNum());
			}
			fRankSymbolsOwned = true;
		}
	}
#ifdef C4ENGINE
	// Bitmap post-load settings
	if (Graphics.GetBitmap())
	{
		// check SolidMask
		if (SolidMask.x < 0 || SolidMask.y < 0 || SolidMask.x + SolidMask.Wdt > Graphics.Bitmap->Wdt || SolidMask.y + SolidMask.Hgt > Graphics.Bitmap->Hgt) SolidMask.Default();
		// Set MainFace (unassigned bitmap: will be set by GetMainFace())
		MainFace.Set(nullptr, 0, 0, Shape.Wdt, Shape.Hgt);

		// validate TopFace
		if (TopFace.x < 0 || TopFace.y < 0 || TopFace.x + TopFace.Wdt > Graphics.Bitmap->Wdt || TopFace.y + TopFace.Hgt > Graphics.Bitmap->Hgt)
		{
			TopFace.Default();
			// warn in debug mode
			DebugLogF("invalid TopFace in %s(%s)", Name.c_str(), C4IdText(id));
		}
	}
#endif
}

#ifdef C4ENGINE
int32_t C4Def::GetValue(C4Object *pInBase, int32_t iBuyPlayer)
{
	// CalcDefValue defined?
	C4AulFunc *pCalcValueFn = Script.GetSFunc(PSF_CalcDefValue, AA_PROTECTED);
	int32_t iValue;
	if (pCalcValueFn)
		// then call it!
		iValue = pCalcValueFn->Exec(nullptr, &C4AulParSet(C4VObj(pInBase), C4VInt(iBuyPlayer))).getInt();
	else
		// otherwise, use default value
		iValue = Value;
	// do any adjustments based on where the item is bought
	if (pInBase)
	{
		C4AulFunc *pFn;
		if (pFn = pInBase->Def->Script.GetSFunc(PSF_CalcBuyValue, AA_PROTECTED))
			iValue = pFn->Exec(pInBase, &C4AulParSet(C4VID(id), C4VInt(iValue))).getInt();
	}
	return iValue;
}

C4PhysicalInfo *C4Def::GetFairCrewPhysicals()
{
	// if fair crew physicals have been created, assume they are valid
	if (!pFairCrewPhysical)
	{
		pFairCrewPhysical = new C4PhysicalInfo(Physical);
		// determine the rank
		int32_t iExpGain = Game.Parameters.FairCrewStrength;
		C4RankSystem *pRankSys = &Game.Rank;
		if (pRankNames) pRankSys = pRankNames;
		int32_t iRank = pRankSys->RankByExperience(iExpGain);
		// promote physicals for rank
		pFairCrewPhysical->PromotionUpdate(iRank, true, this);
	}
	return pFairCrewPhysical;
}

void C4Def::ClearFairCrewPhysicals()
{
	// invalidate physicals so the next call to GetFairCrewPhysicals will
	// reacreate them
	delete pFairCrewPhysical; pFairCrewPhysical = nullptr;
}

void C4Def::Synchronize()
{
	// because recreation of fair crew physicals does a script call, which *might* do a call to e.g. Random
	// fair crew physicals must be cleared and recalculated for everyone
	ClearFairCrewPhysicals();
}

#endif

// C4DefList

C4DefList::C4DefList()
{
	Default();
}

C4DefList::~C4DefList()
{
	Clear();
}

int32_t C4DefList::Load(C4Group &hGroup, uint32_t dwLoadWhat,
	const char *szLanguage,
	C4SoundSystem *pSoundSystem,
	bool fOverload,
	bool fSearchMessage, int32_t iMinProgress, int32_t iMaxProgress, bool fLoadSysGroups)
{
	int32_t iResult = 0;
	C4Def *nDef;
	char szEntryname[_MAX_FNAME + 1];
	C4Group hChild;
	bool fPrimaryDef = false;
	bool fThisSearchMessage = false;

	// This search message
	if (fSearchMessage)
		if (SEqualNoCase(GetExtension(hGroup.GetName()), "c4d")
			|| SEqualNoCase(GetExtension(hGroup.GetName()), "c4s")
			|| SEqualNoCase(GetExtension(hGroup.GetName()), "c4f"))
		{
			fThisSearchMessage = true;
			fSearchMessage = false;
		}

#ifdef C4ENGINE // Message
	if (fThisSearchMessage) { LogF("%s...", GetFilename(hGroup.GetName())); }
#endif

	// Load primary definition
	if (nDef = new C4Def)
		if (nDef->Load(hGroup, dwLoadWhat, szLanguage, pSoundSystem) && Add(nDef, fOverload))
		{
			iResult++; fPrimaryDef = true;
		}
		else
		{
			delete nDef;
		}

	// Load sub definitions
	int i = 0;
	hGroup.ResetSearch();
	while (hGroup.FindNextEntry(C4CFN_DefFiles, szEntryname))
		if (hChild.OpenAsChild(&hGroup, szEntryname))
		{
			// Hack: Assume that there are sixteen sub definitions to avoid unnecessary I/O
			int iSubMinProgress = std::min<int32_t>(iMaxProgress, iMinProgress + ((iMaxProgress - iMinProgress) * i) / 16);
			int iSubMaxProgress = std::min<int32_t>(iMaxProgress, iMinProgress + ((iMaxProgress - iMinProgress) * (i + 1)) / 16);
			++i;
			iResult += Load(hChild, dwLoadWhat, szLanguage, pSoundSystem, fOverload, fSearchMessage, iSubMinProgress, iSubMaxProgress);
			hChild.Close();
		}

	// load additional system scripts for def groups only
#ifdef C4ENGINE
	C4Group SysGroup;
	char fn[_MAX_FNAME + 1] = { 0 };
	if (!fPrimaryDef && fLoadSysGroups) if (SysGroup.OpenAsChild(&hGroup, C4CFN_System))
	{
		C4LangStringTable SysGroupString;
		SysGroupString.LoadEx("StringTbl", SysGroup, C4CFN_ScriptStringTbl, Config.General.LanguageEx);
		// load all scripts in there
		SysGroup.ResetSearch();
		while (SysGroup.FindNextEntry(C4CFN_ScriptFiles, (char *)&fn, nullptr, nullptr, !!fn[0]))
		{
			// host will be destroyed by script engine, so drop the references
			C4ScriptHost *scr = new C4ScriptHost();
			scr->Reg2List(&Game.ScriptEngine, &Game.ScriptEngine);
			scr->Load(nullptr, SysGroup, fn, Config.General.LanguageEx, nullptr, &SysGroupString);
		}
		SysGroup.ResetSearch();
		while (SysGroup.FindNextEntry(C4CFN_Lua, reinterpret_cast<char *>(&fn), nullptr, nullptr, !!fn[0]))
		{
			Game.LuaEngine.Load(SysGroup, fn, Config.General.LanguageEx, &SysGroupString);
		}
		// if it's a physical group: watch out for changes
		if (!SysGroup.IsPacked() && Game.pFileMonitor)
			Game.pFileMonitor->AddDirectory(SysGroup.GetFullName().getData());
		SysGroup.Close();
	}
#endif

#ifdef C4ENGINE // Message
	if (fThisSearchMessage) { LogF(LoadResStr("IDS_PRC_DEFSLOADED"), iResult); }

	// progress (could go down one level of recursion...)
	if (iMinProgress != iMaxProgress) Game.SetInitProgress(float(iMaxProgress));
#endif

	return iResult;
}

int32_t C4DefList::Load(const char *szSearch,
	uint32_t dwLoadWhat, const char *szLanguage,
	C4SoundSystem *pSoundSystem,
	bool fOverload, int32_t iMinProgress, int32_t iMaxProgress)
{
	int32_t iResult = 0;

	// Empty
	if (!szSearch[0]) return iResult;

	// Segments
	char szSegment[_MAX_PATH + 1]; int32_t iGroupCount;
	if (iGroupCount = SCharCount(';', szSearch))
	{
		++iGroupCount; int32_t iPrg = iMaxProgress - iMinProgress;
		for (int32_t cseg = 0; SCopySegment(szSearch, cseg, szSegment, ';', _MAX_PATH); cseg++)
			iResult += Load(szSegment, dwLoadWhat, szLanguage, pSoundSystem, fOverload,
				iMinProgress + iPrg * cseg / iGroupCount, iMinProgress + iPrg * (cseg + 1) / iGroupCount);
		return iResult;
	}

	// Wildcard items
	if (SCharCount('*', szSearch))
	{
#ifdef _WIN32
		struct _finddata_t fdt; int32_t fdthnd;
		if ((fdthnd = _findfirst(szSearch, &fdt)) < 0) return false;
		do
		{
			iResult += Load(fdt.name, dwLoadWhat, szLanguage, pSoundSystem, fOverload);
		} while (_findnext(fdthnd, &fdt) == 0);
		_findclose(fdthnd);
#ifdef C4ENGINE
		// progress
		if (iMinProgress != iMaxProgress) Game.SetInitProgress(float(iMaxProgress));
#endif
#else
		fputs("FIXME: C4DefList::Load\n", stderr);
#endif
		return iResult;
	}

	// File specified with creation (currently not used)
	char szCreation[25 + 1];
	int32_t iCreation = 0;
	if (SCopyEnclosed(szSearch, '(', ')', szCreation, 25))
	{
		// Scan creation
		SClearFrontBack(szCreation);
		sscanf(szCreation, "%i", &iCreation);
		// Extract filename
		SCopyUntil(szSearch, szSegment, '(', _MAX_PATH);
		SClearFrontBack(szSegment);
		szSearch = szSegment;
	}

	// Load from specified file
	C4Group hGroup;
	if (!hGroup.Open(szSearch))
	{
		// Specified file not found (failure)
#ifdef C4ENGINE
		LogFatal(FormatString(LoadResStr("IDS_PRC_DEFNOTFOUND"), szSearch).getData());
#endif
		LoadFailure = true;
		return iResult;
	}
	iResult += Load(hGroup, dwLoadWhat, szLanguage, pSoundSystem, fOverload, true, iMinProgress, iMaxProgress);
	hGroup.Close();

#ifdef C4ENGINE
	// progress (could go down one level of recursion...)
	if (iMinProgress != iMaxProgress) Game.SetInitProgress(float(iMaxProgress));
#endif

	return iResult;
}

bool C4DefList::Add(C4Def *pDef, bool fOverload)
{
	if (!pDef) return false;

	// Check old def to overload
	C4Def *pLastDef = ID2Def(pDef->id);
	if (pLastDef && !fOverload) return false;

#ifdef C4ENGINE
	// Log overloaded def
	if (Config.Graphics.VerboseObjectLoading >= 1)
		if (pLastDef)
		{
			LogF(LoadResStr("IDS_PRC_DEFOVERLOAD"), pDef->GetName(), C4IdText(pLastDef->id));
			if (Config.Graphics.VerboseObjectLoading >= 2)
			{
				LogF("      Old def at %s", pLastDef->Filename.c_str());
				LogF("     Overload by %s", pDef->Filename.c_str());
			}
		}
#endif

	// Remove old def
	Remove(pDef->id);
	table[pDef->id] = pDef;

	return true;
}

bool C4DefList::Remove(C4ID id)
{
	return !!table.erase(id);
}

void C4DefList::Remove(C4Def *def)
{
	for (auto it = table.begin(); it != table.end(); )
	{
		if (it->second == def)
		{
			table.erase(it++);
		}
		else
		{
			++it;
		}
	}
}

void C4DefList::Clear()
{
	table.clear();
}

C4Def *C4DefList::ID2Def(C4ID id)
{
	auto i = table.find(id);
	return i != table.end() ? i->second : nullptr;
}

int32_t C4DefList::GetDefCount(uint32_t dwCategory)
{
	return static_cast<int32_t>(std::count_if(table.begin(), table.end(), [&dwCategory](const std::pair<C4ID, C4Def *> &entry)
	{
		return entry.second->Category & dwCategory;
	}));
}

C4Def *C4DefList::GetDef(int32_t iIndex, uint32_t dwCategory)
{
	int32_t currentIndex = -1;
	for (const auto &entry : table)
	{
		if (entry.second->Category & dwCategory && ++currentIndex == iIndex)
		{
			return entry.second;
		}
	}
	return nullptr;
}

#ifdef C4ENGINE
C4Def *C4DefList::GetByPath(const std::string &path)
{
	auto entry = std::find_if(table.begin(), table.end(), [&path](const std::pair<C4ID, C4Def *> &entry)
	{
		std::string defPath = Config.AtExeRelativePath(entry.second->Filename.c_str());
		return defPath.size() && SEqual2NoCase(path, defPath) &&
				((path.size() == defPath.size()) || (path[defPath.size()] == '\\' && path.find('\\', defPath.size() + 1) == std::string::npos));
	});
	return entry != table.end() ? entry->second : nullptr;
}
#endif

int32_t C4DefList::CheckEngineVersion(int32_t ver1, int32_t ver2, int32_t ver3, int32_t ver4)
{
	int32_t rcount = 0;
	for (auto it = table.begin(); it != table.end(); )
	{
		if (CompareVersion(
				it->second->rC4XVer[0], it->second->rC4XVer[1], it->second->rC4XVer[2], it->second->rC4XVer[3],
				ver1, ver2, ver3, ver4
				) > 0)
		{
			table.erase(it++);
			++rcount;
		}
		else
		{
			++it;
		}
	}
	return rcount;
}

int32_t C4DefList::CheckRequireDef()
{
	int32_t rcount[2] = {0, 0};
	do
	{
		rcount[1] = rcount[0];
		for (auto it = table.begin(); it != table.end(); ++it)
		{
			for (int32_t i = 0; i < it->second->RequireDef.GetNumberOfIDs(); ++i)
			{
				if (table.find(it->second->RequireDef.GetID(i)) == table.end())
				{
					table.erase(it);
					++rcount[0];
					break;
				}
			}
		}
	}
	while (rcount[0] != rcount[1]);
	return rcount[0];
}

int32_t C4DefList::ColorizeByMaterial(C4MaterialMap &rMats, uint8_t bGBM)
{
	return static_cast<int32_t>(std::count_if(table.begin(), table.end(), [&rMats, &bGBM](const std::pair<C4ID, C4Def *> entry)
	{
		return entry.second->ColorizeByMaterial(rMats, bGBM);
	}));
}

void C4DefList::Draw(C4ID id, C4Facet &cgo, bool fSelected, int32_t iColor)
{
	C4Def *cdef = ID2Def(id);
	if (cdef) cdef->Draw(cgo, fSelected, iColor);
}

void C4DefList::Default()
{
	table.clear();
	LoadFailure = false;
}

bool C4DefList::Reload(C4Def *pDef, uint32_t dwLoadWhat, const char *szLanguage, C4SoundSystem *pSoundSystem)
{
	// Safety
	if (!pDef) return false;
#ifdef C4ENGINE
	// backup graphics names and pointers
	// GfxBackup-dtor will ensure that upon loading-failure all graphics are reset to default
	C4DefGraphicsPtrBackup GfxBackup(&pDef->Graphics);
	// clear any pointers into def (name)
	Game.Objects.ClearDefPointers(pDef);
#endif
	// Clear def
	pDef->Clear(); // Assume filename is being kept
	// Reload def
	C4Group hGroup;
	if (!hGroup.Open(pDef->Filename.c_str())) return false;
	if (!pDef->Load(hGroup, dwLoadWhat, szLanguage, pSoundSystem)) return false;
	hGroup.Close();
#ifdef C4ENGINE
	// update script engine - this will also do include callbacks
	Game.ScriptEngine.ReLink(this);
#endif
#ifdef C4ENGINE
	// update definition pointers
	Game.Objects.UpdateDefPointers(pDef);
	// restore graphics
	GfxBackup.AssignUpdate(&pDef->Graphics);
#endif
	// Success
	return true;
}

bool C4Def::LoadPortraits()
{
#ifdef C4ENGINE
	// reset any previous portraits
	Portraits = nullptr; PortraitCount = 0;
	// search for portraits within def graphics
	for (C4DefGraphics *pGfx = &Graphics; pGfx; pGfx = pGfx->GetNext())
		if (pGfx->IsPortrait())
		{
			// assign first portrait
			if (!Portraits) Portraits = pGfx->IsPortrait();
			// count
			++PortraitCount;
		}
#endif
	return true;
}

C4ValueArray *C4Def::GetCustomComponents(C4Value *pvArrayHolder, C4Object *pBuilder, C4Object *pObjInstance)
{
	// return custom components array if script function is defined and returns an array
#ifdef C4ENGINE
	if (Script.SFn_CustomComponents)
	{
		C4AulParSet pars(C4VObj(pBuilder));
		*pvArrayHolder = Script.SFn_CustomComponents->Exec(pObjInstance, &pars);
		return pvArrayHolder->getArray();
	}
#endif
	return nullptr;
}

int32_t C4Def::GetComponentCount(C4ID idComponent, C4Object *pBuilder)
{
	// script overload?
	C4Value vArrayHolder;
	C4ValueArray *pArray = GetCustomComponents(&vArrayHolder, pBuilder);
	if (pArray)
	{
		int32_t iCount = 0;
		for (int32_t i = 0; i < pArray->GetSize(); ++i)
			if (pArray->GetItem(i).getC4ID() == idComponent)
				++iCount;
		return iCount;
	}
	// no valid script overload: Assume definition components
	return Component.GetIDCount(idComponent);
}

C4ID C4Def::GetIndexedComponent(int32_t idx, C4Object *pBuilder)
{
	// script overload?
	C4Value vArrayHolder;
	C4ValueArray *pArray = GetCustomComponents(&vArrayHolder, pBuilder);
	if (pArray)
	{
		// assume that components are always returned ordered ([a, a, b], but not [a, b, a])
		if (!pArray->GetSize()) return 0;
		C4ID idLast = pArray->GetItem(0).getC4ID();
		if (!idx) return idLast;
		for (int32_t i = 1; i < pArray->GetSize(); ++i)
		{
			C4ID idCurr = pArray->GetItem(i).getC4ID();
			if (idCurr != idLast)
			{
				if (!--idx) return (idCurr);
				idLast = idCurr;
			}
		}
		// index out of bounds
		return 0;
	}
	// no valid script overload: Assume definition components
	return Component.GetID(idx);
}

void C4Def::GetComponents(C4IDList *pOutList, C4Object *pObjInstance, C4Object *pBuilder)
{
	assert(pOutList);
	assert(!pOutList->GetNumberOfIDs());
	// script overload?
	C4Value vArrayHolder;
	C4ValueArray *pArray = GetCustomComponents(&vArrayHolder, pBuilder, pObjInstance);
	if (pArray)
	{
		// transform array into IDList
		// assume that components are always returned ordered ([a, a, b], but not [a, b, a])
		C4ID idLast = 0; int32_t iCount = 0;
		for (int32_t i = 0; i < pArray->GetSize(); ++i)
		{
			C4ID idCurr = pArray->GetItem(i).getC4ID();
			if (!idCurr) continue;
			if (i && idCurr != idLast)
			{
				pOutList->SetIDCount(idLast, iCount, true);
				iCount = 0;
			}
			idLast = idCurr;
			++iCount;
		}
		if (iCount) pOutList->SetIDCount(idLast, iCount, true);
	}
	else
	{
#ifdef C4ENGINE
		// no valid script overload: Assume object or definition components
		if (pObjInstance)
			*pOutList = pObjInstance->Component;
		else
			*pOutList = Component;
#endif
	}
}

void C4Def::IncludeDefinition(C4Def *pIncludeDef)
{
#ifdef C4ENGINE
	// inherited rank infos and clonk names, if this definition doesn't have its own
	if (!fClonkNamesOwned) pClonkNames = pIncludeDef->pClonkNames;
	if (!fRankNamesOwned) pRankNames = pIncludeDef->pRankNames;
	if (!fRankSymbolsOwned) { pRankSymbols = pIncludeDef->pRankSymbols; iNumRankSymbols = pIncludeDef->iNumRankSymbols; }
#endif
}

void C4Def::ResetIncludeDependencies()
{
#ifdef C4ENGINE
	// clear all pointers into foreign defs
	if (!fClonkNamesOwned) pClonkNames = nullptr;
	if (!fRankNamesOwned) pRankNames = nullptr;
	if (!fRankSymbolsOwned) { pRankSymbols = nullptr; iNumRankSymbols = 0; }
#endif
}

// C4DefList

bool C4DefList::GetFontImage(const char *szImageTag, CFacet &rOutImgFacet)
{
#ifdef C4ENGINE
	// extended: images by game
	C4FacetExSurface fctOut;
	if (!Game.DrawTextSpecImage(fctOut, szImageTag)) return false;
	if (fctOut.Surface == &fctOut.GetFace()) return false; // cannot use facets that are drawn on the fly right now...
	rOutImgFacet.Set(fctOut.Surface, fctOut.X, fctOut.Y, fctOut.Wdt, fctOut.Hgt);
#endif
	// done, found
	return true;
}

#ifdef C4ENGINE
void C4DefList::Synchronize()
{
	for (const auto &entry : table)
	{
		entry.second->Synchronize();
	}
}
#endif

void C4DefList::ResetIncludeDependencies()
{
	for (const auto &entry : table)
	{
		entry.second->ResetIncludeDependencies();
	}
}
