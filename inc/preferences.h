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

#define NDRIVES ('Z'-'A' + 1)
#define ATARIDRIVEISMAPPABLE(d)    ((d != 'C'-'A') && (d != 'M'-'A') && (d != 'U'-'A'))

#define ATARI_SCREEN_WIDTH_MIN      320
#define ATARI_SCREEN_WIDTH_MAX      4096
#define ATARI_SCREEN_HEIGHT_MIN     200
#define ATARI_SCREEN_HEIGHT_MAX     2048

#define ATARI_RAM_SIZE_MIN          (800*1024)          // 800 KiB ..
#define ATARI_RAM_SIZE_MAX          (2U*1024*1024*1024)  // .. 2 GiB


// Atari screen colour mode
typedef enum
{
    atariScreenMode16M = 0,     // true colour, more than 16 millions of colours
    atariScreenModeHC = 1,      // high colour, 16 bits per pixel
    atariScreenMode256 = 2,     // 256 colours with palette
    atariScreenMode16 = 3,      // 16 colours with palette, packed pixels
    atariScreenMode16ip = 4,    // 16 colours with palette, interleaved plane
    atariScreenMode4ip = 5,     // 4 colours with palette, interleaved plane
    atariScreenMode2 = 6        // monochrome
} enAtariScreenColourMode;


// drive flags
#define DRV_FLAG_RDONLY         1   ///< read-only
#define DRV_FLAG_8p3            2   ///< filenames in 8+3 format, uppercase
#define DRV_FLAG_CASE_INSENS    4   ///< case insensitive, e.g. (V)FAT or HFS(+)


/// @brief  static class, no constructor etc.
class Preferences
{
   public:
    static int init(const char *config_file,
                    int mode_override,
                    int width_override, int height_override,
                    int stretch_x_override, int stretch_y_override,
                    int memsize_override,
                    bool rewrite_conf);
    static const char *videoModeToString(enAtariScreenColourMode mode);

    static unsigned AtariMemSize;
    static char AtariLanguage[16];                  // empty for default, or EN/DE/FR
    static bool bShowHostMenu;
    static enAtariScreenColourMode atariScreenColourMode;
    static bool bHideHostMouse;
    static bool bAutoStartMagiC;
	static char AtariKernelPath[1024];              // "MAGICLIN.OS" file
	static char AtariRootfsPath[1024];              // Atari C:
    static bool AtariHostHome;                      // Atari H: is home
    static bool AtariHostHomeRdOnly;                // Atari H: is write protected
    static bool AtariHostRoot;                      // Atari M: as host root
    static bool AtariHostRootRdOnly;                // Atari M: is write protected
	static char AtariScrapFileUnixPath[1024];       // the clipboard file is called "scrap" in GEM
	static char AtariTempFilesUnixPath[1024];
    static char szPrintingCommand[256];
    static char szAuxPath[256];
    static unsigned Monitor;                        // 0: default
    static int AtariScreenX;                        // -1: auto
    static int AtariScreenY;                        // -1: auto
    static unsigned AtariScreenWidth;               // 320..4096
    static unsigned AtariScreenHeight;              // 200..2048
    static unsigned AtariScreenStretchX;            // horizontal stretch
    static unsigned AtariScreenStretchY;            // vertical stretch
    static unsigned ScreenRefreshFrequency;
    //static bool m_bPPC_VDI_Patch;                 // used for native VDI output on PPC

    static const char *drvPath[NDRIVES];
    static unsigned drvFlags[NDRIVES];              // see above (read-only, ...)

    static const char *getDrvPath(unsigned drv)
    {
        return (drv < NDRIVES) ? drvPath[drv] : nullptr;
    }

    // drive paths are dynamically allocated
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

   private:
    static int getPreferences(const char *cfgfile, bool rewrite_conf);
    static int writePreferences(const char *cfgfile);
    static int evaluatePreferencesLine(const char *line);
};
#endif
