/*
 * LegacyClonk
 *
 * Copyright (c) RedWolf Design
 * Copyright (c) 2013-2017, The OpenClonk Team and contributors
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

#include "C4Include.h"
#ifdef C4ENGINE
#include "C4Game.h"
#endif
#include "C4Version.h"
#include "C4Network2Reference.h"

#include <fcntl.h>

// *** C4Network2Reference

C4Network2Reference::C4Network2Reference()
	: Icon(0), Time(0), Frame(0), StartTime(0), LeaguePerformance(0),
	JoinAllowed(true), ObservingAllowed(true), PasswordNeeded(false), OfficialServer(false),
	iAddrCnt(0), NetpuncherGameID{} {}

C4Network2Reference::~C4Network2Reference() {}

void C4Network2Reference::SetSourceAddress(const C4NetIO::EndpointAddress &ip)
{
	source = ip;
	for (int i = 0; i < iAddrCnt; i++)
	{
		if (Addrs[i].GetAddr().IsNullHost())
		{
			Addrs[i].GetAddr().SetHost(ip);
		}
	}
}

#ifdef C4ENGINE
void C4Network2Reference::InitLocal(C4Game *pGame)
{
	// Copy all game parameters
	Parameters = pGame->Parameters;

	// Discard player resources (we don't want these infos in the reference)
	// Add league performance (but only after game end)
	C4ClientPlayerInfos *pClientInfos; C4PlayerInfo *pPlayerInfo;
	int32_t i, j;
	for (i = 0; pClientInfos = Parameters.PlayerInfos.GetIndexedInfo(i); i++)
		for (j = 0; pPlayerInfo = pClientInfos->GetPlayerInfo(j); j++)
		{
			pPlayerInfo->DiscardResource();
			if (pGame->GameOver)
				pPlayerInfo->SetLeaguePerformance(
					pGame->RoundResults.GetLeaguePerformance(pPlayerInfo->GetID()));
		}

	// Special additional information in reference
	Icon = pGame->C4S.Head.Icon;
	GameStatus = pGame->Network.Status;
	Time = pGame->Time;
	Frame = pGame->FrameCounter;
	StartTime = pGame->StartTime;
	LeaguePerformance = pGame->RoundResults.GetLeaguePerformance();
	Comment = Config.Network.Comment;
	JoinAllowed = pGame->Network.isJoinAllowed();
	ObservingAllowed = pGame->Network.isObservingAllowed();
	PasswordNeeded = pGame->Network.isPassworded();
	NetpuncherGameID = pGame->Network.getNetpuncherGameID();
	NetpuncherAddr = pGame->Network.getNetpuncherAddr();
	Game.Set();

	// Addresses
	C4Network2Client *pLocalNetClient = pGame->Clients.getLocal()->getNetClient();
	iAddrCnt = pLocalNetClient->getAddrCnt();
	for (i = 0; i < iAddrCnt; i++)
		Addrs[i] = pLocalNetClient->getAddr(i);
}
#endif

void C4Network2Reference::CompileFunc(StdCompiler *pComp)
{
	pComp->Value(mkNamingAdapt(Icon,                                               "Icon",              0));
	pComp->Value(mkParAdapt(GameStatus, true));
	pComp->Value(mkNamingAdapt(Time,                                               "Time",              0));
	pComp->Value(mkNamingAdapt(Frame,                                              "Frame",             0));
	pComp->Value(mkNamingAdapt(StartTime,                                          "StartTime",         0));
	pComp->Value(mkNamingAdapt(LeaguePerformance,                                  "LeaguePerformance", 0));
	pComp->Value(mkNamingAdapt(Comment,                                            "Comment",           ""));
	pComp->Value(mkNamingAdapt(JoinAllowed,                                        "JoinAllowed",       true));
	pComp->Value(mkNamingAdapt(ObservingAllowed,                                   "ObservingAllowed",  true));
	pComp->Value(mkNamingAdapt(PasswordNeeded,                                     "PasswordNeeded",    false));
	// Ignore RegJoinOnly
	bool RegJoinOnly = false;
	pComp->Value(mkNamingAdapt(RegJoinOnly,                                        "RegJoinOnly",       false));
	pComp->Value(mkNamingAdapt(mkIntPackAdapt(iAddrCnt),                           "AddressCount",      0));
	iAddrCnt = std::min<uint8_t>(C4ClientMaxAddr, iAddrCnt);
	pComp->Value(mkNamingAdapt(mkArrayAdapt(Addrs, iAddrCnt, C4Network2Address()), "Address"));
	pComp->Value(mkNamingAdapt(Game.sEngineName,                                   "Game",              "None"));
	pComp->Value(mkNamingAdapt(mkArrayAdaptDM(Game.iVer, 0),                       "Version"));
	pComp->Value(mkNamingAdapt(Game.iBuild,                                        "Build",             -1));
	pComp->Value(mkNamingAdapt(OfficialServer,                                     "OfficialServer",    false));

	pComp->Value(Parameters);

	pComp->Value(mkNamingAdapt(NetpuncherGameID,                                   "NetpuncherID",      C4NetpuncherID(), false, false));
	pComp->Value(mkNamingAdapt(NetpuncherAddr,                                     "NetpuncherAddr",    "", false, false));
}

int32_t C4Network2Reference::getSortOrder() const // Don't go over 100, because that's for the masterserver...
{
	C4GameVersion verThis;
	int iOrder = 0;
	// Official server
	if (isOfficialServer() && !Config.Network.UseAlternateServer) iOrder += 50;
	// Joinable
	if (isJoinAllowed() && (getGameVersion() == verThis)) iOrder += 25;
	// League game
	if (Parameters.isLeague()) iOrder += 5;
	// In lobby
	if (getGameStatus().isLobbyActive()) iOrder += 3;
	// No password needed
	if (!isPasswordNeeded()) iOrder += 1;
	// Done
	return iOrder;
}

// *** C4Network2RefServer

C4Network2RefServer::C4Network2RefServer()
	: pReference(nullptr) {}

C4Network2RefServer::~C4Network2RefServer()
{
	Clear();
}

void C4Network2RefServer::Clear()
{
	C4NetIOTCP::Close();
	delete pReference; pReference = nullptr;
}

void C4Network2RefServer::SetReference(C4Network2Reference *pNewReference)
{
	CStdLock RefLock(&RefCSec);
	delete pReference; pReference = pNewReference;
}

void C4Network2RefServer::PackPacket(const C4NetIOPacket &rPacket, StdBuf &rOutBuf)
{
	// Just append the packet
	rOutBuf.Append(rPacket);
}

size_t C4Network2RefServer::UnpackPacket(const StdBuf &rInBuf, const C4NetIO::addr_t &addr)
{
	const char *pData = rInBuf.getPtr<char>();
	// Check for complete header
	const char *pHeaderEnd = strstr(pData, "\r\n\r\n");
	if (!pHeaderEnd)
		return 0;
	// Check method (only GET is allowed for now)
	if (!SEqual2(pData, "GET "))
	{
		RespondMethodNotAllowed(addr);
		return rInBuf.getSize();
	}
	// Check target
	// TODO
	RespondReference(addr);
	return rInBuf.getSize();
}

void C4Network2RefServer::RespondMethodNotAllowed(const C4NetIO::addr_t &addr)
{
	// Send the message
	const std::string data{"HTTP/1.0 405 Method Not Allowed\r\n\r\n"};
	Send(C4NetIOPacket(data.c_str(), data.size(), false, addr));
	// Close the connection
	Close(addr);
}

void C4Network2RefServer::RespondReference(const C4NetIO::addr_t &addr)
{
	CStdLock RefLock(&RefCSec);
	// Pack
	StdStrBuf PacketData = DecompileToBuf<StdCompilerINIWrite>(mkNamingPtrAdapt(pReference, "Reference"));
	// Create header
	const char *szCharset = GetCharsetCodeName(Config.General.LanguageCharset);
	StdStrBuf Header = FormatString(
		"HTTP/1.0 200 OK\r\n"
		"Content-Length: %zu\r\n"
		"Content-Type: text/plain; charset=%s\r\n"
		"Server: " C4ENGINENAME "/" C4VERSION "\r\n"
		"\r\n",
		PacketData.getLength(),
		szCharset);
	// Send back
	Send(C4NetIOPacket(Header.getData(), Header.getLength(), false, addr));
	Send(C4NetIOPacket(PacketData.getData(), PacketData.getLength(), false, addr));
	// Close the connection
	Close(addr);
}

// *** C4Network2HTTPClient

C4Network2HTTPClient::C4Network2HTTPClient()
	: fBusy(false), fSuccess(false), fConnected(false), iDownloadedSize(0), iTotalSize(0), fBinary(false), iDataOffset(0),
	pNotify(nullptr)
{
	C4NetIOTCP::SetCallback(this);
}

C4Network2HTTPClient::~C4Network2HTTPClient() {}

void C4Network2HTTPClient::PackPacket(const C4NetIOPacket &rPacket, StdBuf &rOutBuf)
{
	// Just append the packet
	rOutBuf.Append(rPacket);
}

size_t C4Network2HTTPClient::UnpackPacket(const StdBuf &rInBuf, const C4NetIO::addr_t &addr)
{
	// since new data arrived, increase timeout time
	ResetRequestTimeout();
	// Check for complete header
	if (!iDataOffset)
	{
		// Copy data into string buffer (terminate)
		StdStrBuf Data; Data.Copy(rInBuf.getPtr<char>(), rInBuf.getSize());
		const char *pData = Data.getData();
		// Header complete?
		const char *pContent = SSearch(pData, "\r\n\r\n");
		if (!pContent)
			return 0;
		// Read the header
		if (!ReadHeader(Data))
		{
			fBusy = fSuccess = false;
			Close(addr);
			return rInBuf.getSize();
		}
	}
	iDownloadedSize = rInBuf.getSize() - iDataOffset;
	// Check if the packet is complete
	if (iTotalSize > iDownloadedSize)
	{
		return 0;
	}
	// Get data, uncompress it if needed
	StdBuf Data = rInBuf.getPart(iDataOffset, iTotalSize);
	if (fCompressed)
		if (!Decompress(&Data))
		{
			fBusy = fSuccess = false;
			Close(addr);
			return rInBuf.getSize();
		}
	// Save the result
	if (fBinary)
		ResultBin.Copy(Data);
	else
		ResultString.Copy(Data.getPtr<char>(), Data.getSize());
	fBusy = false; fSuccess = true;
	// Callback
	OnPacket(C4NetIOPacket(Data, addr), this);
	// Done
	Close(addr);
	return rInBuf.getSize();
}

bool C4Network2HTTPClient::ReadHeader(const StdStrBuf &Data)
{
	const char *pData = Data.getData();
	const char *pContent = SSearch(pData, "\r\n\r\n");
	if (!pContent)
		return 0;
	// Parse header line
	int iHTTPVer1, iHTTPVer2, iResponseCode, iStatusStringPtr;
	if (sscanf(pData, "HTTP/%d.%d %d %n", &iHTTPVer1, &iHTTPVer2, &iResponseCode, &iStatusStringPtr) != 3)
	{
		Error = "Invalid status line!";
		return false;
	}
	// Check HTTP version
	if (iHTTPVer1 != 1)
	{
		Error.Format("Unsupported HTTP version: %d.%d!", iHTTPVer1, iHTTPVer2);
		return false;
	}
	// Check code
	if (iResponseCode != 200)
	{
		// Get status string
		StdStrBuf StatusString; StatusString.CopyUntil(pData + iStatusStringPtr, '\r');
		// Create error message
		Error.Format("HTTP server responded %d: %s", iResponseCode, StatusString.getData());
		return false;
	}
	// Get content length
	const char *pContentLength = SSearch(pData, "\r\nContent-Length:");
	int iContentLength;
	if (!pContentLength || pContentLength > pContent ||
		sscanf(pContentLength, "%d", &iContentLength) != 1)
	{
		Error.Format("Invalid server response: Content-Length is missing!");
		return false;
	}
	iTotalSize = iContentLength;
	iDataOffset = (pContent - pData);
	// Get content encoding
	const char *pContentEncoding = SSearch(pData, "\r\nContent-Encoding:");
	if (pContentEncoding)
	{
		while (*pContentEncoding == ' ') pContentEncoding++;
		StdStrBuf Encoding; Encoding.CopyUntil(pContentEncoding, '\r');
		if (Encoding == "gzip")
			fCompressed = true;
		else
			fCompressed = false;
	}
	else
		fCompressed = false;
	// Okay
	return true;
}

bool C4Network2HTTPClient::Decompress(StdBuf *pData)
{
	size_t iSize = pData->getSize();
	// Create buffer
	uint32_t iOutSize = *pData->getPtr<uint32_t>(pData->getSize() - sizeof(uint32_t));
	iOutSize = std::min<uint32_t>(iOutSize, iSize * 1000);
	StdBuf Out; Out.New(iOutSize);
	// Prepare stream
	z_stream zstrm{};
	zstrm.next_in = const_cast<Byte *>(pData->getPtr<Byte>());
	zstrm.avail_in = pData->getSize();
	zstrm.next_out = Out.getMPtr<Byte>();
	zstrm.avail_out = Out.getSize();
	// Inflate...
	if (inflateInit2(&zstrm, 15 + 16) != Z_OK)
	{
		Error.Format("Could not decompress data!");
		return false;
	}
	// Inflate!
	if (inflate(&zstrm, Z_FINISH) != Z_STREAM_END)
	{
		inflateEnd(&zstrm);
		Error.Format("Could not decompress data!");
		return false;
	}
	// Return the buffer
	Out.SetSize(zstrm.total_out);
	pData->Take(Out);
	// Okay
	inflateEnd(&zstrm);
	return true;
}

bool C4Network2HTTPClient::OnConn(const C4NetIO::addr_t &AddrPeer, const C4NetIO::addr_t &AddrConnect, const C4NetIO::addr_t *pOwnAddr, C4NetIO *pNetIO)
{
	// Make sure we're actually waiting for this connection
	if (fConnected || (AddrConnect != ServerAddr && AddrConnect != ServerAddrFallback))
		return false;
	// Save pack peer address
	PeerAddr = AddrPeer;
	// Send the request
	if (!Send(C4NetIOPacket(Request, AddrPeer)))
	{
		Error.Format("Unable to send HTTP request: %s", Error.getData());
	}
	Request.Clear();
	fConnected = true;
	return true;
}

void C4Network2HTTPClient::OnDisconn(const C4NetIO::addr_t &AddrPeer, C4NetIO *pNetIO, const char *szReason)
{
	// Got no complete packet? Failure...
	if (!fSuccess && Error.isNull())
	{
		fBusy = false;
		Error.Format("Unexpected disconnect: %s", szReason);
	}
	fConnected = false;
	// Notify
	if (pNotify)
		pNotify->PushEvent(Ev_HTTP_Response, this);
}

void C4Network2HTTPClient::OnPacket(const class C4NetIOPacket &rPacket, C4NetIO *pNetIO)
{
	// Everything worthwhile was already done in UnpackPacket. Only do notify callback
	if (pNotify)
		pNotify->PushEvent(Ev_HTTP_Response, this);
}

bool C4Network2HTTPClient::Execute(int iMaxTime)
{
	// Check timeout
	if (fBusy)
	{
		if (C4TimeMilliseconds::Now() > HappyEyeballsTimeout)
		{
			HappyEyeballsTimeout = C4TimeMilliseconds::PositiveInfinity;
			Application.InteractiveThread.ThreadLogSF("HTTP: Starting fallback connection to %s (%s)", Server.getData(), ServerAddrFallback.ToString().getData());
			Connect(ServerAddrFallback);
		}

		if (time(nullptr) > iRequestTimeout)
		{
			Cancel("Request timeout");
			return true;
		}
	}
	// Execute normally
	return C4NetIOTCP::Execute(iMaxTime);
}

int C4Network2HTTPClient::GetTimeout()
{
	if (!fBusy)
		return C4NetIOTCP::GetTimeout();
	return MaxTimeout(C4NetIOTCP::GetTimeout(), static_cast<int>(1000 * std::max<time_t>(time(nullptr) - iRequestTimeout, 0)));
}

bool C4Network2HTTPClient::Query(const StdBuf &Data, bool fBinary)
{
	if (Server.isNull()) return false;
	// Cancel previous request
	if (fBusy)
		Cancel("Cancelled");
	// No result known yet
	ResultString.Clear();
	// store mode
	this->fBinary = fBinary;
	// Create request
	const char *szCharset = GetCharsetCodeName(Config.General.LanguageCharset);
	StdStrBuf Header;
	if (Data.getSize())
		Header.Format(
			"POST %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"Connection: Close\r\n"
			"Content-Length: %zu\r\n"
			"Content-Type: text/plain; encoding=%s\r\n"
			"Accept-Charset: %s\r\n"
			"Accept-Encoding: gzip\r\n"
			"Accept-Language: %s\r\n"
			"User-Agent: " C4ENGINENAME "/" C4VERSION "\r\n"
			"\r\n",
			RequestPath.getData(),
			Server.getData(),
			Data.getSize(),
			szCharset,
			szCharset,
			Config.General.LanguageEx);
	else
		Header.Format(
			"GET %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"Connection: Close\r\n"
			"Accept-Charset: %s\r\n"
			"Accept-Encoding: gzip\r\n"
			"Accept-Language: %s\r\n"
			"User-Agent: " C4ENGINENAME "/" C4VERSION "\r\n"
			"\r\n",
			RequestPath.getData(),
			Server.getData(),
			szCharset,
			Config.General.LanguageEx);
	// Compose query
	Request.Take(Header.GrabPointer(), Header.getLength());
	Request.Append(Data);

	bool enableFallback{!ServerAddrFallback.IsNull()};
	// Start connecting
	if (!Connect(ServerAddr))
	{
		if (!enableFallback)
		{
			return false;
		}

		std::swap(ServerAddr, ServerAddrFallback);
		enableFallback = false;
		if (!Connect(ServerAddr))
		{
			return false;
		}
	}
	if (enableFallback)
	{
		HappyEyeballsTimeout = C4TimeMilliseconds::Now() + C4Network2HTTPHappyEyeballsTimeout;
	}
	else
	{
		HappyEyeballsTimeout = C4TimeMilliseconds::PositiveInfinity;
	}

	// Okay, request will be performed when connection is complete
	fBusy = true;
	iDataOffset = 0;
	ResetRequestTimeout();
	ResetError();
	return true;
}

void C4Network2HTTPClient::ResetRequestTimeout()
{
	// timeout C4Network2HTTPQueryTimeout seconds from this point
	iRequestTimeout = time(nullptr) + C4Network2HTTPQueryTimeout;
}

void C4Network2HTTPClient::Cancel(const char *szReason)
{
	// Close connection - and connection attempt
	Close(ServerAddr);
	Close(ServerAddrFallback);
	Close(PeerAddr);

	// Reset flags
	fBusy = fSuccess = fConnected = fBinary = false;
	iDownloadedSize = iTotalSize = iDataOffset = 0;
	Error = szReason;
}

void C4Network2HTTPClient::Clear()
{
	fBusy = fSuccess = fConnected = fBinary = false;
	iDownloadedSize = iTotalSize = iDataOffset = 0;
	ResultBin.Clear();
	ResultString.Clear();
	Error.Clear();
}

bool C4Network2HTTPClient::SetServer(const char *szServerAddress)
{
	// Split address
	const char *pRequestPath;
	if (pRequestPath = strchr(szServerAddress, '/'))
	{
		Server.CopyUntil(szServerAddress, '/');
		RequestPath = pRequestPath;
	}
	else
	{
		Server = szServerAddress;
		RequestPath = "/";
	}
	// Resolve address
	ServerAddr.SetAddress(Server);
	if (ServerAddr.IsNull())
	{
		SetError(FormatString("Could not resolve server address %s!", Server.getData()).getData());
		return false;
	}
	ServerAddr.SetDefaultPort(GetDefaultPort());

	if (ServerAddr.GetFamily() == C4NetIO::HostAddress::IPv6)
	{
		// Try to find a fallback IPv4 address for Happy Eyeballs.
		ServerAddrFallback.SetAddress(Server, C4NetIO::HostAddress::IPv4);
		ServerAddrFallback.SetDefaultPort(GetDefaultPort());
	}
	else
	{
		ServerAddrFallback.Clear();
	}

	// Remove port
	const auto &firstColon = std::strchr(Server.getData(), ':');
	const auto &lastColon = std::strrchr(Server.getData(), ':');
	if (firstColon)
	{
		// Hostname/IPv4 address or IPv6 address with port (e.g. [::1]:1234)
		if (firstColon == lastColon || (Server[0] == '[' && lastColon[-1] == ']'))
			Server.SetLength(lastColon - Server.getData());
	}

	// Done
	ResetError();
	return true;
}

// *** C4Network2RefClient

bool C4Network2RefClient::QueryReferences()
{
	// invalidate version
	fVerSet = false;
	// Perform an Query query
	return Query(nullptr, false);
}

bool C4Network2RefClient::GetReferences(C4Network2Reference ** &rpReferences, int32_t &rRefCount)
{
	// Sanity check
	if (isBusy() || !isSuccess()) return false;
	// Parse response
	MasterVersion.Set("", 0, 0, 0, 0, 0);
	fVerSet = false;
	// local update test
	try
	{
		// Create compiler
		StdCompilerINIRead Comp;
		Comp.setInput(ResultString);
		Comp.Begin();
		// get current version
		Comp.Value(mkNamingAdapt(mkInsertAdapt(mkInsertAdapt(mkInsertAdapt(
			mkNamingAdapt(mkParAdapt(MasterVersion, false), "Version"),
			mkNamingAdapt(mkParAdapt(MessageOfTheDay, StdCompiler::RCT_All), "MOTD", "")),
			mkNamingAdapt(mkParAdapt(MessageOfTheDayHyperlink, StdCompiler::RCT_All), "MOTDURL", "")),
			mkNamingAdapt(mkParAdapt(LeagueServerRedirect, StdCompiler::RCT_All), "LeagueServerRedirect", "")),
			C4ENGINENAME));
		// Read reference count
		Comp.Value(mkNamingCountAdapt(rRefCount, "Reference"));
		// Create reference array and initialize
		rpReferences = new C4Network2Reference *[rRefCount];
		for (int i = 0; i < rRefCount; i++)
			rpReferences[i] = nullptr;
		// Get references
		Comp.Value(mkNamingAdapt(mkArrayAdaptMap(rpReferences, rRefCount, mkPtrAdaptNoNull<C4Network2Reference>), "Reference"));
		mkPtrAdaptNoNull<C4Network2Reference>(*rpReferences);
		// Done
		Comp.End();
	}
	catch (const StdCompiler::Exception &e)
	{
		SetError(e.Msg.getData());
		return false;
	}
	// Set source ip
	for (int i = 0; i < rRefCount; i++)
		rpReferences[i]->SetSourceAddress(getServerAddress());
	// validate version
	if (MasterVersion.iVer[0]) fVerSet = true;
	// Done
	ResetError();
	return true;
}
