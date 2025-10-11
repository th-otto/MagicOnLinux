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
* Preferences for application
*
*/

#ifndef _INCLUDED_MYPREFERENCES_H
#define _INCLUDED_MYPREFERENCES_H

#include <string.h>
#include <stdlib.h>

#define PROGRAM_VERSION_MAJOR 0
#define PROGRAM_VERSION_MINOR 1

#define NDRIVES ('Z'-'A' + 1)
#define ATARIDRIVEISMAPPABLE(d)    ((d != 'C'-'A') && (d != 'M'-'A') && (d != 'U'-'A'))

// Atari screen colour mode
typedef enum
{
    atariScreenMode16M = 0,        // true colour, more than 16 millions of colours
    atariScreenModeHC = 1,        // high colour, 16 bits per pixel
    atariScreenMode256 = 2,        // 256 colours with palette
    atariScreenMode16 = 3,        // 16 colours with palette, packed pixels
    atariScreenMode16ip = 4,    // 16 colours with palette, interleaved plane
    atariScreenMode4ip = 5,        // 4 colours with palette, interleaved plane
    atariScreenMode2 = 6        // monochrome
} enAtariScreenColourMode;


/// @brief  static class, no constructor etc.
class Preferences
{
   public:
    // Initialisierung
    static int Init(void);
    // Alle Einstellungen holen
    static int GetPreferences(void);
    static void Update_Monitor(void);
    static void Update_AtariMem(void);
    static void Update_GeneralSettings(void);
    static void Update_Drives(void);
    static void Update_PrintingCommand(void);
    static void Update_AuxPath(void);
    static void Update_AtariScreen(void);

    // Variablen
    static unsigned AtariMemSize;
    static unsigned AtariLanguage;
    static bool bShowMacMenu;
    static enAtariScreenColourMode atariScreenColourMode;
    static bool bHideHostMouse;
    static bool bAutoStartMagiC;
    static unsigned drvFlags[NDRIVES];    // 1 == RevDir / 2 == 8+3
    static unsigned KeyCodeForRightMouseButton;
	static char AtariKernelPath[1024];              // "MagicMacX.OS" file
	static char AtariRootfsPath[1024];              // Atari C:
	static char AtariScrapFileUnixPath[1024];
	static char AtariTempFilesUnixPath[1024];
    static char szPrintingCommand[256];
    static char szAuxPath[256];
    static int Monitor;        // 0 == Hauptbildschirm
    static bool bAtariScreenManualSize;
    static unsigned AtariScreenX;
    static unsigned AtariScreenY;
    static unsigned AtariScreenWidth;      // 320..4096
    static unsigned AtariScreenHeight;     // 200..2048
    static unsigned AtariScreenStretchX;     // horizontal stretch
    static unsigned AtariScreenStretchY;     // vertical stretch
    static unsigned ScreenRefreshFrequency;
    //static bool m_bPPC_VDI_Patch;

    static const char *drvPath[NDRIVES];

    static const char *getDrvPath(unsigned drv)
    {
        return (drv < NDRIVES) ? drvPath[drv] : nullptr;
    }

    static void setDrvPath(unsigned drv, const char *path)
    {
        if (drv < NDRIVES)
        {
            if (drvPath[drv] != nullptr)
            {
                free((void *) drvPath[drv]);
                drvPath[drv] = nullptr;
            }
            if (path != nullptr)
            {
                size_t len = strlen(path) + 1;
                char *temp = (char *) malloc(len);
                memcpy(temp, path, len);
                drvPath[drv] = temp;
            }
        }
    }
};
#endif
