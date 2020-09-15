/*
 * LegacyClonk
 *
 * Copyright (c) 2020, The LegacyClonk Team and contributors
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

#include "Standard.h"
#include "StdBuf.h"

template<class ...Args>
bool LogF(const char *const fmt, Args &&...args)
{
	return Log(FormatString(fmt, std::forward<Args>(args)...).c_str());
}

template<class ...Args>
bool LogSilentF(const char *const fmt, Args &&...args)
{
	return LogSilent(FormatString(fmt, std::forward<Args>(args)...).c_str());
}
