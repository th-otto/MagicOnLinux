/*
 * Copyright (C) 1990-2025 Andreas Kromke, andreas.kromke@gmail.com
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
* Manages all global variables
*
*/

#include "config.h"
// System-Header
#include <sys/stat.h>
#include <endian.h>
// Programm-Header
#include "Globals.h"
#include "Debug.h"

const char scrapFileName[] = "/GEMSYS/GEMSCRAP/SCRAP.TXT";


bool CGlobals::s_bRunning;
//ProcessInfoRec CGlobals::s_ProcessInfo;
char CGlobals::s_atariKernelPath[1024];
char CGlobals::s_atariRootfsPath[1024];
char CGlobals::s_atariScrapFileUnixPath[1024];

CMyPreferences CGlobals::s_Preferences;
bool CGlobals::s_XFSDrvWasChanged[NDRIVES];
bool CGlobals::s_bShowMacMenu;
bool CGlobals::s_bAtariScreenManualSize;
unsigned short CGlobals::s_AtariScreenX;
unsigned short CGlobals::s_AtariScreenY;
unsigned short CGlobals::s_AtariScreenWidth;
unsigned short CGlobals::s_AtariScreenHeight;

CGlobals Globals;


/*****************************************************************
*
* Konstruktor. Wird VOR (!) main() aufgerufen.
* Achtung: Wird dieser ganze Sermon NICHT gemacht, knallt es!
*
******************************************************************/

CGlobals::CGlobals()
{
//	InitCursor ();
//	RegisterAppearanceClient();
}

/**********************************************************************
*
* (STATIC) Zeichenketten aus Programm-Ressource ermitteln
*
**********************************************************************/

/* ersetzt durch "localizable.strings"
void CGlobals::GetRsrcStr(const unsigned char * name, char *s)
{
	Handle hdl;
	short oldres;

	oldres = CurResFile();			// aktuelle Resourcedatei retten
	UseResFile(s_ThisResFile);		// Programm durchsuchen
	hdl = Get1NamedResource('STR ', name);	// Resource suchen
	if	(hdl)
	{
		p2cstrcpy(s, (StringPtr) *hdl);
		ReleaseResource(hdl);
	}
	else
	{
		s[0] = '\0';
		DebugError("CGlobals::GetRsrcStr() -- Zeichenkette %s nicht gefunden", name);
	}
	UseResFile(oldres);
}
*/

#if 0
/*****************************************************************
*
*  explizite erste Initialisierung
*  Mu� vor Init() aufgerufen werden. W�re vielleicht besser
*  im Konstruktor aufgehoben?
*
******************************************************************/

void CGlobals::InitDirectories(void)
{
	GetExeLocation(&s_ownPSN, &s_ProcessInfo, &s_ProcDir, &s_ProcDirID, &s_ExecutableDirID);
}
#endif


/*****************************************************************
*
*  explizite Initialisierung
*
******************************************************************/

int CGlobals::Init(void)
{
	strcpy(s_atariKernelPath, "/home/and/Documents/Atari-rootfs/MagicMacX.OS");
	strcpy(s_atariRootfsPath, "/home/and/Documents/Atari-rootfs");
	strcpy(s_atariScrapFileUnixPath, s_atariRootfsPath);
	strcat(s_atariScrapFileUnixPath, scrapFileName);
	strcpy(s_atariTempFilesUnixPath, "/tmp");
	return 0;
}


/******************************************************************************
*
*  @brief Get DOS path (M:\xxxx) from host path
*
******************************************************************************/

int CGlobals::getDosPath
(
	const char *hostPath,
	char *pBuf,
	unsigned uBufLen
)
{
	static const char *dos_root = "M:";

	if ((hostPath == nullptr) || (hostPath[0] != '/'))
	{
		fprintf(stderr, "invalid host path\n");
		return -1;
	}

	if (strlen(hostPath) + strlen(dos_root) + 1 > uBufLen)
	{
		fprintf(stderr, "host path too long\n");
		return -1;
	}

	strcpy(pBuf, dos_root);
	char *dest = pBuf + strlen(pBuf);
	while(*hostPath)
	{
		char c = *hostPath++;
		if (c == '/')
		{
			c = '\\';		// convert path separators from standard to Microsoft
		}
		*dest++ = c;
	}
	*dest = '\0';

	return 0;
}

/**//**************************************************************************
*
*  @brief Get file size from host path
*
******************************************************************************/

int64_t getFileSize(const char *hostPath)
{
	int ret;
	struct stat statbuf;

	ret = stat(hostPath, &statbuf);
	if (ret == 0)
	{
		return statbuf.st_size;
	}

	return -1;
}
