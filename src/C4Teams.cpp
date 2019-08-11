/*
 * LegacyClonk
 *
 * Copyright (c) RedWolf Design
 * Copyright (c) 2005, Sven2
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

// player team management for teamwork melees

#include <C4Include.h>

#include "C4Components.h"
#include "C4Game.h"
#include "C4Player.h"
#include "C4Random.h"
#include "C4Teams.h"
#include "C4Wrappers.h"

// C4Team

C4Team::C4Team() : Players{}, iID{0}, iPlrStartIndex{0}, dwClr{0}, MaxPlayer{0}
{
	wrapper = new LuaHelpers::DeletableObjectPtr<C4Team>{nullptr, this};
}

C4Team::C4Team(const C4Team &rCopy)
	: Players{rCopy.Players},
	iID{rCopy.GetID()}, Name{rCopy.Name}, iPlrStartIndex{rCopy.iPlrStartIndex}, dwClr{rCopy.dwClr},
	IconSpec{rCopy.IconSpec}, MaxPlayer{rCopy.MaxPlayer}
{
	if (rCopy.wrapper)
	{
		wrapper = rCopy.wrapper;
		wrapper->incReferenceCount();
	}
}

void C4Team::Clear()
{
	iID = 0;
	Name.clear();
	IconSpec.clear();

	if (wrapper)
	{
		wrapper->reset();
		wrapper->decReferenceCount();
		wrapper = nullptr;
	}
}

void C4Team::AddPlayer(C4PlayerInfo &rInfo, bool fAdjustPlayer)
{
	// must not happen!
	assert(rInfo.GetID());
	if (!rInfo.GetID()) return;
	// add player; grow vector if necessary
	// store new player
	Players.insert(rInfo.GetID());
	if (!fAdjustPlayer) return;
	// set values in info
	rInfo.SetTeam(GetID());
	if (Game.Teams.IsTeamColors()) rInfo.SetColor(GetColor());
	// and in actual player, if it is joined already
	if (rInfo.IsJoined())
	{
		C4Player *pJoinedPlr = Game.Players.GetByInfoID(rInfo.GetID());
		assert(pJoinedPlr);
		if (pJoinedPlr)
		{
			pJoinedPlr->Team = GetID();
			if (Game.Teams.IsTeamColors()) pJoinedPlr->SetPlayerColor(GetColor());
		}
	}
}

int32_t C4Team::GetIndexedPlayer(size_t index)
{
	assert(index < Players.size());
	if (index >= Players.size()) return 0;

	auto it = Players.begin();
	std::advance(it, index);

	return *it;
}

void C4Team::RemoveIndexedPlayer(size_t index)
{
	// safety
	assert(index < Players.size());
	if (index >= Players.size()) return;

	auto it = Players.begin();
	std::advance(it, index);

	Players.erase(it);
}

void C4Team::RemovePlayerByID(int32_t iID)
{
	auto i = std::find(Players.begin(), Players.end(), iID);
	if (i == Players.end())
	{
		assert(false);
		return;
	}
	Players.erase(i);
}

bool C4Team::IsPlayerIDInTeam(int32_t iID)
{
	return Players.count(iID);
}

int32_t C4Team::GetFirstUnjoinedPlayerID() const
{
	// search for a player that does not have the join-flag set
	for (const auto &player : Players)
	{
		if (C4PlayerInfo *info = Game.PlayerInfos.GetPlayerInfoByID(player); !info->HasJoinIssued())
		{
			return player;
		}
	}

	return 0;
}

int32_t C4Team::GetFirstActivePlayerID() const
{
	// search for a player that is currently in the game
	for (const auto &player : Players)
	{
		if (C4Player *p = Game.Players.GetByInfoID(player); p)
		{
			return player;
		}
	}

	return 0;
}

void C4Team::CompileFunc(StdCompiler *pComp)
{
	if (pComp->isCompiler()) Clear();
	pComp->Value(mkNamingAdapt(iID,                          "id",            0));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(Name),         "Name",          ""));
	pComp->Value(mkNamingAdapt(iPlrStartIndex,               "PlrStartIndex", 0));
	pComp->Value(mkNamingAdapt(Players.size(),               "PlayerCount",   0));
	if (pComp->isCompiler()) { Players.clear(); }
	pComp->Value(mkNamingAdapt(mkSTLContainerAdapt(Players), "Players",       decltype(Players){}));
	pComp->Value(mkNamingAdapt(dwClr,                        "Color",         0u));
	pComp->Value(mkNamingAdapt(mkStringAdaptA(IconSpec),     "IconSpec",      ""));
	pComp->Value(mkNamingAdapt(MaxPlayer,                    "MaxPlayer",     0));
}

void C4Team::RecheckPlayers()
{
	Players.erase(std::remove_if(Players.begin(), Players.end(), [this](const auto &id)
	{
		if (id == 0)
		{
			return true;
		}

		else if(C4PlayerInfo *info = Game.PlayerInfos.GetPlayerInfoByID(id); !info && info->IsUsingTeam() && info->GetTeam() == GetID())
		{
			return true;
		}
	}));

	// now check for any new players in the team
	int32_t id = 0; C4PlayerInfo *info;
	while ((info = Game.PlayerInfos.GetNextPlayerInfoByID(id)))
	{
		id = info->GetID();
		if (info->IsUsingTeam() && info->GetTeam() == GetID() && !IsPlayerIDInTeam(id))
		{
			AddPlayer(*info, false);
		}
	}
}

uint32_t GenerateRandomPlayerColor(int32_t iTry); // C4PlayerInfo.cpp
bool IsColorConflict(uint32_t dwClr1, uint32_t dwClr2); // C4PlayerInfo.cpp

void C4Team::RecheckColor(C4TeamList &rForList)
{
	// number of times trying new player colors
	const int32_t C4MaxTeamColorChangeTries = 100;
	if (!dwClr)
	{
		const int defTeamColorCount = 10;
		uint32_t defTeamColorRGB[defTeamColorCount] =
		{
			0xF40000, 0x00C800, 0xFCF41C, 0x2020FF, // red, green, yellow, blue,
			0xC48444, 0xFFFFFF, 0x848484, 0xFF00EF, // brown, white, grey, pink,
			0x00FFFF, 0x784830
		}; // cyan, dk brown
// no color assigned yet: Generate by team ID
		if (iID >= 1 && iID <= defTeamColorCount + 1)
		{
			// default colors
			dwClr = defTeamColorRGB[iID - 1];
		}
		else
		{
			// find a new, unused color
			for (int32_t iTry = 1; iTry < C4MaxTeamColorChangeTries; ++iTry)
			{
				dwClr = GenerateRandomPlayerColor(iTry);
				int32_t iIdx = 0; C4Team *pTeam; bool fOK = true;
				while (pTeam = rForList.GetTeamByIndex(iIdx++))
					if (pTeam != this)
						if (IsColorConflict(pTeam->GetColor(), dwClr))
						{
							fOK = false;
							break;
						}
				// color is fine?
				if (fOK) return;
				// it's not; try next color
			}
			// Giving up: Use last generated color
		}
	}
}

StdStrBuf C4Team::GetNameWithParticipants() const
{
	// compose team name like "Team 1 (boni, GhostBear, Clonko)"
	// or just "Team 1" for empty team
	StdStrBuf teamName;
	teamName.Copy(GetName());
	if (GetPlayerCount())
	{
		teamName.Append(" (");

		size_t teamPlayerCount = 0;
		for (const auto &player : Players)
		{
			if (C4PlayerInfo *info = Game.PlayerInfos.GetPlayerInfoByID(player))
			{
				if (teamPlayerCount++)
				{
					teamName.Append(", ");
					teamName.Append(info->GetName());
				}
			}
		}

		teamName.AppendChar(')');
	}
	return teamName;
}

bool C4Team::HasWon() const
{
	// return true if any member player of the team has won

	for (const auto &player : Players)
	{
		if (C4PlayerInfo *info = Game.PlayerInfos.GetPlayerInfoByID(player); info->HasWon())
		{
			return true;
		}
	}

	return false;
}

// C4TeamList

void C4TeamList::Clear()
{
	// del all teams
	ClearTeams();
	// del player team vector
	delete[] ppList; ppList = nullptr;
	iTeamCapacity = 0;
	fAllowHostilityChange = true;
	fAllowTeamSwitch = false;
	fCustom = false;
	fActive = true;
	fTeamColors = false;
	eTeamDist = TEAMDIST_Free;
	fAutoGenerateTeams = false;
	iMaxScriptPlayers = 0;
	sScriptPlayerNames.Clear();
}

C4TeamList &C4TeamList::operator=(const C4TeamList &rCopy)
{
	Clear();
	if (iTeamCount = iTeamCapacity = rCopy.iTeamCount)
		ppList = new C4Team *[iTeamCapacity];
	for (int i = 0; i < iTeamCount; i++)
		ppList[i] = new C4Team(*rCopy.ppList[i]);
	iLastTeamID = rCopy.iLastTeamID;
	fAllowHostilityChange = rCopy.fAllowHostilityChange;
	fAllowTeamSwitch = rCopy.fAllowTeamSwitch;
	fCustom = rCopy.fCustom;
	fActive = rCopy.fActive;
	eTeamDist = rCopy.eTeamDist;
	fTeamColors = rCopy.fTeamColors;
	fAutoGenerateTeams = rCopy.fAutoGenerateTeams;
	sScriptPlayerNames.Copy(rCopy.sScriptPlayerNames);
	return *this;
}

bool C4TeamList::CanLocalChooseTeam() const
{
	// only if there are any teams
	if (!fActive) return false;
	// check by mode
	switch (eTeamDist)
	{
	case TEAMDIST_Free: return true;
	case TEAMDIST_Host: return Game.Control.isCtrlHost();
	case TEAMDIST_None:
	case TEAMDIST_Random:
	case TEAMDIST_RandomInv:
		return false;
	default: assert(false); return false;
	}
}

bool C4TeamList::CanLocalChooseTeam(int32_t idPlayer) const
{
	// must be possible at all
	if (!CanLocalChooseTeam()) return false;
	// there must be space in a target team
	// always possible if teams are generated on the fly
	if (IsAutoGenerateTeams()) return true;
	// also possible if one of the teams that's not the player's is not full
	C4Team *pCurrentTeam = nullptr, *pCheck;
	if (idPlayer) pCurrentTeam = GetTeamByPlayerID(idPlayer);
	int32_t iCheckTeam = 0;
	while (pCheck = GetTeamByIndex(iCheckTeam++))
		if (pCheck != pCurrentTeam)
			if (!pCheck->IsFull())
				break;
	return !!pCheck;
}

bool C4TeamList::CanLocalSeeTeam() const
{
	if (!fActive) return false;
	// invisible teams aren't revealed before game start
	if (eTeamDist != TEAMDIST_RandomInv) return true;
	return !!Game.IsRunning;
}

void C4TeamList::AddTeam(C4Team *pNewTeam)
{
	// add team; grow vector if necessary
	if (iTeamCount >= iTeamCapacity)
	{
		C4Team **ppNewTeams = new C4Team*[iTeamCapacity = iTeamCount + 4 & ~3];
		if (iTeamCount) memcpy(ppNewTeams, ppList, iTeamCount * sizeof(C4Team *));
		delete[] ppList; ppList = ppNewTeams;
	}
	// store new team
	ppList[iTeamCount++] = pNewTeam;
	// adjust ID
	iLastTeamID = (std::max)(pNewTeam->iID, iLastTeamID);
}

void C4TeamList::ClearTeams()
{
	// delete all teams
	C4Team **ppTeam = ppList;
	if (iTeamCount) { while (iTeamCount--) delete *(ppTeam++); iTeamCount = 0; }
	iLastTeamID = 0;
}

C4Team *C4TeamList::CreateTeam(const char *szName)
{
	// custom team
	C4Team *pNewTeam = new C4Team();
	pNewTeam->iID = iLastTeamID + 1;
	pNewTeam->Name = szName;
	AddTeam(pNewTeam);
	pNewTeam->RecheckColor(*this);
	return pNewTeam;
}

bool C4TeamList::GenerateDefaultTeams(int32_t iUpToID)
{
	// generate until last team ID matches given
	while (iLastTeamID < iUpToID)
	{
		char TeamName[C4MaxName + 1];
		sprintf(TeamName, LoadResStr("IDS_MSG_TEAM"), iLastTeamID + 1);
		if (!CreateTeam(TeamName)) return false;
	}
	return true;
}

C4Team *C4TeamList::GetTeamByID(int32_t iID) const
{
	C4Team **ppCheck = ppList; int32_t iCnt = iTeamCount;
	for (; iCnt--; ++ppCheck) if ((*ppCheck)->GetID() == iID) return *ppCheck;
	return nullptr;
}

C4Team *C4TeamList::GetGenerateTeamByID(int32_t iID)
{
	// only if enabled
	if (!IsMultiTeams()) return nullptr;
	// new team?
	if (iID == TEAMID_New) iID = GetLargestTeamID() + 1;
	// find in list
	C4Team *pTeam = GetTeamByID(iID);
	if (pTeam) return pTeam;
	// not found: Generate
	GenerateDefaultTeams(iID);
	return GetTeamByID(iID);
}

C4Team *C4TeamList::GetTeamByIndex(int32_t iIndex) const
{
	// safety
	if (!Inside<int32_t>(iIndex, 0, iTeamCount - 1)) return nullptr;
	// direct list access
	return ppList[iIndex];
}

C4Team *C4TeamList::GetTeamByPlayerID(int32_t iID) const
{
	C4Team **ppCheck = ppList; int32_t iCnt = iTeamCount;
	for (; iCnt--; ++ppCheck) if ((*ppCheck)->IsPlayerIDInTeam(iID)) return *ppCheck;
	return nullptr;
}

int32_t C4TeamList::GetLargestTeamID() const
{
	int32_t iLargest = 0;
	C4Team **ppCheck = ppList; int32_t iCnt = iTeamCount;
	for (; iCnt--; ++ppCheck) iLargest = (std::max)((*ppCheck)->GetID(), iLargest);
	return iLargest;
}

C4Team *C4TeamList::GetRandomSmallestTeam() const
{
	C4Team *pLowestTeam = nullptr; int iLowestTeamCount = 0;
	C4Team **ppCheck = ppList; int32_t iCnt = iTeamCount;
	for (; iCnt--; ++ppCheck)
	{
		if ((*ppCheck)->IsFull()) continue; // do not join into full teams
		if (!pLowestTeam || pLowestTeam->GetPlayerCount() > (*ppCheck)->GetPlayerCount())
		{
			pLowestTeam = *ppCheck;
			iLowestTeamCount = 1;
		}
		else if (pLowestTeam->GetPlayerCount() == (*ppCheck)->GetPlayerCount())
			if (!SafeRandom(++iLowestTeamCount))
				pLowestTeam = *ppCheck;
	}
	return pLowestTeam;
}

bool C4TeamList::IsTeamVisible() const
{
	// teams invisible during lobby time if random surprise teams
	if (eTeamDist == TEAMDIST_RandomInv)
		if (Game.Network.isLobbyActive())
			return false;
	return true;
}

bool C4TeamList::RecheckPlayerInfoTeams(C4PlayerInfo &rNewJoin, bool fByHost)
{
	// only if enabled
	assert(IsMultiTeams());
	if (!IsMultiTeams()) return false;
	// check whether a new team is to be assigned first
	C4Team *pCurrentTeam = GetTeamByPlayerID(rNewJoin.GetID());
	int32_t idCurrentTeam = pCurrentTeam ? pCurrentTeam->GetID() : 0;
	if (rNewJoin.GetTeam())
	{
		// was that team a change to the current team?
		// no change anyway: OK, skip this info
		if (idCurrentTeam == rNewJoin.GetTeam()) return true;
		// the player had a different team assigned: Check if changes are allowed at all
		if (eTeamDist == TEAMDIST_Free || (eTeamDist == TEAMDIST_Host && fByHost))
			// also make sure that selecting this team is allowed, e.g. doesn't break the team limit
			// this also checks whether the team number is a valid team - but it would accept TEAMID_New, which shouldn't be used in player infos!
			if (rNewJoin.GetTeam() != TEAMID_New && IsJoin2TeamAllowed(rNewJoin.GetTeam()))
				// okay; accept change
				return true;
		// Reject change by reassigning the current team
		rNewJoin.SetTeam(idCurrentTeam);
		// and determine a new team, if none has been assigned yet
		if (idCurrentTeam) return true;
	}
	// new team assignment
	// teams are always needed in the lobby, so there's a team preset to change
	// for runtime joins, teams are needed if specified by teams.txt or if any teams have been created before (to avoid mixed team-noteam-scenarios)
	// but only assign teams in runtime join if the player won't pick it himself
	bool fWillHaveLobby = Game.Network.isEnabled() && !Game.Network.Status.isPastLobby() && Game.fLobby;
	bool fHasOrWillHaveLobby = Game.Network.isLobbyActive() || fWillHaveLobby;
	bool fCanPickTeamAtRuntime = !IsRandomTeam() && (rNewJoin.GetType() == C4PT_User) && IsRuntimeJoinTeamChoice();
	bool fIsTeamNeeded = IsRuntimeJoinTeamChoice() || GetTeamCount();
	if (!fHasOrWillHaveLobby && (!fIsTeamNeeded || fCanPickTeamAtRuntime)) return false;
	// get least-used team
	C4Team *pAssignTeam = nullptr;
	C4Team *pLowestTeam = GetRandomSmallestTeam();
	// melee mode
	if (IsAutoGenerateTeams() && !IsRandomTeam())
	{
		// reuse old team only if it's empty
		if (pLowestTeam && !pLowestTeam->GetPlayerCount())
			pAssignTeam = pLowestTeam;
		else
		{
			// no empty team: generate new
			GenerateDefaultTeams(iLastTeamID + 1);
			pAssignTeam = GetTeamByID(iLastTeamID);
		}
	}
	else
	{
		if (!pLowestTeam)
		{
			// not enough teams defined in teamwork mode?
			// then create two teams as default
			if (!GetTeamByIndex(1))
				GenerateDefaultTeams(2);
			else
				// otherwise, all defined teams are full. This is a scenario error, because MaxPlayer should have been adjusted
				return false;
			pLowestTeam = GetTeamByIndex(0);
		}
		pAssignTeam = pLowestTeam;
	}
	// assign it
	if (!pAssignTeam) return false;
	pAssignTeam->AddPlayer(rNewJoin, true);
	return true;
}

bool C4TeamList::IsJoin2TeamAllowed(int32_t idTeam)
{
	// join to new team: Only if new teams can be created
	if (idTeam == TEAMID_New) return IsAutoGenerateTeams();
	// team number must be valid
	C4Team *pTeam = GetTeamByID(idTeam);
	if (!pTeam) return false;
	// team player count must not exceed the limit
	return !pTeam->IsFull();
}

void C4TeamList::CompileFunc(StdCompiler *pComp)
{
	pComp->Value(mkNamingAdapt(fActive,               "Active",               true));
	pComp->Value(mkNamingAdapt(fCustom,               "Custom",               true));
	pComp->Value(mkNamingAdapt(fAllowHostilityChange, "AllowHostilityChange", false));
	pComp->Value(mkNamingAdapt(fAllowTeamSwitch,      "AllowTeamSwitch",      false));
	pComp->Value(mkNamingAdapt(fAutoGenerateTeams,    "AutoGenerateTeams",    false));
	pComp->Value(mkNamingAdapt(iLastTeamID,           "LastTeamID",           0));

	StdEnumEntry<TeamDist> TeamDistEntries[] =
	{
		{ "Free",      TEAMDIST_Free },
		{ "Host",      TEAMDIST_Host },
		{ "None",      TEAMDIST_None },
		{ "Random",    TEAMDIST_Random },
		{ "RandomInv", TEAMDIST_RandomInv },
	};
	pComp->Value(mkNamingAdapt(mkEnumAdaptT<uint8_t>(eTeamDist, TeamDistEntries), "TeamDistribution", TEAMDIST_Free));

	pComp->Value(mkNamingAdapt(fTeamColors,        "TeamColors",        false));
	pComp->Value(mkNamingAdapt(iMaxScriptPlayers,  "MaxScriptPlayers",  0));
	pComp->Value(mkNamingAdapt(sScriptPlayerNames, "ScriptPlayerNames", StdStrBuf()));

	int32_t iOldTeamCount = iTeamCount;
	pComp->Value(mkNamingCountAdapt(iTeamCount, "Team"));

	if (pComp->isCompiler())
	{
		while (iOldTeamCount--) delete ppList[iOldTeamCount];
		delete[] ppList;
		if (iTeamCapacity = iTeamCount)
		{
			ppList = new C4Team *[iTeamCapacity]{};
		}
		else
			ppList = nullptr;
	}

	if (iTeamCount)
	{
		// Force compiler to spezialize
		mkPtrAdaptNoNull(*ppList);
		// Save team list, using map-function.
		pComp->Value(mkNamingAdapt(
			mkArrayAdaptMap(ppList, iTeamCount, mkPtrAdaptNoNull<C4Team>),
			"Team"));
	}

	if (pComp->isCompiler())
	{
		// adjust last team ID, which may not be set properly for player-generated team files
		iLastTeamID = (std::max)(GetLargestTeamID(), iLastTeamID);
		// force automatic generation of teams if none are defined
		if (!iTeamCount) fAutoGenerateTeams = true;
	}
}

bool C4TeamList::Load(C4Group &hGroup, class C4Scenario *pInitDefault, class C4LangStringTable *pLang)
{
	// clear previous
	Clear();
	// load file contents
	StdStrBuf Buf;
	if (!hGroup.LoadEntryString(C4CFN_Teams, Buf))
	{
		// no teams: Try default init
		if (!pInitDefault) return false;
		// no teams defined: Activate default melee teams if a melee rule is found
		C4ID C4ID_Melee = C4Id("MELE");
		C4ID C4ID_TeamworkMelee = C4Id("MEL2"); // deprecated
		C4ID C4ID_Rivalry = C4Id("RVLR");
		// default: FFA for anything that looks like melee
		if (pInitDefault->Game.Goals.GetIDCount(C4ID_Melee, 1)
			|| pInitDefault->Game.Rules.GetIDCount(C4ID_Rivalry, 1)
			|| pInitDefault->Game.Goals.GetIDCount(C4ID_TeamworkMelee, 1)
			|| Game.C4S.Game.Mode == C4S_Melee || Game.C4S.Game.Mode == C4S_MeleeTeamwork)
		{
			fAllowHostilityChange = true;
			fActive = true;
			fAutoGenerateTeams = true;
		}
		else
		{
			// No goals/rules whatsoever: They could be present in the objects.txt, but parsing that would be a bit of
			//  overkill
			// So just keep the old behaviour here, and disallow teams
			fAllowHostilityChange = true;
			fActive = false;
		}
		fCustom = false;
	}
	else
	{
		// team definition file may be localized
		if (pLang) pLang->ReplaceStrings(Buf);
		// compile
		if (!CompileFromBuf_LogWarn<StdCompilerINIRead>(mkNamingAdapt(*this, "Teams"), Buf, C4CFN_Teams)) return false;
	}
	// post-initialization: Generate default team colors
	int32_t iTeam = 0; C4Team *pTeam;
	while (pTeam = GetTeamByIndex(iTeam++))
		pTeam->RecheckColor(*this);
	return true;
}

bool C4TeamList::Save(C4Group &hGroup)
{
	// remove previous entry from group
	hGroup.DeleteEntry(C4CFN_Teams);
	// decompile
	try
	{
		StdStrBuf Buf = DecompileToBuf<StdCompilerINIWrite>(mkNamingAdapt(*this, "Teams"));
		// save it
		hGroup.Add(C4CFN_Teams, Buf, false, true);
	}
	catch (StdCompiler::Exception *)
	{
		return false;
	}
	// done, success
	return true;
}

void C4TeamList::RecheckPlayers()
{
	C4Team **ppCheck = ppList; int32_t iCnt = iTeamCount;
	for (; iCnt--; ++ppCheck)(*ppCheck)->RecheckPlayers();
}

void C4TeamList::RecheckTeams()
{
	// automatic team distributions only
	if (!IsRandomTeam()) return;
	// host decides random teams
	if (!Game.Control.isCtrlHost()) return;
	// random teams in auto generate mode? Make sure there are exactly two teams
	if (IsAutoGenerateTeams() && GetTeamCount() != 2)
	{
		ReassignAllTeams();
		return;
	}
	// redistribute players of largest team that has relocatable players left towards smaller teams
	for (;;)
	{
		C4Team *pLowestTeam = GetRandomSmallestTeam();
		if (!pLowestTeam) break; // no teams: Nothing to re-distribute.
		// get largest team that has relocateable players
		C4Team *pLargestTeam = nullptr;
		C4Team **ppCheck = ppList; int32_t iCnt = iTeamCount;
		for (; iCnt--; ++ppCheck) if (!pLargestTeam || pLargestTeam->GetPlayerCount() > (*ppCheck)->GetPlayerCount())
			if ((*ppCheck)->GetFirstUnjoinedPlayerID())
				pLargestTeam = *ppCheck;
		// no team can redistribute?
		if (!pLargestTeam) break;
		// redistribution won't help much?
		if (pLargestTeam->GetPlayerCount() - pLowestTeam->GetPlayerCount() <= 1) break;
		// okay; redistribute one player!
		int32_t idRedistPlayer = pLargestTeam->GetFirstUnjoinedPlayerID();
		C4PlayerInfo *pInfo = Game.PlayerInfos.GetPlayerInfoByID(idRedistPlayer);
		assert(pInfo);
		if (!pInfo) break; // umn...serious problems
		pLargestTeam->RemovePlayerByID(idRedistPlayer);
		pLowestTeam->AddPlayer(*pInfo, true);
		C4ClientPlayerInfos *pClrInfo = Game.PlayerInfos.GetClientInfoByPlayerID(idRedistPlayer);
		assert(pClrInfo);
		// player info change: mark updated to remote clients get information
		if (pClrInfo)
		{
			pClrInfo->SetUpdated();
		}
	}
}

void C4TeamList::ReassignAllTeams()
{
	assert(Game.Control.isCtrlHost());
	if (!Game.Control.isCtrlHost()) return;
	// go through all player infos; reset team in them
	int32_t idStart = -1; C4PlayerInfo *pNfo;
	while (pNfo = Game.PlayerInfos.GetNextPlayerInfoByID(idStart))
	{
		idStart = pNfo->GetID();
		if (pNfo->HasJoinIssued()) continue;
		pNfo->SetTeam(0);
		// mark changed info as updated
		C4ClientPlayerInfos *pCltInfo = Game.PlayerInfos.GetClientInfoByPlayerID(idStart);
		assert(pCltInfo);
		if (pCltInfo)
		{
			pCltInfo->SetUpdated();
		}
	}
	// clear players from team lists
	RecheckPlayers();
	// in random autogenerate mode, there must be exactly two teams
	if (IsRandomTeam())
		if (IsAutoGenerateTeams() && GetTeamCount() != 2)
		{
			ClearTeams();
			GenerateDefaultTeams(2);
		}
	// reassign them
	idStart = -1;
	while (pNfo = Game.PlayerInfos.GetNextPlayerInfoByID(idStart))
	{
		idStart = pNfo->GetID();
		if (pNfo->HasJoinIssued()) continue;
		assert(!pNfo->GetTeam());
		RecheckPlayerInfoTeams(*pNfo, true);
	}
}

StdStrBuf C4TeamList::GetTeamDistName(TeamDist eTeamDist) const
{
	switch (eTeamDist)
	{
	case TEAMDIST_Free:      return (StdStrBuf(LoadResStr("IDS_MSG_TEAMDIST_FREE"),   true));
	case TEAMDIST_Host:      return (StdStrBuf(LoadResStr("IDS_MSG_TEAMDIST_HOST"),   true));
	case TEAMDIST_None:      return (StdStrBuf(LoadResStr("IDS_MSG_TEAMDIST_NONE"),   true));
	case TEAMDIST_Random:    return (StdStrBuf(LoadResStr("IDS_MSG_TEAMDIST_RND"),    true));
	case TEAMDIST_RandomInv: return (StdStrBuf(LoadResStr("IDS_MSG_TEAMDIST_RNDINV"), true));
	default: return (FormatString("TEAMDIST_undefined(%d)", (int)eTeamDist));
	}
}

void C4TeamList::FillTeamDistOptions(C4GUI::ComboBox_FillCB *pFiller) const
{
	// no teams if disabled
	if (!fActive) return;
	// team distribution options
	pFiller->AddEntry(GetTeamDistName(TEAMDIST_Free).getData(), TEAMDIST_Free);
	pFiller->AddEntry(GetTeamDistName(TEAMDIST_Host).getData(), TEAMDIST_Host);
	if (IsAutoGenerateTeams()) pFiller->AddEntry(GetTeamDistName(TEAMDIST_None).getData(), TEAMDIST_None); // no teams: only for regular melees
	pFiller->AddEntry(GetTeamDistName(TEAMDIST_Random).getData(), TEAMDIST_Random);
	pFiller->AddEntry(GetTeamDistName(TEAMDIST_RandomInv).getData(), TEAMDIST_RandomInv);
}

void C4TeamList::SendSetTeamDist(TeamDist eNewTeamDist)
{
	assert(Game.Control.isCtrlHost());
	// set it for all clients
	Game.Control.DoInput(CID_Set, new C4ControlSet(C4CVT_TeamDistribution, eNewTeamDist), CDT_Sync);
}

StdStrBuf C4TeamList::GetTeamDistString() const
{
	// return name of current team distribution setting
	return GetTeamDistName(eTeamDist);
}

bool C4TeamList::HasTeamDistOptions() const
{
	// team distribution can be changed if teams are enabled
	return fActive;
}

void C4TeamList::SetTeamDistribution(TeamDist eToVal)
{
	if (!Inside(eToVal, TEAMDIST_First, TEAMDIST_Last)) { assert(false); return; }
	eTeamDist = eToVal;
	// team distribution mode changed: Host may beed to redistribute
	if (Game.Control.isCtrlHost())
	{
		// if a random team mode was set, reassign all teams so it's really random. Also reassign in no-team-mode so enough teams for all players exist
		if (IsRandomTeam() || eTeamDist == TEAMDIST_None)
			ReassignAllTeams();
		else
		{
			// otherwise, it's sufficient to just reassign any teams that are incorrect for the current mode
			RecheckTeams();
		}
		// send updates to other clients and reset flags
		if (Game.Network.isEnabled())
		{
			Game.Network.Players.SendUpdatedPlayers();
		}
	}
}

void C4TeamList::SendSetTeamColors(bool fEnabled)
{
	// set it for all clients
	Game.Control.DoInput(CID_Set, new C4ControlSet(C4CVT_TeamColors, fEnabled), CDT_Sync);
}

void C4TeamList::SetTeamColors(bool fEnabled)
{
	// change only
	if (fEnabled == fTeamColors) return;
	// reflect change
	fTeamColors = fEnabled;
	// update colors of all players
	if (!Game.Control.isCtrlHost()) return;
	// go through all player infos; reset color in them
	Game.PlayerInfos.UpdatePlayerAttributes(); // sets team and savegame colors
	if (Game.Network.isEnabled())
	{
		// sends color updates to all clients
		Game.Network.Players.SendUpdatedPlayers();
	}
}

void C4TeamList::EnforceLeagueRules()
{
	// enforce some league settings
	fAllowTeamSwitch = false; // switching teams in league games? Yeah, sure...
}

int32_t C4TeamList::GetForcedTeamSelection(int32_t idForPlayer) const
{
	// if there's only one team for the player to join, return that team ID
	C4Team *pOKTeam = nullptr, *pCheck;
	if (idForPlayer) pOKTeam = GetTeamByPlayerID(idForPlayer); // curent team is always possible, even if full
	int32_t iCheckTeam = 0;
	while (pCheck = GetTeamByIndex(iCheckTeam++))
		if (!pCheck->IsFull())
		{
			// this team could be joined
			if (pOKTeam && pOKTeam != pCheck)
			{
				// there already was a team that could be joined
				// two alternatives -> team selection is not forced
				return 0;
			}
			pOKTeam = pCheck;
		}
	// was there a team that could be joined?
	if (pOKTeam)
	{
		// if teams are generated on the fly, there would always be the possibility of creating a new team
		if (IsAutoGenerateTeams()) return 0;
		// otherwise, this team is forced!
		return pOKTeam->GetID();
	}
	// no team could be joined: Teams auto generated?
	if (IsAutoGenerateTeams())
	{
		// then the only possible way is to join a new team
		return TEAMID_New;
	}
	// otherwise, nothing can be done...
	return 0;
}

StdStrBuf C4TeamList::GetScriptPlayerName() const
{
	// get a name to assign to a new script player. Try to avoid name conflicts
	if (!sScriptPlayerNames.getLength()) return StdStrBuf::MakeRef(LoadResStr("IDS_TEXT_COMPUTER")); // default name
	// test available script names
	int32_t iNameIdx = 0; StdStrBuf sOut;
	while (sScriptPlayerNames.GetSection(iNameIdx++, &sOut, '|'))
		if (!Game.PlayerInfos.GetActivePlayerInfoByName(sOut.getData()))
			return sOut;
	// none are available: Return a random name
	sScriptPlayerNames.GetSection(SafeRandom(iNameIdx - 1), &sOut, '|');
	return sOut;
}
