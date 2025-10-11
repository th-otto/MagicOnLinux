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

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include "preferences.h"


#define MAX_ATARIMEMSIZE	(2U*1024U*1024U*1024U)		// 2 Gigabytes

// Schalter

#define MONITOR                "AtariMonitor"
#define ATARIMEM            "AtariMemSizeInKB"
#define SHOWMACMOUSE        "ShowMacMouse"
#define AUTOSTARTMAGIC        "AutoStartMagiC"
#define KEYCODEFORRIGHTMOUSEBUTTON    "KeyCodeForRightMouseButton"
#define PRINTINGCOMMAND        "PrintingCommand"
#define AUXPATH                "AuxPath"
#define SHOWMACMENU            "ShowMacMenu"
#define SETSCREENSIZEMANUALLY    "SetAtariScreenSizeManually"
#define SETSCREENSIZE_X        "SetAtariScreenSizeX"
#define SETSCREENSIZE_Y        "SetAtariScreenSizeY"
#define SETSCREENSIZE_WIDTH        "SetAtariScreenSizeWidth"
#define SETSCREENSIZE_HEIGHT    "SetAtariScreenSizeHeight"
#define SCREEN_FREQ            "ScreenRefreshFreq"
#define PVDI                "PVDI"


#define  ATARI_SCRAP_FILE "/GEMSYS/GEMSCRAP/SCRAP.TXT"


unsigned Preferences::AtariLanguage = 0;
unsigned Preferences::AtariMemSize = (8 * 1024 * 1024);
bool Preferences::bShowHostMenu = true;
enAtariScreenColourMode Preferences::atariScreenColourMode = atariScreenMode16M;
bool Preferences::bHideHostMouse = false;
bool Preferences::bAutoStartMagiC = true;
unsigned Preferences::drvFlags[NDRIVES];    // 1 == RdOnly / 2 == 8+3
const char *Preferences::drvPath[NDRIVES];
char Preferences::AtariKernelPath[1024] = "/home/and/Documents/Atari-rootfs/MagicMacX.OS";
char Preferences::AtariRootfsPath[1024] = "/home/and/Documents/Atari-rootfs";
bool Preferences::AtariHostHomeRdonly = true;
char Preferences::AtariTempFilesUnixPath[1024] = "/tmp";
char Preferences::szPrintingCommand[256] = "echo printing not yet implemented";
char Preferences::szAuxPath[256];
int Preferences::Monitor = 0;        // 0 == Hauptbildschirm
unsigned Preferences::AtariScreenX = 100;
unsigned Preferences::AtariScreenY = 100;
unsigned Preferences::AtariScreenWidth = 1024;      // 320..4096
unsigned Preferences::AtariScreenHeight = 768;     // 200..2048
unsigned Preferences::AtariScreenStretchX = 2;     // horizontal stretch
unsigned Preferences::AtariScreenStretchY = 2;     // vertical stretch
unsigned Preferences::ScreenRefreshFrequency = 60;
//bool Preferences::bPPC_VDI_Patch;

// derived
char Preferences::AtariScrapFileUnixPath[1024];


static const char *get_home()
{
    const char *homedir;

    if ((homedir = getenv("HOME")) == nullptr)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }
    return homedir;
}


/**********************************************************************
*
* Initialisierung: Bestimmt den "Preferences"-Ordner und legt die Datei
* fest.
* => 0 = OK, sonst = Fehler
*
**********************************************************************/

int Preferences::Init()
{
    for (int i = 0; i < NDRIVES; i++)
    {
        drvPath[i] = nullptr;
        drvFlags[i] = 0;        // Lange Namen, vorw�rts sortiert
    }

    char path[1024] = "";
    const char *home = get_home();
    if (home != nullptr)
    {
        strcpy(path, home);
    }
    strcat(path, "/.config/magiclinux.conf");
    (void) getPreferences(path);

    // drive C: is root FS with 8+3 name scheme
    drvFlags['C'-'A'] |= 2;        // C: drive has 8+3 name scheme
    setDrvPath('C'-'A', AtariRootfsPath);

    // drive H: is user home
    if (home != nullptr)
    {
        drvFlags['H'-'A'] = AtariHostHomeRdonly ? 1 : 0;        // long names, read-only
        setDrvPath('H'-'A', home);
    }

    strcpy(AtariScrapFileUnixPath, AtariRootfsPath);
    strcat(AtariScrapFileUnixPath, ATARI_SCRAP_FILE);
    return 0;
}


/**********************************************************************
*
* Alle Einstellungen holen
* => 0 = OK, sonst = Fehler
*
**********************************************************************/

int Preferences::getPreferences(const char *cfgfile)
{
    struct stat statbuf;
    FILE *f;

    if (stat(cfgfile, &statbuf))
    {
        // Configuration file does not exist. Create it.
        f = fopen(cfgfile, "wt");
        if (f == nullptr)
        {
            return 1;
        }

        fprintf(f, "[HOST PATHS]\n");
        fprintf(f, "atari_kernel_path = \"%s\"\n", AtariKernelPath);
        fprintf(f, "atari_rootfs_path = \"%s\"\n", AtariRootfsPath);
        fprintf(f, "atari_h_rdonly = %s\n", AtariHostHomeRdonly ? "YES" : "NO");
        fprintf(f, "atari_temp_path = \"%s\"\n", AtariTempFilesUnixPath);
        fprintf(f, "[HOST DEVICES]\n");
        fprintf(f, "atari_print_cmd = \"%s\"\n", szPrintingCommand);
        fprintf(f, "atari_serial_dev_path = \"%s\"\n", szAuxPath);
        fprintf(f, "[ATARI SCREEN]\n");
        fprintf(f, "atari_screen_width = %u\n", AtariScreenWidth);
        fprintf(f, "atari_screen_height = %u\n", AtariScreenHeight);
        fprintf(f, "atari_screen_stretch_x = %u\n", AtariScreenStretchX);
        fprintf(f, "atari_screen_stretch_y = %u\n", AtariScreenStretchY);
        fprintf(f, "atari_screen_rate_hz = %u\n", ScreenRefreshFrequency);
        fprintf(f, "atari_screen_colour_mode = %u\n", atariScreenColourMode);
        fprintf(f, "# 0:24b 1:16b 2:256 3:16 4:16ip 5:4ip 6:mono\n");
        fprintf(f, "hide_host_mouse = %s\n", bHideHostMouse ? "YES" : "NO");
        fprintf(f, "[SCREEN PLACEMENT]\n");
        fprintf(f, "app_display_number = %u\n", Monitor);
        fprintf(f, "app_window_x = %u\n", AtariScreenX);
        fprintf(f, "app_window_y = %u\n", AtariScreenY);
        fprintf(f, "[ATARI EMULATION]\n");
        fprintf(f, "atari_memory_size = %u\n", AtariMemSize);
        fprintf(f, "atari_language = %u\n", AtariLanguage);
        fprintf(f, "show_host_menu = %s\n", bShowHostMenu ? "YES" : "NO");
        fprintf(f, "atari_autostart = %s\n", bAutoStartMagiC ? "YES" : "NO");
        fprintf(f, "[ADDITIONAL ATARI DRIVES]\n");
        fprintf(f, "# atari_drv_n = flags [1:read-only, 2:8+3] path\n");
        for (unsigned n = 1; n < NDRIVES; n++)
        {
            if (drvPath[n] != nullptr)
            {
                fprintf(f, "atari_drv_%c = %u \"%s\"\n", n + 'a', drvFlags[n], drvPath[n]);
            }
        }

        fclose(f);
        return 0;
    }
#if 0
    // TODO: implement
    int ret;


    ret = Open();
    if    (ret)
        return(ret);

    m_AtariMemSize = (unsigned long) GetRsrcNum(CFSTR(ATARIMEM),
                                    ATARIMEMSIZEDEFAULT >> 10) << 10;
    if    (m_AtariMemSize > MAX_ATARIMEMSIZE)
        m_AtariMemSize = MAX_ATARIMEMSIZE;

    m_Monitor = (short) GetRsrcNum(CFSTR(MONITOR), 0);

    m_bShowMacMenu = (bool) GetRsrcNum(CFSTR(SHOWMACMENU), 1);

    m_bShowMacMouse = (bool) GetRsrcNum(CFSTR(SHOWMACMOUSE), 1);

    m_bAutoStartMagiC = (bool) GetRsrcNum(CFSTR(AUTOSTARTMAGIC), 0);

    // default: F15 (Pause)
    m_KeyCodeForRightMouseButton = (unsigned short) GetRsrcNum(CFSTR(KEYCODEFORRIGHTMOUSEBUTTON), 113);

    // Laufwerke (au�er C: und M: und U:)

    register int i;
    char szData[256];
    CFStringRef cfKey;

    for    (i = 0; i < NDRIVES; i++)
    {
        // Pfad

        if    (ATARIDRIVEISMAPPABLE(i))
        {
            sprintf(szData, "Drive_%c", i + 'A');
            cfKey = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, szData, kCFStringEncodingISOLatin1, kCFAllocatorNull);
            m_drvAlias[i] = GetRsrcAlias(cfKey);
            CFRelease(cfKey);
        }
        else
            m_drvAlias[i] = nil;

        // Flags

        sprintf(szData, "Drive_%c_Flags", i + 'A');
        cfKey = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, szData, kCFStringEncodingISOLatin1, kCFAllocatorNull);
        m_drvFlags[i] = GetRsrcNum(cfKey, m_drvFlags[i], false);
        CFRelease(cfKey);
        if    (i == 'M' - 'A')
            m_drvFlags[i] &= ~2;    // immer lange Namen, nicht 8+3
    }

    // Druck-Kommando

    GetRsrcStr(CFSTR(PRINTINGCOMMAND), m_szPrintingCommand, "lpr -P HP870cxi -o raw %s");

    // BSD-Pfad f�r Modem

    GetRsrcStr(CFSTR(AUXPATH), m_szAuxPath, "/dev/cu.modem");

    // Atari-Bildschirm

    m_bAtariScreenManualSize = (bool) GetRsrcNum(CFSTR(SETSCREENSIZEMANUALLY), 0);
    // wichtig: Wenn im Fenster, dann Mac-Men�leiste immer anzeigen
    if    (m_bAtariScreenManualSize)
        m_bShowMacMenu = true;
    m_AtariScreenX = (unsigned short) GetRsrcNum(CFSTR(SETSCREENSIZE_X), 5);
    m_AtariScreenY= (unsigned short) GetRsrcNum(CFSTR(SETSCREENSIZE_Y), 25);
    m_AtariScreenWidth = (unsigned short) GetRsrcNum(CFSTR(SETSCREENSIZE_WIDTH), 640);
    m_AtariScreenHeight= (unsigned short) GetRsrcNum(CFSTR(SETSCREENSIZE_HEIGHT), 480);

    m_ScreenRefreshFrequency = (unsigned short) GetRsrcNum(CFSTR(SCREEN_FREQ), 1);

    m_bPPC_VDI_Patch = (bool) GetRsrcNum(CFSTR(PVDI), 1);

    /* no longer used
    // Dateitypen

    GetRsrcStringArray("\pDOCTYPES", &m_DocTypes, &m_DocTypesNum, (void *) DEF_DOCTYPES, (short) sizeof(DEF_DOCTYPES));
    */

    return(0);
#else
    return 0;
#endif
}


/**********************************************************************
*
* Aktualisieren
*
**********************************************************************/

void Preferences::Update_Monitor(void)
{
#if 0
    // TODO: implement
    SetRsrcNum(CFSTR(MONITOR), (long) (m_Monitor));

    Update();    // write to disk
#endif
}

void Preferences::Update_AtariMem(void)
{
#if 0
    // TODO: implement
    SetRsrcNum(CFSTR(ATARIMEM), (long) (m_AtariMemSize >> 10));

    Update();    // write to disk
#endif
}

void Preferences::Update_GeneralSettings(void)
{
#if 0
    // TODO: implement
    SetRsrcNum(CFSTR(SHOWMACMOUSE), (long) m_bShowMacMouse);
    SetRsrcNum(CFSTR(AUTOSTARTMAGIC), (long) m_bAutoStartMagiC);
    SetRsrcNum(CFSTR(KEYCODEFORRIGHTMOUSEBUTTON), (long) m_KeyCodeForRightMouseButton);

    SetRsrcNum(CFSTR(PVDI), (long) m_bPPC_VDI_Patch);

    Update();    // write to disk
#endif
}

void Preferences::Update_AtariScreen(void)
{
#if 0
    // TODO: implement
    SetRsrcNum(CFSTR(SHOWMACMENU), (long) m_bShowMacMenu);
    SetRsrcNum(CFSTR(SETSCREENSIZEMANUALLY), (long) m_bAtariScreenManualSize);
    SetRsrcNum(CFSTR(SETSCREENSIZE_X), (long) m_AtariScreenX);
    SetRsrcNum(CFSTR(SETSCREENSIZE_Y), (long) m_AtariScreenY);
    SetRsrcNum(CFSTR(SETSCREENSIZE_WIDTH), (long) m_AtariScreenWidth);
    SetRsrcNum(CFSTR(SETSCREENSIZE_HEIGHT), (long) m_AtariScreenHeight);
    SetRsrcNum(CFSTR(SCREEN_FREQ), (long) m_ScreenRefreshFrequency);

    Update();    // write to disk
#endif
}

void Preferences::Update_Drives(void)
{
#if 0
    // TODO: implement
    register int i;
    char szData[256];
    CFStringRef cfKey;

    for    (i = 0; i < NDRIVES; i++)
    {
        // Pfad

        if    (ATARIDRIVEISMAPPABLE(i))
        {
            // Alle Laufwerke au�er C:, M: und U: k�nnen einen �nderbaren Mac-Pfad haben.

            sprintf(szData, "Drive_%c", i + 'A');
            cfKey = CFStringCreateWithCString/*NoCopy*/(kCFAllocatorDefault, szData, kCFStringEncodingISOLatin1/*, kCFAllocatorNull*/);
            SetRsrcAlias(cfKey, m_drvAlias[i]);
            Update();    // write to disk
            CFRelease(cfKey);
        }

        // Flags

        if    (ATARIDRIVEISMAPPABLE(i) || (i == 'C' - 'A'))
        {
            // zus�tzlich kann C: ge�nderte Flags haben, M: und U: jedoch nicht.

            sprintf(szData, "Drive_%c_Flags", i + 'A');
            cfKey = CFStringCreateWithCString/*NoCopy*/(kCFAllocatorDefault, szData, kCFStringEncodingISOLatin1/*, kCFAllocatorNull*/);
            if    ((m_drvAlias[i]) || (i == 'M'-'A'))
            {
                //(void) GetRsrcNum(cfKey, m_drvFlags[i], true);    // ggf. hinzuf�gen
                SetRsrcNum(cfKey, m_drvFlags[i]);
                Update();    // save to disk (necessary!!!)
            }
            else
            {
                // Flags l�schen, wenn existieren
                SetRsrcStr(cfKey, NULL);
                Update();
            }
            CFRelease(cfKey);
        }
    }
#endif

//    Update();    // too late? Flags will be ignored?!?
}

void Preferences::Update_PrintingCommand(void)
{
#if 0
    // TODO: implement
    SetRsrcStr(CFSTR(PRINTINGCOMMAND), m_szPrintingCommand);

    Update();    // write to disk
#endif
}


void Preferences::Update_AuxPath(void)
{
#if 0
    // TODO: implement
    SetRsrcStr(CFSTR(AUXPATH), m_szAuxPath);

    Update();    // write to disk
#endif
}
