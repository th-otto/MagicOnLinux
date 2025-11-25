/*
 * Copyright (C) 1990-2018 Andreas Kromke, andreas.kromke@gmail.com
 *
 * This program is free software; you can redistribute it or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
*
* Manages debug output
*
*/

#if !defined(NDEBUG)
// System-Header
#include <stdio.h>
#include <string.h>
#include <time.h>
// Programm-Header
#include "Debug.h"

// Schalter

FILE *CDebug::dbgFile = nullptr;
int CDebug::GeneralPurposeVariable = 0;


/**********************************************************************
*
* Initialisierung
*
**********************************************************************/

void CDebug::_DebugInit(const char *debugFileName)
{
	if (debugFileName != nullptr)
	{
		dbgFile = fopen(debugFileName, "wt");
	}
}


/**********************************************************************
*
* Haupt-Ausgabe-Routine
*
**********************************************************************/

void CDebug::_DebugPrint(const char *head, const char *format, va_list arglist)
{
	char line[1024];
	char *s;

	time_t t = time(nullptr);
	struct tm tm;
	(void) localtime_r(&t, &tm);
	sprintf(line, "(%02d:%02d:%02d) %s", tm.tm_hour, tm.tm_min, tm.tm_sec, head);

	s = line + strlen(line);
	vsprintf(s, format, arglist);
	// Steuerzeichen entfernen
	while((s = strchr(line, '\r')) != NULL)
	{
		memmove(s, s+1, strlen(s+1) + 1);
	}

	while((s = strchr(line, '\n')) != NULL)
	{
		memmove(s, s+1, strlen(s+1) + 1);
	}

	if (dbgFile != nullptr)
	{
		// Zeilenende
		strcat(line, "\r\n");
		(void) fwrite(line, 1, strlen(line), dbgFile);
	}
	else
	{
		fprintf(stderr, "%s\n", line);
	}
}


/**********************************************************************
*
* Ausgaben
*
**********************************************************************/

void CDebug::_DebugInfo(const char *format, ...)
{
	va_list arglist;
	va_start(arglist, format);

	_DebugPrint("DBG-INF ", format, arglist);
}

void CDebug::_DebugWarning(const char *format, ...)
{
	va_list arglist;
	va_start(arglist, format);

	_DebugPrint("DBG-WRN ", format, arglist);
}

void CDebug::_DebugError(const char *format, ...)
{
	va_list arglist;
	va_start(arglist, format);

	_DebugPrint("DBG-ERR ", format, arglist);
}
#endif
