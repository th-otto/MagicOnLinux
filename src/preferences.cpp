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
unsigned Preferences::drvFlags[NDRIVES];    // 1 == RdOnly / 2 == 8+3 / 4 == case insensitive, ...
const char *Preferences::drvPath[NDRIVES];
char Preferences::AtariKernelPath[1024] = "~/Documents/Atari-rootfs/MAGICLIN.OS";
char Preferences::AtariRootfsPath[1024] = "~/Documents/Atari-rootfs";
bool Preferences::AtariHostHome = true;                      // Atari H: as host home
bool Preferences::AtariHostHomeRdOnly = true;
bool Preferences::AtariHostRoot = true;                      // Atari M: as host root
bool Preferences::AtariHostRootRdOnly = true;                // Atari M: is write protected
char Preferences::AtariTempFilesUnixPath[1024] = "/tmp";
char Preferences::szPrintingCommand[256] = "echo printing not yet implemented";
char Preferences::szAuxPath[256];
unsigned Preferences::Monitor = 0;
int Preferences::AtariScreenX = -1;    // -1 = auto
int Preferences::AtariScreenY = -1;
unsigned Preferences::AtariScreenWidth = 1024;
unsigned Preferences::AtariScreenHeight = 768;
unsigned Preferences::AtariScreenStretchX = 2;
unsigned Preferences::AtariScreenStretchY = 2;
unsigned Preferences::ScreenRefreshFrequency = 60;
//bool Preferences::bPPC_VDI_Patch;

// derived
char Preferences::AtariScrapFileUnixPath[1024];


/** **********************************************************************************************
 *
 * @brief Get home directory for Atari drive H:
 *
 * @return pointer to home directory path
 *
 ************************************************************************************************/
static const char *get_home()
{
    const char *homedir;

    if ((homedir = getenv("HOME")) == nullptr)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }
    return homedir;
}


/** **********************************************************************************************
 *
 * @brief Write, load and evaluate the configuration file
 *
 * @param[in] config_file       configuration text file, may start with '~'
 * @param[in] rewrite_conf      overwrite existing configuration file with default values
 *
 * @return zero or number of errors
 *
 * @note The configuration file is created with default values if it does not
 *       exist or if requested.
 *
 ************************************************************************************************/
int Preferences::init(const char *config_file, bool rewrite_conf)
{
    for (int i = 0; i < NDRIVES; i++)
    {
        drvPath[i] = nullptr;
        drvFlags[i] = 0;        // long names, read-write, case sensitive
    }

    char path[1024] = "";
    const char *home = get_home();
    if (config_file[0] == '~')
    {
        if (home != nullptr)
        {
            strcpy(path, home);
        }
        strcat(path, config_file + 1);
        config_file = path;
    }
    int num_errors = getPreferences(config_file, rewrite_conf);

    // drive C: is root FS with 8+3 name scheme
    drvFlags['C'-'A'] = DRV_FLAG_8p3 + DRV_FLAG_CASE_INSENS;        // C: drive has 8+3 name scheme
    setDrvPath('C'-'A', AtariRootfsPath);

    // drive H: is user home
    if (AtariHostHome && (home != nullptr))
    {
        drvFlags['H'-'A'] |= AtariHostHomeRdOnly ? DRV_FLAG_RDONLY : 0;        // long names, read-only
        setDrvPath('H'-'A', home);
    }

    // drive M: is host root, if requested
    if (AtariHostRoot)
    {
        drvFlags['M'-'A'] |= AtariHostRootRdOnly ? DRV_FLAG_RDONLY : 0;        // long names, read-only
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
    fprintf(f, "%s = %s\n",     var_name[VAR_ATARI_H_HOME], AtariHostHome ? "YES" : "NO");
    fprintf(f, "%s = %s\n",     var_name[VAR_ATARI_H_RDONLY], AtariHostHomeRdOnly ? "YES" : "NO");
    fprintf(f, "%s = %s\n",     var_name[VAR_ATARI_M_HOST_ROOT], AtariHostRoot ? "YES" : "NO");
    fprintf(f, "%s = %s\n",     var_name[VAR_ATARI_M_HOST_ROOT_RDONLY], AtariHostRootRdOnly ? "YES" : "NO");
    fprintf(f, "%s = \"%s\"\n", var_name[VAR_ATARI_TEMP_PATH], AtariTempFilesUnixPath);
    fprintf(f, "[HOST DEVICES]\n");
    fprintf(f, "%s = \"%s\"\n", var_name[VAR_ATARI_PRINT_CMD], szPrintingCommand);
    fprintf(f, "%s = \"%s\"\n", var_name[VAR_ATARI_SERIAL_DEV_PATH], szAuxPath);
    fprintf(f, "[ATARI SCREEN]\n");
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_SCREEN_WIDTH], AtariScreenWidth);
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_SCREEN_HEIGHT], AtariScreenHeight);
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_SCREEN_STRETCH_X], AtariScreenStretchX);
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_SCREEN_STRETCH_Y], AtariScreenStretchY);
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_SCREEN_RATE_HZ], ScreenRefreshFrequency);
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_SCREEN_COLOUR_MODE], atariScreenColourMode);
    fprintf(f, "# 0:24b 1:16b 2:256 3:16 4:16ip 5:4ip 6:mono\n");
    fprintf(f, "%s = %s\n",     var_name[VAR_HIDE_HOST_MOUSE], bHideHostMouse ? "YES" : "NO");
    fprintf(f, "[SCREEN PLACEMENT]\n");
    fprintf(f, "%s = %u\n",     var_name[VAR_APP_DISPLAY_NUMBER], Monitor);
    fprintf(f, "%s = %u\n",     var_name[VAR_APP_WINDOW_X], AtariScreenX);
    fprintf(f, "%s = %u\n",     var_name[VAR_APP_WINDOW_Y], AtariScreenY);
    fprintf(f, "[ATARI EMULATION]\n");
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_MEMORY_SIZE], AtariMemSize);
    fprintf(f, "%s = %u\n",     var_name[VAR_ATARI_LANGUAGE], AtariLanguage);
    fprintf(f, "%s = %s\n",     var_name[VAR_SHOW_HOST_MENU], bShowHostMenu ? "YES" : "NO");
    fprintf(f, "%s = %s\n",     var_name[VAR_ATARI_AUTOSTART], bAutoStartMagiC ? "YES" : "NO");
    fprintf(f, "[ADDITIONAL ATARI DRIVES]\n");
    fprintf(f, "# %s<A..Z> = flags [1:read-only, 2:8+3, 4:case-insensitive] path or image\n", var_name[VAR_ATARI_DRV_]);
    for (unsigned n = 0; n < NDRIVES; n++)
    {
        if (drvPath[n] != nullptr)
        {
            fprintf(f, "%s%c = %u \"%s\"\n", var_name[VAR_ATARI_DRV_], n + 'a', drvFlags[n], drvPath[n]);
        }
    }

    fclose(f);
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Read string, optionally enclosed in "" or ''
 *
 * @param[out] outbuf   output buffer, holds raw string
 * @param[in]  bufsiz   size of output buffer including end-of-string
 * @param[out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_quotated_str(char *outbuf, unsigned bufsiz, const char **in)
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
    if (len < bufsiz - 1)
    {
        strncpy(outbuf, in_start, len);
        outbuf[len] = '\0';
    }
    else
    {
        return 1;       // overflow
    }
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Read unsigned numerical value, decimal, sedecimal or octal
 *
 * @param[out] outval   number
 * @param[in]  maxval   maximum valid number
 * @param[out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_unsigned(unsigned *outval, unsigned maxval, const char **in)
{
    char *endptr;
    unsigned long value = strtoul(*in, &endptr, 0 /*auto base*/);
    if (endptr > *in)
    {
        if (value <= maxval)
        {
            *outval = (unsigned) value;
            *in = endptr;
            return 0;
        }
    }
    return 1;
}


/** **********************************************************************************************
 *
 * @brief Read signed numerical value, decimal, sedecimal or octal
 *
 * @param[out] outval   number
 * @param[in]  maxval   maximum valid number
 * @param[out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_signed(int *outval, const char **in)
{
    char *endptr;
    long value = strtol(*in, &endptr, 0 /*auto base*/);
    if (endptr > *in)
    {
        *outval = (int) value;
        *in = endptr;
        return 0;
    }
    return 1;
}


/** **********************************************************************************************
 *
 * @brief Evaluate boolan value "yes" or "no"
 *
 * @param[out] outval   boolean value
 * @param[in]  YesOrNo  boolean string, case-insensitive
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_bool(bool *outval, const char *YesOrNo)
{
    if (!strcasecmp(YesOrNo, "yes"))
    {
        *outval = true;
        return 0;
    }
    else
    if (!strcasecmp(YesOrNo, "no"))
    {
        *outval = false;
        return 0;
    }

    return 1;
}


/** **********************************************************************************************
 *
 * @brief Evaluate boolan string "yes" or "no", optionally enclosed in "" or ''
 *
 * @param[out] outval   boolean value
 * @param[out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_quotated_str_bool(bool *outval, const char **line)
{
    char YesOrNo[16];
    int num_errors;

    num_errors = eval_quotated_str(YesOrNo, sizeof(YesOrNo), line);
    if (num_errors == 0)
    {
        num_errors += eval_bool(outval, YesOrNo);
    }

    return num_errors;
}


/** **********************************************************************************************
 *
 * @brief Replace leading '~/' in path with home directory
 *
 * @param[in,out] pathbuf  path to be extended
 * @param[in]     bufsiz   size of output buffer including end-of-string
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_home(char *pathbuf, unsigned bufsiz)
{
    if ((pathbuf[0] == '~') && (pathbuf[1] == '/'))
    {
        const char *home = get_home();
        if (home != nullptr)
        {
            unsigned lenh = strlen(home);
            unsigned lenp = strlen(pathbuf) + 1;   // including end-of-string

            // subtract 1 for '~' which we will remove
            if (lenp + lenh - 1 > bufsiz)
            {
                fprintf(stderr, "Overflow in path: %s", pathbuf);
                return 1;
            }
            memmove(pathbuf + lenh - 1, pathbuf, lenp);
            memcpy(pathbuf, home, lenh);
        }
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Read string, optionally enclosed in "" or '', and evaluates leading '~' for user home directory
 *
 * @param[out] outbuf   output buffer, holds raw string
 * @param[in]  bufsiz   size of output buffer including end-of-string
 * @param[out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
static int eval_quotated_str_path(char *outbuf, unsigned bufsiz, const char **in)
{
    int num_errors = eval_quotated_str(outbuf, bufsiz, in);
    if (num_errors == 0)
    {
        num_errors += eval_home(outbuf, bufsiz);
    }
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
        fprintf(stderr, "unknown key\n");
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
            num_errors += eval_quotated_str_path(AtariKernelPath, sizeof(AtariKernelPath), &line);
            break;

        case VAR_ATARI_ROOTFS_PATH:
            num_errors += eval_quotated_str_path(AtariRootfsPath, sizeof(AtariRootfsPath), &line);
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
            num_errors += eval_quotated_str_path(AtariTempFilesUnixPath, sizeof(AtariTempFilesUnixPath), &line);
            break;

        case VAR_ATARI_PRINT_CMD:
            num_errors += eval_quotated_str(szPrintingCommand, sizeof(szPrintingCommand), &line);
            break;

        case VAR_ATARI_SERIAL_DEV_PATH:
            num_errors += eval_quotated_str_path(szAuxPath, sizeof(szAuxPath), &line);
            break;

        case VAR_ATARI_SCREEN_WIDTH:
            num_errors += eval_unsigned(&AtariScreenWidth, 4096, &line);
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
            num_errors += eval_signed(&AtariScreenX, &line);
            break;

        case VAR_APP_WINDOW_Y:
            num_errors += eval_signed(&AtariScreenY, &line);
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
                num_errors += eval_quotated_str_path(pathbuf, sizeof(pathbuf), &line);
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


/** **********************************************************************************************
 *
 * @brief Read all preferences from a configuration file or write default file, if requested
 *
 * @param[in]  line   input line, with trailing \n and zero byte
 *
 * @return number of errors
 *
 ************************************************************************************************/
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
}
