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
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include "preferences.h"


#define MAX_ATARIMEMSIZE	(2U*1024U*1024U*1024U)		// 2 Gigabytes

#define  ATARI_SCRAP_FILE "/GEMSYS/GEMSCRAP/SCRAP.TXT"

// variable numbers in preferences
#define VAR_ATARI_KERNEL_PATH           0
#define VAR_ATARI_ROOTFS_PATH           1
#define VAR_ATARI_H_HOME                2
#define VAR_ATARI_H_RDONLY              3
#define VAR_ATARI_M_HOST_ROOT           4
#define VAR_ATARI_M_HOST_ROOT_RDONLY    5
#define VAR_ATARI_TEMP_PATH             6
#define VAR_ATARI_PRINT_CMD             7
#define VAR_ATARI_SERIAL_DEV_PATH       8
#define VAR_ATARI_SCREEN_WIDTH          9
#define VAR_ATARI_SCREEN_HEIGHT         10
#define VAR_ATARI_SCREEN_STRETCH_X      11
#define VAR_ATARI_SCREEN_STRETCH_Y      12
#define VAR_ATARI_SCREEN_RATE_HZ        13
#define VAR_ATARI_SCREEN_COLOUR_MODE    14
#define VAR_HIDE_HOST_MOUSE             15
#define VAR_APP_DISPLAY_NUMBER          16
#define VAR_APP_WINDOW_X                17
#define VAR_APP_WINDOW_Y                18
#define VAR_ATARI_MEMORY_SIZE           19
#define VAR_ATARI_LANGUAGE              20
#define VAR_SHOW_HOST_MENU              21
#define VAR_ATARI_AUTOSTART             22
#define VAR_ATARI_DRV_                  23
#define VAR_NUMBER                      24

// variable names in preferences
static const char *var_name[VAR_NUMBER] =
{
    //[HOST PATHS]
    "atari_kernel_path",
    "atari_rootfs_path",
    "atari_h_home",
    "atari_h_rdonly",
    "atari_m_host_root",
    "atari_m_host_root_rdonly",
    "atari_temp_path",
    //[HOST DEVICES]
    "atari_print_cmd",
    "atari_serial_dev_path",
    //[ATARI SCREEN]
    "atari_screen_width",
    "atari_screen_height",
    "atari_screen_stretch_x",
    "atari_screen_stretch_y",
    "atari_screen_rate_hz",
    "atari_screen_colour_mode",
    "hide_host_mouse",
    //[SCREEN PLACEMENT]
    "app_display_number",
    "app_window_x",
    "app_window_y",
    //[ATARI EMULATION]
    "atari_memory_size",
    "atari_language",
    "show_host_menu",
    "atari_autostart",
    //[ADDITIONAL ATARI DRIVES]
    "atari_drv_"
};


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
bool Preferences::AtariHostHome = true;                      // Atari H: as host home
bool Preferences::AtariHostHomeRdOnly = true;
bool Preferences::AtariHostRoot = true;                      // Atari M: as host root
bool Preferences::AtariHostRootRdOnly = true;                // Atari M: is write protected
char Preferences::AtariTempFilesUnixPath[1024] = "/tmp";
char Preferences::szPrintingCommand[256] = "echo printing not yet implemented";
char Preferences::szAuxPath[256];
unsigned Preferences::Monitor = 0;        // 0 == Hauptbildschirm
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

int Preferences::Init(bool rewrite_conf)
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
    int num_errors = getPreferences(path, rewrite_conf);

    // drive C: is root FS with 8+3 name scheme
    drvFlags['C'-'A'] |= 2;        // C: drive has 8+3 name scheme
    setDrvPath('C'-'A', AtariRootfsPath);

    // drive H: is user home
    if (AtariHostHome && (home != nullptr))
    {
        drvFlags['H'-'A'] = AtariHostHomeRdOnly ? 1 : 0;        // long names, read-only
        setDrvPath('H'-'A', home);
    }

    // drive M: is host root, if requested
    if (AtariHostRoot)
    {
        drvFlags['M'-'A'] = AtariHostRootRdOnly ? 1 : 0;        // long names, read-only
        setDrvPath('M'-'A', "/");
    }

    strcpy(AtariScrapFileUnixPath, AtariRootfsPath);
    strcat(AtariScrapFileUnixPath, ATARI_SCRAP_FILE);
    return num_errors;
}


/** **********************************************************************************************
 *
 * @brief Write configuration file
 *
 * @param[in]  cfgfile   path
 *
 * @return 0 or error code
 *
 ************************************************************************************************/
int Preferences::writePreferences(const char *cfgfile)
{
    FILE *f = fopen(cfgfile, "wt");
    if (f == nullptr)
    {
        return errno;
    }

    fprintf(f, "[HOST PATHS]\n");
    fprintf(f, "%s = \"%s\"\n", var_name[VAR_ATARI_KERNEL_PATH], AtariKernelPath);
    fprintf(f, "%s = \"%s\"\n", var_name[VAR_ATARI_ROOTFS_PATH], AtariRootfsPath);
    fprintf(f, "atari_h_home = %s\n", AtariHostHome ? "YES" : "NO");
    fprintf(f, "atari_h_rdonly = %s\n", AtariHostHomeRdOnly ? "YES" : "NO");
    fprintf(f, "atari_m_host_root = %s\n", AtariHostRoot ? "YES" : "NO");
    fprintf(f, "atari_m_host_root_rdonly = %s\n", AtariHostRootRdOnly ? "YES" : "NO");
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


// read string, optionally enclosed in "" or ''
static int eval_quotated_str(char *out, unsigned maxlen, const char **in)
{
    const char *in_start = *in;
    const char *in_end;
    char delimiter = **in;
    if ((delimiter == '\"') || (delimiter == '\''))
    {
        (*in)++;
        in_start++;
    }
    else
    {
        delimiter = 0;
    }
    while((**in) && (**in != '\n') && (**in != delimiter))
    {
        (*in)++;
    }

    if (delimiter != 0)
    {
        if (**in == delimiter)
        {
            in_end = *in;
            (*in)++;
        }
        else
        {
            return 1;   // missing delimiter
        }
    }
    else
    {
        in_end = *in;
    }

    unsigned len = in_end - in_start;
    if (len < maxlen)
    {
        strncpy(out, in_start, len);
        out[len] = '\0';
    }
    else
    {
        return 1;       // overflow
    }
    return 0;
}


static int eval_unsigned(unsigned *out, unsigned maxval, const char **in)
{
    char *endptr;
    unsigned long long value = strtoul(*in, &endptr, 0 /*auto base*/);
    if (endptr > *in)
    {
        if (value <= maxval)
        {
            *out = (unsigned) value;
            *in = endptr;
            return 0;
        }
    }
    return 1;
}


static int eval_bool(bool *out, const char *YesOrNo)
{
    if (!strcasecmp(YesOrNo, "yes"))
    {
        *out = true;
        return 0;
    }
    else
    if (!strcasecmp(YesOrNo, "no"))
    {
        *out = false;
        return 0;
    }

    return 1;
}


// read string, optionally enclosed in "" or ''
static int eval_quotated_str_bool(bool *out, const char **line)
{
    char YesOrNo[16];
    int num_errors;

    num_errors = eval_quotated_str(YesOrNo, sizeof(YesOrNo), line);
    if (num_errors == 0)
        num_errors += eval_bool(out, YesOrNo);

    return num_errors;
}



/** **********************************************************************************************
 *
 * @brief Evaluate a single preferences line
 *
 * @param[in]  line   input line, with trailing \n and zero byte
 *
 * @return 0 for OK, 1 for error
 *
 * @note empty lines, sections and comments have already been processed
 *
 ************************************************************************************************/
int Preferences::evaluatePreferencesLine(const char *line)
{
    unsigned vu;
    unsigned var;
    const char *key;
    int num_errors = 0;
    unsigned drv;

    for (var = 0; var < VAR_NUMBER; var++)
    {
        key = var_name[var];
        unsigned keylen = strlen(key);
        if (!strncasecmp(line, key, keylen))
        {
            if (var == VAR_ATARI_DRV_)
            {
                char c = toupper(line[keylen]);
                if ((c < 'A') || (c > 'Z'))
                {
                    return 1;
                }
                line += keylen + 1;
                drv = c - 'A';
                break;
            }
            else
            {
                char c = line[keylen];
                if (isspace(c) || (c == '='))
                {
                    line += keylen;
                    break;
                }
            }
        }
    }

    if (var >= VAR_NUMBER)
    {
        printf("unknown key");
        return 1;
    }

    // skip spaces
    while(isspace(*line))
    {
        line++;
    }
    if (*line != '=')
    {
        return 1;
    }
    line++;
    // skip spaces
    while(isspace(*line))
    {
        line++;
    }

    switch(var)
    {
        case VAR_ATARI_KERNEL_PATH:
            num_errors += eval_quotated_str(AtariKernelPath, sizeof(AtariKernelPath), &line);
            break;

        case VAR_ATARI_ROOTFS_PATH:
            num_errors += eval_quotated_str(AtariRootfsPath, sizeof(AtariRootfsPath), &line);
            break;

        case VAR_ATARI_H_HOME:
            num_errors += eval_quotated_str_bool(&AtariHostHome, &line);
            break;

        case VAR_ATARI_H_RDONLY:
            num_errors += eval_quotated_str_bool(&AtariHostHomeRdOnly, &line);
            break;

        case VAR_ATARI_M_HOST_ROOT:
            num_errors += eval_quotated_str_bool(&AtariHostRoot, &line);
            break;

        case VAR_ATARI_M_HOST_ROOT_RDONLY:
            num_errors += eval_quotated_str_bool(&AtariHostRootRdOnly, &line);
            break;

        case VAR_ATARI_TEMP_PATH:
            num_errors += eval_quotated_str(AtariTempFilesUnixPath, sizeof(AtariTempFilesUnixPath), &line);
            break;

        case VAR_ATARI_PRINT_CMD:
            num_errors += eval_quotated_str(szPrintingCommand, sizeof(szPrintingCommand), &line);
            break;

        case VAR_ATARI_SERIAL_DEV_PATH:
            num_errors += eval_quotated_str(szAuxPath, sizeof(szAuxPath), &line);
            break;

        case VAR_ATARI_SCREEN_WIDTH:
            num_errors += eval_unsigned(&AtariScreenWidth, 4096, &line);
            printf("AtariScreenWidth = %u\n", AtariScreenWidth);
            break;

        case VAR_ATARI_SCREEN_HEIGHT:
            num_errors += eval_unsigned(&AtariScreenHeight, 2048, &line);
            break;

        case VAR_ATARI_SCREEN_STRETCH_X:
            num_errors += eval_unsigned(&AtariScreenStretchX, 8, &line);
            break;

        case VAR_ATARI_SCREEN_STRETCH_Y:
            num_errors += eval_unsigned(&AtariScreenStretchY, 8, &line);
            break;

        case VAR_ATARI_SCREEN_RATE_HZ:
            num_errors += eval_unsigned(&ScreenRefreshFrequency, 120, &line);
            break;

        case VAR_ATARI_SCREEN_COLOUR_MODE:
            num_errors += eval_unsigned(&vu, 6, &line);
            atariScreenColourMode = (enAtariScreenColourMode) vu;
            break;

        case VAR_HIDE_HOST_MOUSE:
            num_errors += eval_quotated_str_bool(&bHideHostMouse, &line);
            break;

        case VAR_APP_DISPLAY_NUMBER:
            num_errors += eval_unsigned(&Monitor, 0xffffffff, &line);
            break;

        case VAR_APP_WINDOW_X:
            num_errors += eval_unsigned(&AtariScreenX, 4096, &line);
            break;

        case VAR_APP_WINDOW_Y:
            num_errors += eval_unsigned(&AtariScreenY, 4096, &line);
            break;

        case VAR_ATARI_MEMORY_SIZE:
            num_errors += eval_unsigned(&AtariMemSize, 0x20000000, &line);
            break;

        case VAR_ATARI_LANGUAGE:
            num_errors += eval_unsigned(&AtariLanguage, 0xffffffff, &line);
            break;

        case VAR_SHOW_HOST_MENU:
            num_errors += eval_quotated_str_bool(&bShowHostMenu, &line);
            break;

        case VAR_ATARI_AUTOSTART:
            num_errors += eval_quotated_str_bool(&bAutoStartMagiC, &line);
            break;

        case VAR_ATARI_DRV_:
            {
                unsigned flags;
                num_errors += eval_unsigned(&flags, 0xffffffff, &line);
                if (num_errors > 0)
                {
                    break;
                }
                while(isspace(*line))
                {
                    line++;
                }
                char pathbuf[1024];
                num_errors += eval_quotated_str(pathbuf, sizeof(pathbuf), &line);
                if (num_errors == 0)
                {
                    setDrvPath(drv, pathbuf);
                    drvFlags[drv] = flags;
                }
            }
            break;

        default:
            return 1;
            break;
    } // switch

    if (num_errors == 0)
    {
        // skip trailing blanks
        while(isspace(*line))
        {
            line++;
        }
        if ((*line != '\0') && (*line != '\n'))
        {
            num_errors++;   // rubbish at end of line
        }
    }

    return num_errors;
}


/**********************************************************************
*
* Alle Einstellungen holen
* => 0 = OK, sonst = Fehler
*
**********************************************************************/

int Preferences::getPreferences(const char *cfgfile, bool rewrite_conf)
{
    struct stat statbuf;
    FILE *f;

    if (rewrite_conf || stat(cfgfile, &statbuf))
    {
        // Configuration file does not exist. Create it.
        return writePreferences(cfgfile);
    }

    f = fopen(cfgfile, "rt");
    if (f == 0)
    {
        return errno;
    }

    char line[2048];
    int num_err = 0;
    while(fgets(line, 2046, f))
    {
        const char *c = line;

        // skip spaces
        while(isspace(*c))
        {
            c++;
        }

        // skip comments
        if (*c == '#')
        {
            continue;
        }

        // skip section names
        if (*c == '[')
        {
            continue;
        }

        // skip empty lines
        if (*c == '\0')
        {
            continue;
        }

        int nline_err = evaluatePreferencesLine(c);
        if (nline_err)
        {
            fprintf(stderr, "Syntax error in configuration file: %s", line);
        }
        num_err += nline_err;
    }
    fclose(f);
    return num_err;

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
