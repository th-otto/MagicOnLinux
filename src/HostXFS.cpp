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
**
** This is the host part of Host XFS for MagiCLinux
**
** (C) Andreas Kromke 1994-2025
**
**
** Data transfer structures:
**   ataridos       contains the host part of the file system (XFS)
**   macdev         contains the host part of the file driver (MX_FD)
**
*/

#include "config.h"

#include <endian.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <utime.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "Debug.h"
#include "Globals.h"
#include "HostXFS.h"
#include "Atari.h"
#include "emulation_globals.h"
#include "conversion.h"
#include "gui.h"

#if !defined(_DEBUG_XFS)
 #undef DebugInfo
 #define DebugInfo(...)
 #undef DebugInfo2
 #define DebugInfo2(...)
#endif

#if defined(_DEBUG)
//#define DEBUG_VERBOSE
#endif

#define EOS '\0'

#if DEBUG_68K_EMU
// mechanism to get load address of a program
static int trigger = 0;
#define ATARI_PRG_TO_TRACE "PD.PRG"
short trigger_refnum;
//int m68k_trace_trigger = 0;
char *trigger_ProcessStartAddr = 0;
unsigned trigger_ProcessFileLen = 0;
extern void _DumpAtariMem(const char *filename);
#endif


/** **********************************************************************************************
 *
 * @brief Constructor
 *
 ************************************************************************************************/
CHostXFS::CHostXFS()
{
    xfs_drvbits = 0;
    for (int i = 0; i < NDRIVES; i++)
    {
        drv_host_path[i] = nullptr;    // invalid
    }
    HostHandles::init();
}


/** **********************************************************************************************
 *
 * @brief Destructor
 *
 ************************************************************************************************/
CHostXFS::~CHostXFS()
{
}


#ifdef DEBUG_VERBOSE
/** **********************************************************************************************
 *
 * @brief Debug helper
 *
 ************************************************************************************************/
static void __dump(const unsigned char *p, int len)
{
//    char buf[256];

    while (len >= 4)
    {
        DebugInfo(" mem = %02x %02x %02x %02x", p[0], p[1], p[2], p[3]);
        p += 4;
        len -= 4;
    }

    while (len)
    {
        DebugInfo(" mem = %02x", p[0]);
        p += 1;
        len -= 1;
    }
}
#endif


/** **********************************************************************************************
 *
 * @brief Converts host filename to 8+3 for GEMDOS
 *
 * @param[in]  fname            input filename (host or Atari)
 * @param[out] dosname          8+3 GEMDOS filename, zero terminated
 * @param[in]  upperCase        true: convert to 8+3 uppercase, false: convert to 8+3
 * @param[in]  to_atari_charset true: convert host charset (UTF-8) to Atari charset
 *
 * @return true: filename was shortened
 *
 * @note Characters ' ' and '\' are skipped
 *
 * @note If to_atari_charset is true, the fname is in utf-8, otherwise in Atari format.
 *
 ************************************************************************************************/
bool CHostXFS::nameto_8_3
(
    const char *fname,
    unsigned char *dosname,
    bool upperCase,
    bool to_atari_charset
)
{
    // special case "." and ".."

    if (fname[0] == '.')
    {
        if (fname[1] == '\0')
        {
            *dosname++ = '.';
            *dosname = '\0';
            return false;
        }

        if ((fname[1] == '.') && (fname[2] == '\0'))
        {
            *dosname++ = '.';
            *dosname++ = '.';
            *dosname = '\0';
            return false;
        }
    }

    bool truncated = false;

    // convert up to 8 characters for filename
    int i = 0;
    while ((i < 8) && (*fname) && (*fname != '.'))
    {
        if ((*fname == ' ') || (*fname == '\\'))
        {
            fname++;
            continue;
        }

        unsigned char c;
        if (to_atari_charset)
        {
            // utf-8 -> Atari
            unsigned len = CConversion::charHost2Atari(fname, &c);
            fname += len;
        }
        else
        {
            // host_fname is alread in Atari character set
            c = *fname++;
        }

        if (upperCase)
        {
            c = CConversion::charAtari2UpperCase(c);
        }
        *dosname++ = c;
        i++;
    }

    while ((*fname) && (*fname != '.'))
    {
        fname++;        // skip everything before '.'
        truncated = true;
    }
    if (*fname == '.')
    {
        fname++;        // skip '.'
    }
    *dosname++ = '.';        // insert '.' into DOS filename

    // convert up to 3 characters for filename extension
    i = 0;
    while ((i < 3) && (*fname) && (*fname != '.'))
    {
        if ((*fname == ' ') || (*fname == '\\'))
        {
            fname++;
            continue;
        }

        unsigned char c;
        if (to_atari_charset)
        {
            // utf-8 -> Atari
            unsigned len = CConversion::charHost2Atari(fname, &c);
            fname += len;
        }
        else
        {
            // host_fname is alread in Atari character set
            c = *fname++;
        }

        if (upperCase)
        {
            c = CConversion::charAtari2UpperCase(c);
        }
        *dosname++ = c;
        i++;
    }

    if (dosname[-1] == '.')        // trailing '.'
        dosname[-1] = EOS;        //   remove
    else
        *dosname = EOS;

    if (*fname)
        truncated = true;

    return truncated;
}


/** **********************************************************************************************
 *
 * @brief [static] Get host path from directory fd
 *
 * @param[in] dir_fd    host directory file descriptor
 * @param[in] pathbuf   buffer for host path
 * @param[in] bufsiz    buffer size, including end-of-string
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::hostFd2Path(int dir_fd, char *pathbuf, uint16_t bufsiz)
{
    char pathname[32];
    sprintf(pathname, "/proc/self/fd/%u", dir_fd);
    ssize_t size = readlink(pathname, pathbuf, bufsiz - 1);     // leave one byte for end-of-string
    if (size < 0)
    {
        DebugWarning2("() : readlink() -> %s", strerror(errno));
        return EINTRN;
    }
    pathbuf[size] = '\0';   // necessary
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert Atari filename to host filename, including path separator
 *
 * @param[in]   src         Atari filename
 * @param[in]   upperCase   convert to uppercase, for 8+3 drives
 * @param[out]  dst         buffer for host filename
 * @param[in]   bufsiz      buffer size, including end-of-string.
 *
 * @return -1 on overflow, otherwise zero
 *
 ************************************************************************************************/
int CHostXFS::atariFnameToHostFname(const unsigned char *src, bool upperCase, char *dst, unsigned bufsiz)
{
    char *buf_start = dst;
    // leave space for end-of-string and four-byte utf-8 character
    while (*src && (dst < buf_start + bufsiz - 5))
    {
        if (*src == '\\')
        {
            src++;
            *dst++ = '/';
        }
        else
        {
            // Atari -> utf-8
            char c = *src++;
            if (upperCase)
            {
                c = CConversion::charAtari2UpperCase(c);
            }
            unsigned len = CConversion::charAtari2Host(c, dst);
            dst += len;
        }
    }
    *dst = EOS;
    return (dst >= buf_start + bufsiz - 1) ? -1 : 0;
}


/** **********************************************************************************************
 *
 * @brief Convert Atari filename to 8+3 and uppercase, if appropriate, and to host filename (utf-8)
 *
 * @param[in]   src     Atari filename
 * @param[out]  dst     buffer for host filename
 * @param[in]   bufsiz  buffer size, including end-of-string.
 *
 * @return -1 on overflow, otherwise zero
 *
 ************************************************************************************************/
int CHostXFS::atariFnameToHostFnameCond8p3
(
    uint16_t drv,
    const unsigned char *atari_fname,
    char *host_fname,
    unsigned bufsiz
)
{
    unsigned char dosname[16];
    if (!drv_longNames[drv])
    {
        nameto_8_3((const char *) atari_fname, dosname, drv_caseInsens[drv], false);
        atari_fname = dosname;
    }

    return atariFnameToHostFname(atari_fname, false, host_fname, bufsiz);
}


/// Conversion from Atari filename to 8+3 Atari filename.
/// If drive has 8+3 format, then convert name to dosname
/// and additionally to upper case, if file system is case insenstive.
/// Finally convert to host filename in utf-8 format.
#define CONV8p3(DRV, NAME, HOSTNAME) \
    char HOSTNAME[256]; \
    if (atariFnameToHostFnameCond8p3(DRV, NAME, HOSTNAME, sizeof(HOSTNAME))) \
    { \
        DebugError2("() -- cannot convert Atari filename to host format: %s ", NAME); \
        return ATARIERR_ERANGE; \
    }


/** **********************************************************************************************
 *
 * @brief [static] Get drive number from Atari drive name
 *
 * @param[in]  c    first character of an Atari path
 *
 * @return 0..NDRVS-1 if valid (upper or lower case), otherwise -1.
 *
 ************************************************************************************************/
int CHostXFS::getDrvNo(char c)
{
    char drv = CConversion::charAtari2UpperCase(c);
    if ((drv >= 'A') && (drv <= 'Z'))
        return drv - 'A';
    else
        return -1;
}


/** **********************************************************************************************
 *
 * @brief Convert Atari path to host path
 *
 * @param[in]   src          Atari path
 * @param[in]   default_drv  Atari drive, if src does not start with "A:" or similar
 * @param[out]  dst          buffer for host path
 * @param[in]   bufsiz       buffer size, including end-of-string.
 *
 * @return -1 on overflow, otherwise zero
 *
 ************************************************************************************************/
int CHostXFS::atariPath2HostPath(const unsigned char *src, unsigned default_drv, char *dst, unsigned bufsiz)
{
    int drv = getDrvNo(src[0]);
    if ((drv >= 0) && (src[1] == ':'))
    {
        src += 2;
    }
    else
    {
        drv = default_drv;
    }

    if (src[0] == '\\')
    {
        // absolute Atari path
        const char *host_root = drv_host_path[drv];
        if (host_root != nullptr)
        {
            unsigned len = strlen(host_root);
            if (len >= bufsiz)
            {
                DebugError2("() - buffer overflow");
                return -1;
            }
            strcpy(dst, host_root);
            dst += len;
            bufsiz -= len;
        }
    }

    bool upperCase = drv_caseInsens[drv];   // 8+3 drives usually also are case-insensitive
    return atariFnameToHostFname(src, upperCase, dst, bufsiz);
}


/** **********************************************************************************************
 *
 * @brief [static] Convert host filename to Atari filename, character and path separator
 *
 * @param[in]   src      host filename
 * @param[out]  dst      Atari filename
 * @param[in]   bufsiz   buffer size, including end-of-string.
 *
 * @return -1 on overflow, otherwise zero
 *
 * @note There is no uppercase conversion.
 *
 ************************************************************************************************/
int CHostXFS::hostFnameToAtariFname(const char *src, unsigned char *dst, unsigned bufsiz)
{
    unsigned char *buf_start = dst;
    while (*src && (dst < buf_start + bufsiz - 1))
    {
        if (*src == '/')
        {
            src++;
            *dst++ = '\\';
        }
        else
        {
            // utf-8 -> Atari
            unsigned len = CConversion::charHost2Atari(src, dst);
            src += len;
            dst++;
        }
    }

    *dst = EOS;
    if (*src)
    {
        DebugError2("() -- file name length overflow: %s", buf_start);
    }

    return (*src == '\0') ? 0 : -1;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert host filename to Atari 8+3 filename
 *
 * @param[in]   host_fname      host filename
 * @param[out]  dosname         Atari filename
 * @param[in]   upperCase       convert to uppercase
 *
 * @return true: filename was shortened
 *
 ************************************************************************************************/
bool CHostXFS::hostFnameToAtariFname8p3(const char *host_fname, unsigned char *dosname, bool upperCase)
{
    return nameto_8_3(host_fname, dosname, upperCase, true);
}


/** **********************************************************************************************
 *
 * @brief Convert host path to Atari path
 *
 * @param[in]   src          host path
 * @param[in]   default_drv  Atari drive, if src does not start with an Atari root
 * @param[out]  dst          buffer for Atari path
 * @param[in]   bufsiz       buffer size, including end-of-string.
 *
 * @return -1 on overflow, otherwise zero
 *
 ************************************************************************************************/
int CHostXFS::hostPath2AtariPath(const char *src, unsigned default_drv, char unsigned *dst, unsigned bufsiz)
{
    if (bufsiz < 4)
    {
        return -1;
    }

    unsigned drv;
    for (drv = 0; drv < NDRIVES; drv++)
    {
        const char *host_root = drv_host_path[drv];
        if (host_root != nullptr)
        {
            unsigned len = strlen(host_root);
            if (!strncmp(src, host_root, len))
            {
                *dst++ = drv + 'A';
                *dst++ = ':';
                *dst++ = '\\';
                bufsiz -= 3;
                src += len;
                if (src[0] == '/')
                {
                    src++;  // avoid double separator
                }
                break;
            }
        }
    }

    if (drv >= NDRIVES)
    {
        *dst++ = default_drv + 'A';
        *dst++ = ':';
        *dst++ = '\\';
        bufsiz -= 3;
    }

    return hostFnameToAtariFname(src, dst, bufsiz);
}


/** **********************************************************************************************
 *
 * @brief [static] Check if an 8+3 filename matches an 8+3 search pattern (each 12 bytes)
 *
 * @param[in]   pattern      internal 8+3 representation of search pattern, for Fsfirst() and Fsnext()
 * @param[out]  fname        internal 8+3 representation of filename
 * @param[in]   upperCase    case insensitive compare
 *
 * @return match result
 *
 * @note Byte 11 is the search attribute, respectively the file attribute. '?' is a wildcard.
 *
 * @note Attribute comparison rules:
 *    1) ReadOnly and Archive are always ignored.
 *    2) If the search attribute is 8, exactly those files with Volume flag match,
 *       also hidden ones.
 *    3) If search attribute is different from 8, regular files always match.
 *    4) If search attribute is different from 8, directories are only found if bit 4 is set.
 *    5) If search attribute is different from 8, volume names are only found if bit 3 is set.
 *    6) If search attribute is different from 8, hiden and system files, including
 *       directories and volume names, are only match, if the respective bit in the search
 *       attribute is set.
 *
 *  @note match examples (bits ReadOnly and Archive are ignored):
 *    8    all files with bit 3 set (volumes)
 *    0    only regular files
 *    2    regular and hidden files
 *    6    regular, hidden and system files
 *  $10    regular files and regular subdirectories
 *  $12    regular and hidden files and subdirectories
 *  $16    regular and hidden and system files and directories
 *   $a    regular and hidden files and volume names
 *   $e    regular and hidden and system files and volume names
 *  $1e    everything
 *
 ************************************************************************************************/
bool CHostXFS::filename8p3_match(const char *pattern, const char *fname, bool upperCase)
{
    if (fname[0] == '\xe5')     // search for deleted file
    {
        return((pattern[0] == '?') || (pattern[0] == fname[0]));
    }

    // compare 11 characters (8 plus 3)

    for (int i = 10; i >= 0; i--)
    {
        char c1 = *pattern++;
        char c2 = *fname++;
        if (c1 != '?')
        {
            if (upperCase)
            {
                c1 = CConversion::charAtari2UpperCase(c1);
                c2 = CConversion::charAtari2UpperCase(c2);
            }
            if (c1 != c2)
            {
                return false;
            }
        }
    }

    // compare attribute

    char c1 = *pattern;
    char c2 = *fname;

//    if (c1 == F_ARCHIVE)
//        return(c1 & c2);
    c2 &= F_SUBDIR + F_SYSTEM + F_HIDDEN + F_VOLUME;
    if (!c2)
        return true;
    if ((c2 & F_SUBDIR) && !(c1 & F_SUBDIR))
        return false;
    if ((c2 & F_VOLUME) && !(c1 & F_VOLUME))
        return false;
    if (c2 & (F_HIDDEN | F_SYSTEM))
    {
        c2 &= (F_HIDDEN | F_SYSTEM);
        c1 &= (F_HIDDEN | F_SYSTEM);
    }

    return (bool) (c1 & c2);
}


/** **********************************************************************************************
 *
 * @brief [static] Convert first Atari path element to internal 8+3 format in Atari DTA
 *
 * @param[in]   path        Atari path
 * @param[out]  name        internal 8+3 representation, for Fsfirst() and Fsnext()
 * @param[in]   upperCase   convert to uppercase
 *
 * @return true: the path element was truncated
 *
 * @note On file systems with long filenames, Fsfirst()/Fsnext() should not convert to
 *       uppercase, unless the file system is case insensitive.
 *
 * The first path element (before '\' or EOS) is converted to the 8+3 format that is internally
 * stored inside the DTA.
 *
 ************************************************************************************************/
bool CHostXFS::pathElemToDTA8p3(const unsigned char *path, unsigned char *name, bool upperCase)
{
    bool truncated = false;

    // special case "." and ".."

    if (path[0] == '.')
    {
        if (path[1] == '\0')
        {
            *name++ = '.';      // "." -> ".          "
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            return false;
        }

        if ((path[1] == '.') && (path[2] == '\0'))
        {
            *name++ = '.';      // ".." -> "..         "
            *name++ = '.';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            *name++ = ' ';
            return false;
        }
    }

    // copy a maximum of eight characters for filename

    int i;
    for (i = 0; (i < 8) && (*path) &&
         (*path != '\\') && (*path != '*') && (*path != '.') && (*path != ' '); i++)
    {
        *name++ = (upperCase) ? CConversion::charAtari2UpperCase(*path++) : *path++;
    }

    // determine the fill character, that will be used to fill the complete eight characters

    if (i == 8)
    {
        while ((*path) && (*path != ' ') && (*path != '\\') && (*path != '.'))
        {
            path++;
            truncated = true;
        }
    }

    char c = (*path == '*') ? '?' : ' ';
    if (*path == '*')
        path++;
    if (*path == '.')
        path++;

    // fill up the remaining name up to eight characters

    for (;i < 8; i++)
    {
        *name++ = c;
    }

    // copy a maximum of three characters for the filename extension

    for (i = 0; (i < 3) && (*path) &&
         (*path != '\\') && (*path != '*') && (*path != '.') && (*path != ' '); i++)
    {
        *name++ = (upperCase) ? CConversion::charAtari2UpperCase(*path++) : *path++;
    }

    if ((*path) && (*path != '\\') && (*path != '*'))
        truncated = true;

    // determine the fill character

    c = (*path == '*') ? '?' : ' ';

    // fill up the remaining extension up to three characters

    for (;i < 3; i++)
    {
        *name++ = c;
    }

    return truncated;
}


// check if drive is valid
#define CHK_DRIVE(DRV) \
    if (drv_changed[DRV]) \
    { \
        return E_CHNG; \
    } \
    if (drv_host_path[DRV] == nullptr) \
    { \
        return EDRIVE; \
    }

// check if drive is valid and writeable
#define CHK_DRIVE_WRITEABLE(DRV) \
    CHK_DRIVE(DRV) \
    if (drv_readOnly[DRV]) \
    { \
        return EWRPRO; \
    }


/** **********************************************************************************************
 *
 * @brief Atari callback: Synchronise a drive, i.e. write back caches
 *
 * @param[in] drv        Atari drive number 0..25
 *
 * @return 0 = OK   < 0 = error  > 0 in progress
 *
 * @note This call has currently no functionality for the host file system.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_sync(uint16_t drv)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE(drv)
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Tells the host that an Atari process has terminated
 *
 * @param[in] pd        Atari process descriptor
 *
 * @note used for tidy-up, e.g. close hanging Fsnext handles.
 *
 ************************************************************************************************/
void CHostXFS::xfs_pterm(uint32_t pd, uint8_t *addrOffset68k)
{
    DebugInfo2("() -- pd 0x%08x", pd);
    (void) addrOffset68k;

    // PD = addrOffset68k + be32toh(pptermparm->pd)), addrOffset68k;
    uint32_t act_pd = getActPd();
    DebugInfo2("() -- PD 0x%08x terminated, act_pd = 0x%08x", pd, act_pd);
    (void) act_pd;
    HostHandles::snextPterm(pd);
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Tells the host that a Directory Descriptor will be deallocated
 *
 * @param[in] dd                MagiC directory descriptor
 * @param[in] addrOffset68k     needed to dereference pointers in dd
 *
 * @note New for MagicOnLinux
 *
 ************************************************************************************************/
void CHostXFS::xfs_freeDD(XFS_DD *dd, uint8_t *addrOffset68k)
{
    DebugInfo2("()");
    XFS_DMD *hdmd = (XFS_DMD *) (addrOffset68k + be32toh(dd->dd_dmd));
    DebugInfo2("() : drive = %u", be16toh(hdmd->d_drive));
    (void) hdmd;
    MXFSDD *hdd = (MXFSDD *) dd->data;
    DebugInfo2("() : dirID = %u", hdd->dirID);
    DebugInfo2("() : vRefNum = %u", hdd->vRefNum);
    HostHandle_t hhdl = hdd->dirID;  // host endian
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning2("%s() -- invalid hostFD");
    }
    else
    {
        freeHostFD(hostFD);
    }
    (void) dd;
}


/** **********************************************************************************************
 *
 * @brief Helper function to open a host path, relative or absolute
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  reldir       relative directory or nullptr
 * @param[in]  rel_hhdl     hhdl for relative directory
 * @param[in]  path         host path
 * @param[in]  flags        O_PATH for root, otherwise O_DIRECTORY | O_RDONLY
 * @param[out] hhdl         handle to be passed to Atari
 *
 * @return Atari error code
 *
 ************************************************************************************************/
INT32 CHostXFS::hostpath2HostFD
(
    uint16_t drv,
    HostFD *reldir,
    uint16_t rel_hhdl,
    const char *path,
    int flags,
    HostHandle_t *hhdl
)
{
    HostFD *hostFD = getFreeHostFD();   // note that this is not allocated, yet
    if (hostFD == nullptr)
    {
        DebugError2("() : No host FDs left");
        *hhdl = HOST_HANDLE_INVALID;
        return ENHNDL;
    }

    int rel_fd = (reldir != nullptr) ? reldir->fd : -1;
    DebugInfo2("() : rel host fd = %d, path = \"%s\"", rel_fd, path);
    if ((rel_fd >= 0) && (path[0] == '\0'))
    {
        // the relative directory itself is addressed with an empty path
        reldir->ref_cnt++;  // TODO: correct?
        *hhdl = rel_hhdl;
        DebugInfo2("() : host fd = %d (reused)", rel_fd);
    }
    else
    {
        struct stat statbuf;
        int res = fstatat(rel_fd, path, &statbuf, AT_EMPTY_PATH);
        if (res < 0)
        {
            DebugWarning2("() : fstatat(\"%s\") -> %s", path, strerror(errno));
            *hhdl = HOST_HANDLE_INVALID;
            return CConversion::host2AtariError(errno);
        }
        DebugInfo2("() : dev=%d, ino=%d)", statbuf.st_dev, statbuf.st_ino);
        uint16_t nhhdl;
        HostFD *hostFD_exist = findHostFD(statbuf.st_dev, statbuf.st_ino, &nhhdl);
        if (hostFD_exist != nullptr)
        {
             DebugInfo2("() : already opened host fd = %d", hostFD_exist->fd);
            *hhdl = nhhdl;
            return E_OK;
        }

        int dir_fd = openat(rel_fd, path, flags);
        if (dir_fd < 0)
        {
            DebugWarning2("() : openat(\"%s\") -> %s", path, strerror(errno));
            *hhdl = HOST_HANDLE_INVALID;
            return CConversion::host2AtariError(errno);
        }

        //
        // Check if hostFD->fd is valid, i.e. is inside this Atari drive
        //

        char pathbuf[1024];
        INT32 aret = hostFd2Path(dir_fd, pathbuf, sizeof(pathbuf));
        if (aret != E_OK)
        {
            close(dir_fd);
            return aret;
        }
        const char *host_root = drv_host_path[drv];
        unsigned len = strlen(host_root);
        if (strncmp(pathbuf, host_root, len))
        {
            DebugError2("() -- host path is located outside Atari drive %c: \"%s\"", 'A' + drv, pathbuf);
            close(dir_fd);
            return EPTHNF;
        }

        //
        // new path is valid, continue
        //

        hostFD->fd = dir_fd;
        int ret = fstat(hostFD->fd, &statbuf);
        if (ret < 0)
        {
            close(hostFD->fd);
            DebugWarning2("() : fstat(\"%s\") -> %s", path, strerror(errno));
            *hhdl = HOST_HANDLE_INVALID;
            return CConversion::host2AtariError(errno);
        }

        //DebugInfo2("() : dev=%d, ino=%d)", statbuf.st_dev, statbuf.st_ino);
        hostFD->dev = statbuf.st_dev;
        hostFD->ino = statbuf.st_ino;

        *hhdl = allocHostFD(&hostFD);
        DebugInfo2("() : host fd = %d", hostFD->fd);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Open a drive, i.e. fill in a descriptor for it, if valid
 *
 * @param[in]  drv                   Atari drive number 0..25 for A: .. Z:
 * @param[out] dd                    directory descriptor of the drive's root directory
 * @param[in]  flg_ask_diskchange    only ask if disk has been changed
 *
 * @return 0 for OK or negative error code
 *
 * @note Due to a bug in Calamus (Atari program), drive M: must never return an error code.
 * @note Due to a design flaw, dd points to a 6-byte memory block located on the 68k stack.
 *       MACXFS.S copies these six bytes later to a newly allocated DD block that would have space for
 *       94 bytes, ie. another 88 bytes.
 *
 * @note If this function returns error EDRIVE, the MagiC kernel will try to open the drive
 *       with the internal FAT file system driver.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_drv_open(uint16_t drv, MXFSDD *dd, int32_t flg_ask_diskchange)
{
    DebugInfo2("(drv = %u (%c:), flg = 0x%08x)", drv, 'A' + drv, flg_ask_diskchange);

    if (flg_ask_diskchange)
    {
        DebugInfo2("() -> %d", (drv_changed[drv]) ? E_CHNG : E_OK);
        return (drv_changed[drv]) ? E_CHNG : E_OK;
    }

    drv_changed[drv] = false;        // reset disk change

    const char *pathname = drv_host_path[drv];
    if (pathname == nullptr)
    {
        DebugInfo2("() -> EDRIVE");
        return EDRIVE;
    }

    // Obtain handle for root directory
    // Note that the combination
    //  name_to_handle_at() .. open_by_handle_at()
    // does not work at all for user applications and just leads to
    // "operation not permitted".
    // The call
    //  name_to_handle_at(-1, path, &fd->fh,  &fd->mount_id, AT_HANDLE_FID)
    // could only be used to retrieve a unique file handle to later
    // identify the file, otherwise the "fh" will be useless.

    DebugInfo2("() : open directory \"%s\"", pathname);

    HostHandle_t hhdl;
    INT32 atari_ret = hostpath2HostFD(drv, nullptr, -1, pathname, /* O_PATH*/ O_DIRECTORY | O_RDONLY, &hhdl);

    dd->dirID = hhdl;           // host endian format
    dd->vRefNum = drv;          // host endian
    DebugInfo2("() -> dirID %u", hhdl);

    return atari_ret;
}


/** **********************************************************************************************
 *
 * @brief Make an Atari drive invalid ("close")
 *
 * @param[in]  drv      Atari drive number 0..25
 * @param[in]  mode     0: normal operation / 1: always return E_OK
 *
 * @return 0 for OK or negative error code
 *
 * @note TODO: free all handles referring to that "drive"
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_drv_close(uint16_t drv, uint16_t mode)
{
    (void) mode;
    DebugInfo2("(drv = %u, mode = %u)", drv, mode);
    CHK_DRIVE(drv)

    drv_host_path[drv] = nullptr;
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Open Atari path or partial path, up to symbolic link or filename
 *
 * @param[in]  mode         0: pathname is a file, 1: pathname is a directory
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  rel_dd       parent directory, search from here
 * @param[in]  pathname     Atari path
 * @param[out] remain_path  remaining path after evaluation, i.e. filename if mode=1, or after symlink
 * @param[out] symlink_dd   directory of the first symbolic link in the path
 * @param[out] symlink      link destination as Pascal string, on even address, 16-bit for length
 * @param[out] dd           if success, this structure describes the opened directory
 * @param[out] dir_drive    normally drv (Attention: convert to big-endian!)
 *
 * @note <symlink> and <symlink_dd> are only evaluated in case the function returns ELINK.
 *       <remain_path> then shall point to the remaining path after the symbolic link.
 * @note The symbolic link handling was designed for classic MacOS. The link contains a string
 *       describing the link destination. This string returned in <symlink> must be located on an
 *       even address, zero-terminated and is preceded with 16-bit length (including trailing zero).
 *       The length is stored in  big-endian mode. The storage <symlink> points to
 *       will be immmediately copied by the MagiC kernel.
 * @note Return NULL for symlink if the parent of the root directory was selected. This allows
 *       the MagiC kernel to go from U:\A back to U:\.
 *       TODO: test and implement, if missing
 * @note <remain_path> is returned without leading path separator "\"
 * @note <dir_drive> might be different from drv in case the drive is changed during
 *       path evaluation.
 *
 * @return 0 for OK, ELINK for symbolic link or negative error code
 *
 * @note Due to a design flaw, dd points to a 6-byte memory block located on the 68k stack.
 *       MACXFS.S copies these six bytes later to a newly allocated DD block that would have space for
 *       94 bytes, ie. another 88 bytes.
 * @note Unfortunately, the XFS function mxfs_freeDD() in MACXFS.S just frees the internal
 *       memory block (IMB) instead of calling the host before. This is a severe design
 *       flaw in MAC_XFS and has historical reasons, because classic MacOS could address
 *       each file or folder in the system with a combination of a 16-bit volume reference
 *       number and a 32-bit file number, kind of inode.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_path2DD
(
    uint16_t mode,
    uint16_t drv, const MXFSDD *rel_dd, const char *pathname,
    const char **remain_path, MXFSDD *symlink_dd, const char **symlink,
    MXFSDD *dd,
    UINT16 *dir_drive
)
{
    DebugInfo2("(drv = %u (%c:), mode = %d, dirID = %d, vRefNum = %d, name = \"%s\")",
         drv, 'A' + drv, mode, rel_dd->dirID, rel_dd->vRefNum, pathname);
    CHK_DRIVE(drv)

    // host path may contain multi-byte characters!
    char host_pathbuf[1024];
    char *host_p;
    // Atari path has single-byte characters only
    const char *atari_p;

    bool upperCase = drv_caseInsens[drv];   // 8+3 drives usually also are case-insensitive
    atariFnameToHostFname((const uint8_t *) pathname, upperCase, host_pathbuf, 1024);
    DebugInfo2("() - host path is \"%s\"", host_pathbuf);
    if (mode == 0)
    {
        // The path refers to a file. Remove filename from path.
        host_p = strrchr(host_pathbuf, '/');
        if (host_p != nullptr)
        {
            *host_p++ = 0;
            atari_p = strrchr(pathname, '\\');
            atari_p++;
        }
        else
        {
            host_p = host_pathbuf;    // the file is directly located inside the given directory
            *host_p = '\0';
            atari_p = pathname;
        }
        DebugInfo2("() - remaining host path with filename removed is \"%s\"", host_pathbuf);
    }
    else
    {
        host_p = host_pathbuf + strlen(host_pathbuf);
        atari_p = pathname + strlen(pathname);
    }

    HostHandle_t hhdl_rel = rel_dd->dirID;  // host endian
    HostFD *rel_hostFD = getHostFD(hhdl_rel);
    if (rel_hostFD == nullptr)
    {
        DebugWarning2("%s() -> EINTRN");
        return EINTRN;
    }

    // O_PATH or O_DIRECTORY | O_RDONLY?
    HostHandle_t hhdl;
    INT32 atari_ret = hostpath2HostFD(drv, rel_hostFD, hhdl_rel, host_pathbuf, /*O_PATH?*/ O_DIRECTORY | O_RDONLY, &hhdl);

    /*
    // Note that O_DIRECTORY is essential, otherwise fdopendir() will refuse
    int dir_fd = openat(rel_fd, pathbuf, O_DIRECTORY | O_RDONLY);
    if (dir_fd < 0)
    {
        DebugWarning2("() : openat() -> %s", strerror(errno));
        return CConversion::Host2AtariError(errno);
    }
    HostHandle_t hhdl = HostHandles::alloc(sizeof(dir_fd));
    if (hhdl == HOST_HANDLE_INVALID)
    {
        DebugError("No host handles left");
        return ENHNDL;
    }
    HostHandles::putInt(hhdl, dir_fd);
    */

    dd->dirID = hhdl;
    dd->vRefNum = rel_dd->vRefNum;

    int len = atari_p - pathname;  // length of consumed atari path
    *remain_path = pathname + len;
    *dir_drive = htobe16(drv);  // big endian

    // dummy, no symlink handling
    *symlink = pathname + strlen(pathname);
    symlink_dd->dirID = -1;
    symlink_dd->vRefNum = dd->vRefNum;

    /*
    DebugWarning("Atari interrupts disabled for debugging. Remove this later!");
    extern bool CMagiC__sNoAtariInterrupts;
    CMagiC__sNoAtariInterrupts = true;      // TODO: remove!!
    extern volatile unsigned char sExitImmediately;
    sExitImmediately = 0;
    */

    // TODO: How and when will thís handle ever be closed?
    //       Maybe we should reuse the same handle for the
    //       same directory?

    DebugInfo2("() -> dirID %u", hhdl);
    return atari_ret;
}


/** **********************************************************************************************
 *
 * @brief Check if a host directory entry matches Fsfirst/next search pattern
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dir_fd       directory
 * @param[in]  entry        directory entry
 * @param[out] dta          file found and internal data for Fsnext
 *
 * @return 0: found, 1: mismatch, <0: error
 *
 * @note The drive number is needed to determine, if the file system is case-insensitive
 *
 ************************************************************************************************/
int CHostXFS::_snext(uint16_t drv, int dir_fd, const struct dirent *entry, MAC_DTA *dta)
{
    unsigned char atariname[256];   // long filename in Atari charset
    unsigned char dosname[14];      // internal, 8+3

    DebugInfo2("() - %d \"%s\"", entry->d_type, entry->d_name);
    // Convert character set and path separator ('\' -> '/')
    hostFnameToAtariFname(entry->d_name, atariname, sizeof(atariname));
    bool convUpper = drv_caseInsens[drv];
    if (pathElemToDTA8p3(atariname, dosname, convUpper))  // .. and to 8+3
    {
        return -1;   // filename too long
    }

    // TODO: handle symbolic links here
    if (entry->d_type == DT_REG)
    {
        dosname[11] = 0;    // regular file
    }
    else
    if (entry->d_type == DT_DIR)
    {
        dosname[11] = F_SUBDIR;
    }
    else
    {
        DebugWarning2("() -- unhandled file type %d", entry->d_type);
        return -2;   // unhandled file type
    }

    if (!filename8p3_match(dta->macdta.sname, (char *) dosname, convUpper))
    {
        return 1;
    }

    int fd = openat(dir_fd, entry->d_name, O_RDONLY);
    if (fd < 0)
    {
        DebugWarning2("() : openat(\"%s\") -> %s", entry->d_name, strerror(errno));
        return -3;
    }
    struct stat statbuf;
    int ret = fstat(fd, &statbuf);
    close(fd);
    if (ret < 0)
    {
        DebugWarning2("() : fstat(\"%s\") -> %s", entry->d_name, strerror(errno));
        return -4;
    }
    // DebugInfo2("() - file size = %lu\n", statbuf.st_size);

    //
    // fill DTA
    //

    if (dosname[11] == F_SUBDIR)
    {
        dta->mxdta.dta_len = 0;
    }
    else
    if (statbuf.st_size > 0xffffffff)
    {
        DebugWarning2("() -- file size > 4 GB, shown as zero length");
        dta->mxdta.dta_len = 0;
    }
    else
    {
        dta->mxdta.dta_len = htobe32((uint32_t) statbuf.st_size);
    }

    dta->mxdta.dta_attribute = (char) dosname[11];
    hostFnameToAtariFname8p3(entry->d_name, (unsigned char *) dta->mxdta.dta_name, convUpper);
    const struct timespec *mtime = &statbuf.st_mtim;
    uint16_t time, date;
    CConversion::hostDateToDosDate(mtime->tv_sec, &time, &date);
    dta->mxdta.dta_time = htobe16(time);
    dta->mxdta.dta_date = htobe16(date);

    return 0;
}


// perform various checks to the directory file descriptor
#define GET_hhdl_hostFD_dir_fd(DD, HHDL, HOST_FD, DIR_FD) \
    HostHandle_t HHDL = DD->dirID; \
    HostFD *HOST_FD = getHostFD(HHDL); \
    if (HOST_FD == nullptr) \
    { \
        return EINTRN; \
    } \
    int DIR_FD = HOST_FD->fd; \
    if (DIR_FD == -1) \
    { \
        return EINTRN; \
    }


/** **********************************************************************************************
 *
 * @brief Scan a directory and remember search position for following Fsnext
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, search here
 * @param[in]  name         Search pattern in 8+3 name scheme
 * @param[out] dta          file found and internal data for Fsnext
 * @param[out] attrib       search attributes
 *
 * @return 0 for OK, ELINK for symbolic link or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_sfirst
(
    uint16_t drv,
    const MXFSDD *dd,
    const char *name,
    MAC_DTA *dta,
    uint16_t attrib
)
{
    DebugInfo2("(drv = %u, name = \"%s\")", drv, name);

    dta->mxdta.dta_drive = (char) drv;
    bool convUpper = !drv_longNames[drv] || drv_caseInsens[drv];
    pathElemToDTA8p3((const unsigned char *) name, (unsigned char *) dta->macdta.sname, convUpper);    // search pattern -> DTA
    dta->mxdta.dta_name[0] = 0;                    // nothing found yet
    dta->macdta.sattr = (char) attrib;            // search attribute

    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    // Got host fd for the directory.
    // Note that an fdopendir(), followed by readdir(), advances the file
    // read pointer while walking through the directory entries. Thus,
    // we must rewind it here, because there might have been
    // Fsfirst/Fsnext operations here before.

    off_t lret = lseek(dir_fd, 0, SEEK_SET);
    if (lret < 0)
    {
        DebugWarning2("() : lseek() -> %s", strerror(errno));
    }

    // Duplicate the dir fd before opening it, otherwise it would also be
    // closed on Dclosedir().
    DebugInfo2("() - open directory from host fd %d", dir_fd);
    int dup_dir_fd = dup(dir_fd);
    DIR *dir = fdopendir(dup_dir_fd);
    if (dir == nullptr)
    {
        DebugWarning2("() : fdopendir() -> %s", strerror(errno));
        close(dup_dir_fd);
        return CConversion::host2AtariError(errno);
    }

    for (;;)
    {
        errno = 0;  // strange, but following advice in man page
        struct dirent *entry = readdir(dir);
        if (entry == nullptr)
        {
            if (errno != 0)
            {
                DebugWarning2("() : readdir() -> %s", strerror(errno));
            }
            break;  // end of directory
        }

        int match = _snext(drv, dir_fd, entry, dta);
        if (match == 0)
        {
            // directory entry matches search pattern

            //long pos = telldir(dir);
            //DebugInfo2("() : directory read position %ld", pos);

            dta->macdta.vRefNum = (int16_t) HostHandles::snextSet(dir, dup_dir_fd, getActPd());
            dta->macdta.dirID = hhdl;
            dta->macdta.index = 0;  // unused

            DebugInfo2("() -> E_OK");
            return E_OK;
        }
    }

    dta->macdta.sname[0] = EOS;     // invalidate DTA
    dta->macdta.dirID = -1;
    dta->macdta.vRefNum = -1;
    dta->macdta.index = -1;

    closedir(dir);  // also closes dup_dir_fd

    DebugInfo2("() -> EFILNF");
    return EFILNF;
}


/** **********************************************************************************************
 *
 * @brief For Fsnext() : scan a directory for the next matching entry
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[out] dta          file found and internal data for Fsnext
 *
 * @return 0 for OK, ELINK for symbolic link or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_snext(uint16_t drv, MAC_DTA *dta)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE(drv)
    (void) dta;

    if (!dta->macdta.sname[0])
    {
        DebugWarning2("() -> ENMFIL");
        return ENMFIL;
    }

    DIR *dir;
    uint16_t snextHdl = (uint16_t) dta->macdta.vRefNum;
    int dup_fd;
    if (HostHandles::snextGet(snextHdl, &dir, &dup_fd))
    {
        DebugWarning2("() -> EINTRN");
        return EINTRN;
    }

    for (;;)
    {
        errno = 0;  // strange, but following advice in man page
        struct dirent *entry = readdir(dir);
        if (entry == nullptr)
        {
            if (errno != 0)
            {
                DebugWarning2("() : readdir() -> %s", strerror(errno));
            }
            break;  // end of directory
        }

        int match = _snext(drv, dup_fd, entry, dta);
        if (match == 0)
        {
            //long pos = telldir(dir);
            //DebugInfo2("() : directory read position %ld", pos);
            DebugInfo2("() -> E_OK");
            return E_OK;
        }
    }

    dta->macdta.sname[0] = EOS;     // invalidate DTA
    dta->macdta.dirID = -1;
    dta->macdta.vRefNum = -1;
    dta->macdta.index = -1;

    HostHandles::snextClose(snextHdl);  // also does closedir()

    DebugInfo2("() -> ENMFIL");

    return ENMFIL;
}


/** **********************************************************************************************
 *
 * @brief Open a file and create it, if requested
 *
 * @param[in]  name         filename, can be long or 8+3
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, search here
 * @param[in]  omode        bit mask, see OM_RPERM and _ATARI_O_... bits
 * @param[out] attrib       attribute of new file in case O_CREAT bit is set
 *
 * @return 16-bit file handle or 32-bit negative error code
 *
 * @note For historical reasons the file handle must fit to 16-bits, because the structure
 *       MAC_FD designed for MagiCMac only reserves space for a classic MacOS refNum.
 * @note The Atari part of the HostXFS, in particular MACXFS.S, gets the return value of
 *       this function as big-endian and stores it to the MAC_FD.
 * @note The Atari part of the file system driver sets fd.mod_tdate_dirty to FALSE.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fopen
(
    const unsigned char *name,
    uint16_t drv,
    MXFSDD *dd,
    uint16_t omode,
    uint16_t attrib
)
{
    (void) attrib;

    DebugInfo2("(name = \"%s\", drv = %u, omode = %d, attrib = %d)", name, drv, omode, attrib);
    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    int host_oflags = -1;

    if (omode & OM_RPERM)
    {
        host_oflags = O_RDONLY;
    }
    if (omode & OM_WPERM)
    {
        host_oflags = (host_oflags == O_RDONLY) ? O_RDWR : O_WRONLY;
        if (drv_readOnly[drv])
        {
            DebugInfo2("() -> EWRPRO");
            return EWRPRO;
        }
    }
    if (omode & _ATARI_O_APPEND)
    {
        if (drv_readOnly[drv])
        {
            DebugInfo2("() -> EWRPRO");
            return EWRPRO;
        }
        host_oflags |= O_APPEND;
    }
    if (omode & _ATARI_O_CREAT)
    {
        if (drv_readOnly[drv])
        {
            DebugInfo2("() -> EWRPRO");
            return EWRPRO;
        }
        host_oflags |= O_CREAT;
    }
    if (omode & _ATARI_O_TRUNC)
    {
        if (drv_readOnly[drv])
        {
            DebugInfo2("() -> EWRPRO");
            return EWRPRO;
        }
        host_oflags |= O_TRUNC;
    }

    /*
    so far ignore these
    #define _ATARI_O_COMPAT    0
    #define _ATARI_O_DENYRW    0x10
    #define _ATARI_O_DENYW     0x20
    #define _ATARI_O_DENYR     0x30
    #define _ATARI_O_DENYNONE  0x40
    #define _ATARI_O_EXCL      0x800
   */

    HostFD *file_hostFD = getFreeHostFD();
    if (file_hostFD == nullptr)
    {
        return ENHNDL;
    }

    file_hostFD->fd = openat(dir_fd, host_name, host_oflags, new_file_perm);
    if (file_hostFD->fd < 0)
    {
        DebugWarning2("() : openat(\"%s\") -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }
    DebugInfo2("() - host fd %d", file_hostFD->fd);

    struct stat statbuf;
    int ret = fstat(file_hostFD->fd, &statbuf);
    if (ret < 0)
    {
        close(file_hostFD->fd);
        DebugWarning2("() : fstat(\"%s\") -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }
    file_hostFD->dev = statbuf.st_dev;
    file_hostFD->ino = statbuf.st_ino;

    hhdl = allocHostFD(&file_hostFD);

    assert(hhdl <= 0xfffff);
    DebugInfo2("() -> %d", hhdl);
    return hhdl;
}


/** **********************************************************************************************
 *
 * @brief For Fdelete() - remove a file
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory where the file is located
 * @param[in]  name         name of file to delete
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note If the file is an alias, the alias should be removed, not the directory
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fdelete(uint16_t drv, MXFSDD *dd, const unsigned char *name)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    // with flags AT_REMOVEDIR we could remove directories, what do not want here
    if (unlinkat(dir_fd, host_name, 0))
    {
        DebugError2("() : unlinkat(\"%s\") -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Frename() and Flink() - create a hardlink or rename or move a file or folder
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  name_from    name of existing file or folder
 * @param[in]  name_to      new name
 * @param[in]  dd_from      directory where the file or folder is located
 * @param[in]  dd_to        directory where the file or folder shall be located
 * @param[in]  mode         1: hard link, 0: move or rename
 * @param[in]  dst_drv      Atari drive where the file or folder shall be located
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note If the file is an alias, the alias should be moved, not the file or directory
 * @note Even if the Atari drive of source and destination differ, the host volume
 *       may be the same.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_link
(
    uint16_t drv,
    const unsigned char *name_from,
    const unsigned char *name_to,
    MXFSDD *dd_from,
    MXFSDD *dd_to,
    uint16_t mode,
    uint16_t dst_drv
)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE_WRITEABLE(drv)
    CHK_DRIVE_WRITEABLE(dst_drv)

    GET_hhdl_hostFD_dir_fd(dd_from, hhdl_from, hostFD_from, dir_fd_from)
    GET_hhdl_hostFD_dir_fd(dd_to, hhdl_to, hostFD_to, dir_fd_to)

    CONV8p3(drv, name_from, host_name_from)
    CONV8p3(drv, name_to, host_name_to)

    if (mode != 0)
    {
        // hard link: new directory entry shall refer to same file (volume and inode)

        if (linkat(dir_fd_from, host_name_from, dir_fd_to, host_name_to, 0))
        {
            DebugError2("() : linkat(\"%s\", \"%s\") -> %s", host_name_from, host_name_to, strerror(errno));
            return CConversion::host2AtariError(errno);
        }
    }
    else
    {
        // move or rename: old directory entry is removed, and new one is created that refers to the same file

        // without RENAME_NOREPLACE, an existing file might be removed here, which we do not allow
        if (renameat2(dir_fd_from, host_name_from, dir_fd_to, host_name_to, RENAME_NOREPLACE))
        {
            DebugError2("() : renameat2(\"%s\", \"%s\") -> %s", host_name_from, host_name_to, strerror(errno));
            return CConversion::host2AtariError(errno);
        }
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Helper for xfs_xattr() and dev_ioctl(FSTAT)
 *
 * @param[in]  pstat        host stat data
 * @param[out] pxattr       structure to be filled
 *
 ************************************************************************************************/
void CHostXFS::statbuf2xattr(XATTR *pxattr, const struct stat *pstat)
{
    uint16_t ast_mode = pstat->st_mode & 0777;    // copy rwx bits
    uint16_t attr = 0;
    switch(pstat->st_mode & S_IFMT)
    {
        case S_IFBLK:  ast_mode |= 0; break;              // block device
        case S_IFCHR:  ast_mode |= _ATARI_S_IFCHR; break; // character device
        case S_IFDIR:  ast_mode |= _ATARI_S_IFDIR;
                       attr |= F_SUBDIR; break;           // directory
        case S_IFIFO:  ast_mode |= _ATARI_S_IFIFO; break; // FIFO/pipe
        case S_IFLNK:  ast_mode |= _ATARI_S_IFLNK; break; // symlink
        case S_IFREG:  ast_mode |= _ATARI_S_IFREG; break; // regular file
        case S_IFSOCK: ast_mode |= 0; break;              // socket
        default:       ast_mode |= 0; break;              // unknown
    }
    if (!(pstat->st_mode & __S_IWRITE))
    {
        attr |= F_RDONLY;
    }
    uint16_t uid = pstat->st_uid;
    uint16_t gid = pstat->st_gid;
    if ((uid != pstat->st_uid) || (gid != pstat->st_gid))
    {
        DebugWarning2("() -- uid or gid overflow, set to zero");
        uid = 0;
        gid = 0;
    }
    uint16_t nlink = pstat->st_nlink;
    if (nlink != pstat->st_nlink)
    {
        DebugWarning2("() -- nlink overflow, set to 65535");
        nlink = 65535;
    }
    uint16_t dev = pstat->st_dev;
    if (dev != pstat->st_dev)
    {
        static int warned = 0;
        if (!warned)
        {
            DebugWarning2("() -- dev overflow, set to zero - NO MORE WARNINGS");
            warned = 1;
        }
        dev = 0;
    }
    uint32_t index = pstat->st_ino;
    if (index != pstat->st_ino)
    {
        DebugWarning2("() -- index overflow, set to zero");
        index = 0;
    }
    pxattr->mode = htobe16(ast_mode);
    pxattr->index = htobe32(index);
    pxattr->dev = htobe16(dev);
    pxattr->reserved1 = htobe16(0);
    pxattr->nlink = htobe16(nlink);
    pxattr->uid = htobe16(uid);
    pxattr->gid = htobe16(gid);
    if (pstat->st_size > 0xffffffff)
    {
        DebugWarning2("() -- file size > 4 GB, shown as zero length");
        pxattr->size = 0;
    }
    else
    {
        pxattr->size = htobe32((uint32_t) pstat->st_size);
    }
    pxattr->blksize = htobe32(pstat->st_blksize);  // TODO: check overflow
    pxattr->nblocks = htobe32(pstat->st_blocks);  // TODO: check overflow

    uint16_t time, date;
    CConversion::hostDateToDosDate(pstat->st_mtim.tv_sec, &time, &date);
    pxattr->mtime = htobe16(time);
    pxattr->mdate = htobe16(date);
    CConversion::hostDateToDosDate(pstat->st_atim.tv_sec, &time, &date);
    pxattr->atime = htobe16(time);
    pxattr->adate = htobe16(date);
    CConversion::hostDateToDosDate(pstat->st_ctim.tv_sec, &time, &date);
    pxattr->ctime = htobe16(time);
    pxattr->cdate = htobe16(date);

    pxattr->attr = htobe16(attr);
    pxattr->reserved2 = 0;
    pxattr->reserved3[0] = 0;
    pxattr->reserved3[1] = 0;
}


/** **********************************************************************************************
 *
 * @brief For Fxattr() - get information about a file without opening it
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, search here
 * @param[in]  name         filename, can be long or 8+3
 * @param[out] xattr        structure to be filled
 * @param[in]  mode         0: follow symlinks, otherwise do not follow
 *
 * @return E_OK or 32-bit negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_xattr
(
    uint16_t drv,
    MXFSDD *dd,
    const unsigned char *name,
    XATTR *xattr,
    uint16_t mode
)
{
    DebugInfo2("(name = \"%s\", drv = %u, mode = %d)", name, drv, mode);
    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    struct stat statbuf;
    int flags = AT_EMPTY_PATH;
    if (mode)
    {
        flags |= AT_SYMLINK_NOFOLLOW;
    }
    int res = fstatat(dir_fd, host_name, &statbuf, flags);
    if (res < 0)
    {
        DebugWarning2("() : fstatat(%s) -> %s", host_name, strerror(errno));
        memset(xattr, 0, sizeof(*xattr));
        return CConversion::host2AtariError(errno);
    }
    statbuf2xattr(xattr, &statbuf);

    DebugInfo2("() -> E_OK");
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Fattrib() - get or set information about a file without opening it
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, search here
 * @param[in]  name         filename, can be long or 8+3
 * @param[in]  rwflag       0: read, otherwise: write
 * @param[in]  attr         attribute to write if rwflag > 0
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note We can set or reset the F_RDONLY atribute, but nothing else. Contrary to the Atari,
 *       this should also work on folders.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_attrib(uint16_t drv, MXFSDD *dd, const unsigned char *name, uint16_t rwflag, uint16_t attr)
{
    DebugInfo2("(drv = %u, name = %s, rwflag = %u, attr = %u)", drv, name, rwflag, attr);
    CHK_DRIVE(drv)
    if (rwflag && drv_readOnly[drv])
    {
        return EWRPRO;
    }

    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    struct stat statbuf;
    int res = fstatat(dir_fd, host_name, &statbuf, AT_EMPTY_PATH);
    if (res < 0)
    {
        DebugWarning2("() : fstatat() -> %s", strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    uint16_t old_attr = 0;
    if  ((statbuf.st_mode & S_IFMT) == S_IFDIR)
    {
        old_attr |= F_SUBDIR;
    }
    if (!(statbuf.st_mode & __S_IWRITE))
    {
        old_attr |= F_RDONLY;
    }

    if (rwflag)
    {
        mode_t old_perm = statbuf.st_mode & 07777;  // extract permissions
        mode_t new_perm = old_perm;
        if (attr & F_RDONLY)
        {
            // write-protect, i.e. clear "write by owner" permission
            new_perm &= ~S_IWUSR;
        }
        else
        if (attr & F_RDONLY)
        {
            // write-allow, i.e. add "write by owner" permission
            new_perm |= S_IWUSR;
        }

        if (old_perm != new_perm)
        {
            // write-protect file or folder or un-write-protect it
            if (fchmodat(dir_fd, host_name, new_perm, 0))
            {
                DebugWarning2("() : fchmodat(%s) -> %s", host_name, strerror(errno));
                return CConversion::host2AtariError(errno);
            }
        }
        else
        {
            DebugWarning2("() attributes unchanged or not supported");
        }
    }

    DebugInfo2("() -> %u", old_attr);
    return old_attr;
}


/** **********************************************************************************************
 *
 * @brief For Fchown() - change owner id and group id for file or folder
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, search here
 * @param[in]  name         filename or directory name, can be long or 8+3
 * @param[in]  uid          new user id
 * @param[in]  gid          new group id
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note We could that implement later, but it has not much use.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fchown(uint16_t drv, MXFSDD *dd, const unsigned char *name,
                    uint16_t uid, uint16_t gid)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    if ((uid == 0) || (gid == 0))
    {
        DebugWarning2("() : invalid uid %u or gid %u", uid, gid);
        return EACCDN;
    }

    // TODO: shall we follow symbolic links here? We might change the flags then.
    if (fchownat(dir_fd, host_name, uid, gid, 0))
    {
        DebugWarning2("() : fchownat(%s) -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Fchmod() - change access mode for file or folder
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, search here
 * @param[in]  name         filename or directory name, can be long or 8+3
 * @param[in]  fmode        new mode
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note The bits are same for Atari and Unix/Linux. Here, we support all 12 bits, although
 *       the Atari documentation only mentions the lower 9.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fchmod(uint16_t drv, MXFSDD *dd, const unsigned char *name, uint16_t fmode)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    fmode &= 07777;
    if (fchmodat(dir_fd, host_name, fmode, 0))
    {
        DebugWarning2("() : fchmodat(%s) -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Dcreate() - create directory
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory, create the new one here
 * @param[in]  name         new directory name, can be long or 8+3
 *
 * @return E_OK or 32-bit negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dcreate(uint16_t drv, MXFSDD *dd, const unsigned char *name)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    // create directory with rwxrwxrwx access, which will then be ANDed with umask
    if (mkdirat(dir_fd, host_name, 0777) < 0)
    {
        DebugError2("() : openat(\"%s\") -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    DebugInfo2("() -> E_OK");
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Ddelete() - remove directory
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory to delete
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note If the directory is an alias, the alias should be removed, not the directory
 * @note The DD will be released by a following xfs_freeDD().
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_ddelete(uint16_t drv, MXFSDD *dd)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    // We cannot remove the directory via its DD or fd, instead we need a path
    char pathbuf[1024];
    INT32 aret = xfs_DD2hostPath(dd, pathbuf, 1023);
    if (aret != E_OK)
    {
        DebugError2("() -> %d", aret);
        return aret;
    }

    // Note that we shall not try to free the hostFD here, instead we
    // expect an xfs_freeDD() call from the MagiC DOS kernel.

    if (rmdir(pathbuf))
    {
        DebugWarning2("() : rmdir(\"%s\") -> %s (%d)", pathbuf, strerror(errno), errno);
        return CConversion::host2AtariError(errno);
    }

    DebugInfo2("() -> E_OK");
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Get host directory path, provides functionality for Dgetpath() and Ddelete()
 *
 * @param[in] dd        Atari directory descriptor
 * @param[in] pathbuf   buffer for host path
 * @param[in] bufsiz    buffer size
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_DD2hostPath(MXFSDD *dd, char *pathbuf, uint16_t bufsiz)
{
    DebugInfo2("()");

    pathbuf[0] = '\0';      // in case of error...
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    // get host path from directory file descriptor
    INT32 aret = hostFd2Path(dir_fd, pathbuf, bufsiz);

    DebugInfo2("() -> \"%s\"", pathbuf);
    return aret;
}


/** **********************************************************************************************
 *
 * @brief Get directory path, provides functionality for Dgetpath()
 *
 * @param[in] drv       Atari drive number 0..31
 * @param[in] dd        Atari directory descriptor
 * @param[in] buf       buffer for directory name
 * @param[in] bufsiz    buffer size
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_DD2name(uint16_t drv, MXFSDD *dd, char *buf, uint16_t bufsiz)
{
    DebugInfo2("(drv = %u, dd->dirID = %u)", drv, dd->dirID);
    buf[0] = '\0';      // in case of error...
    CHK_DRIVE(drv)

    if (bufsiz < 64)
    {
        return EPTHOV;
    }

    // first get host path ...
    char pathbuf[1024];
    INT32 aret = xfs_DD2hostPath(dd, pathbuf, 1023);
    if (aret < 0)
    {
        DebugError2("() -> %d", aret);
        return aret;
    }

    // ... then convert host path to Atari path
    const char *atari_root = drv_host_path[drv];
    int rootlen = strlen(atari_root);
    int pathlen = strlen(pathbuf);
    // root must be prefix of our path
    if ((pathlen < rootlen) || strncmp(atari_root, pathbuf, rootlen))
    {
        DebugError2("() : cannot convert host path %s to Atari", pathbuf);
        return EINTRN;
    }
    const char *atari_path = pathbuf + rootlen;
    if (atari_path[0] == '/')
    {
        atari_path++;
    }
    if (strlen(atari_path) + 1 > bufsiz)
    {
        DebugWarning2("() : buffer too small");
        return EPTHOV;
    }

    char *p = buf;
    /* it seems that we may not precede the drive here
    *p++ = 'A' + drv;
    *p++ = ':';
    *p++ = '\\';
    bufsiz -= 3;
    */
    *p++ = '\\';
    bufsiz -= 1;
    hostFnameToAtariFname(atari_path, (unsigned char *) p, bufsiz);
    DebugInfo2("() -> \"%s\"", buf);

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Dopendir(), open a directory for reading, with long name support
 *
 * @param[out] dirh      directory handle, to be used by Dreaddir() etc.
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  dd        Atari directory descriptor
 * @param[in]  tosflag   0: long names with 4 bytes inode, >0: filenames in 8+3 and without inode
 *
 * @return E_OK or negative error code
 *
 * @note Symbolic links already have been resolved in path2DD (hopefully...)
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dopendir
(
    HOST_DIRHANDLE *dirh,
    uint16_t drv,
    MXFSDD *dd,
    uint16_t tosflag
)
{
    DebugInfo2("(drv = %u, tosflag = %d)", drv, tosflag);
    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    off_t lret = lseek(dir_fd, 0, SEEK_SET);    // necessary if directory has been scanned before?
    if (lret < 0)
    {
        DebugWarning2("() : lseek() -> %s", strerror(errno));
    }
    DebugInfo2("() - open directory from host dir_fd %d", dir_fd);
    int dup_dir_fd = dup(dir_fd);
    DIR *dir = fdopendir(dup_dir_fd);
    if (dir == nullptr)
    {
        DebugWarning2("() : fdopendir() -> %s", strerror(errno));
        close(dup_dir_fd);
        return CConversion::host2AtariError(errno);
    }

    dirh->hostDirHdl = (uint16_t) HostHandles::snextSet(dir, dup_dir_fd, getActPd());
    dirh ->tosflag = tosflag;

    DebugInfo2("() -> E_OK");

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For D(x)readdir(), read a directory, with long name support
 *
 * @param[in]  dirh      directory handle from Dopendir()
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  bufsiz    buffer size for name and, if requested, i-node
 * @param[out] buf       buffer for name and, if requested, i-node
 * @param[out] xattr     file information for Dxreaddir(), is nullptr for Dreaddir()
 * @param[out] xr        error code from Fxattr()
 *
 * @return E_OK or negative error code
 *
 * @note If the directory had been opened in 8+3 mode, long filenames are ignored,
 *       and no preceding 4-bytes inode will be stored to buffer.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dreaddir
(
    HOST_DIRHANDLE *dirh,
    uint16_t drv,
    uint16_t bufsiz,
    unsigned char *buf,
    XATTR *xattr,
    INT32 *xr
)
{
    DebugInfo2("(drv = %u, bufsiz = %u)", drv);
    CHK_DRIVE(drv)

    if ((dirh == nullptr) || (dirh->hostDirHdl == 0xffff))
    {
        DebugWarning2("() -> EIHNDL");
        return EIHNDL;
    }

    // AAAAAAAA.BBB occupies exactly 13 bytes, including "." and end-of-string
    if ((dirh->tosflag) && (bufsiz < 13))
    {
        DebugWarning2("() : name buffer is too small for 8+3, ignore all entries");
        return ATARIERR_ERANGE;
    }

    DIR *dir;
    uint16_t snextHdl = dirh->hostDirHdl;
    int dir_fd;
    if (HostHandles::snextGet(snextHdl, &dir, &dir_fd))
    {
        DebugWarning2("() -> EINTRN");
        return EINTRN;
    }

    INT32 atari_err = E_OK;
    INT32 atari_stat_err = E_OK;

    for (;;)
    {
        errno = 0;  // strange, but following advice in man page
        struct dirent *entry = readdir(dir);
        if (entry == nullptr)
        {
            if (errno != 0)
            {
                DebugWarning2("() : readdir() -> %s", strerror(errno));
            }
            atari_err = ENMFIL;
            break;  // end of directory
        }

        if (dirh->tosflag)
        {
            if (hostFnameToAtariFname8p3(entry->d_name, (unsigned char *) buf, drv_caseInsens[drv]))
            {
                DebugWarning2("() : filename \"%s\" does not fit to 8+3 scheme", entry->d_name);
                continue;   // filename was shortened, so it was too long for 8+3
            }
        }

        if (xattr != nullptr)
        {
            struct stat statbuf;
            int ret = fstatat(dir_fd, entry->d_name, &statbuf, AT_SYMLINK_NOFOLLOW);
            if (ret >= 0)
            {
                statbuf2xattr(xattr, &statbuf);
            }
            else
            {
                DebugWarning2("() : fstat(\"%s\") -> %s", entry->d_name, strerror(errno));
                atari_stat_err = CConversion::host2AtariError(errno);
            }
        }

        DebugInfo2("() - found \"%s\"", entry->d_name);

        if (dirh->tosflag == 0)
        {
            // buf needs space for 4 bytes i-node plus filename plus NUL byte
            uint32_t a_ino = htobe32((uint32_t) entry->d_ino);  // there is no suitable reduction from 64-bit to 32-bit
            memcpy(buf, &a_ino, 4);
            buf += 4;
            bufsiz -= 4;
        }
        if (hostFnameToAtariFname(entry->d_name, buf, bufsiz))
        {
            // overflow
            DebugError2("() : buffer size %u too small => ERANGE", bufsiz);
            atari_err = ATARIERR_ERANGE;
        }

        break;
    }

    if (xr != nullptr)
    {
        *xr = htobe32(atari_stat_err);
    }
    return atari_err;
}


/** **********************************************************************************************
 *
 * @brief For Drewinddir(), set read pointer back to the beginning of the directory
 *
 * @param[in]  dirh      directory handle from Dopendir()
 * @param[in]  drv       Atari drive number 0..31
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_drewinddir(HOST_DIRHANDLE *dirh, uint16_t drv)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE(drv)

    if ((dirh == nullptr) || (dirh->hostDirHdl == 0xffff))
    {
        DebugWarning2("() -> EIHNDL");
        return EIHNDL;
    }

    DIR *dir;
    uint16_t snextHdl = dirh->hostDirHdl;
    int dir_fd;
    if (HostHandles::snextGet(snextHdl, &dir, &dir_fd))
    {
        DebugWarning2("s() -> EINTRN");
        dirh->hostDirHdl = -1;
        return EINTRN;
    }

    rewinddir(dir);

    DebugInfo2("() -> E_OK");

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Dclosedir(), close the directory handle
 *
 * @param[in]  dirh      directory handle from Dopendir()
 * @param[in]  drv       Atari drive number 0..31
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dclosedir(HOST_DIRHANDLE *dirh, uint16_t drv)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE(drv)

    if ((dirh == nullptr) || (dirh->hostDirHdl == 0xffff))
    {
        DebugWarning2("() -> EIHNDL");
        return EIHNDL;
    }

    DIR *dir;
    uint16_t snextHdl = dirh->hostDirHdl;
    int dir_fd;
    if (HostHandles::snextGet(snextHdl, &dir, &dir_fd))
    {
        DebugWarning2("() -> EINTRN");
        dirh->hostDirHdl = -1;
        return EINTRN;
    }

    HostHandles::snextClose(snextHdl);  // also does closedir()
    dirh->hostDirHdl = -1;

    DebugInfo2("() -> E_OK");
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Dpathconf(), get information about the path
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  dd        Atari directory descriptor
 * @param[in]  which     sub-function code
 *
 * @return E_OK or negative error code
 *
 * @note which =
 *        -1:   max. legal value for n in Dpathconf(n)
 *         0:   internal limit on the number of open files
 *         1:   max. number of links to a file
 *         2:   max. length of a full path name
 *         3:   max. length of an individual file name
 *         4:   number of bytes that can be written atomically
 *         5:   information about file name truncation
 *              0 = File names are never truncated; if the file name in any system call affecting
 *                  this directory exceeds the maximum length (returned by mode 3), then the
 *                  error value ERANGE is returned from that system call.
 *
 *              1 = File names are automatically truncated to the maximum length.
 *
 *              2 = File names are truncated according to DOS rules, i.e. to a
 *                  maximum 8 character base name and a maximum 3 character extension.
 *         6:   0 = case sensitive
 *              1 = not case sensitive, always uppercase
 *              2 = not case sensitive, uppercase and lowercase
 *         7:   Information about supported attributes and modes
 *         8:   information about valid fields in in XATTR
 *
 *      If any  of these items are unlimited, then 0x7fffffffL is
 *      returned.
 *
 * @note Symbolic links already have been resolved in path2DD (hopefully...)
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dpathconf(uint16_t drv, MXFSDD *dd, uint16_t which)
{
    DebugInfo2("(drv = %u, which = %u)", drv, which);

    (void) dd;
    switch(which)
    {
        case DP_MAXREQ:      return DP_XATTRFIELDS ;
        case DP_IOPEN:       return 100;    // ???
        case DP_MAXLINKS:    return 1;
        case DP_PATHMAX:     return 128;
        case DP_NAMEMAX:     return (drv_longNames[drv]) ? 31 : 12;
        case DP_ATOMIC:      return 512;    // ???
        case DP_TRUNC:       return (drv_longNames[drv]) ? DP_AUTOTRUNC : DP_DOSTRUNC;
        case DP_CASE:        return (drv_longNames[drv]) ? ((drv_caseInsens[drv]) ? DP_CASEINSENS : DP_CASESENS) : DP_CASECONV;
        case DP_MODEATTR:    return F_RDONLY + F_SUBDIR + F_ARCHIVE + F_HIDDEN +
                                    DP_FT_DIR + DP_FT_REG + DP_FT_LNK;
        case DP_XATTRFIELDS: return DP_INDEX + DP_DEV + DP_NLINK + DP_BLKSIZE +
                                    DP_SIZE + DP_NBLOCKS + DP_CTIME + DP_MTIME;
    }
    return EINVFN;
}


/** **********************************************************************************************
 *
 * @brief For Dfree(), get volume usage information
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  dirID     host directory id
 * @param[out] data      free,total,secsiz,clsiz
 *
 * @return E_OK or negative error code
 *
 * @note Symbolic links already have been resolved in path2DD (hopefully...)
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dfree(uint16_t drv, INT32 dirID, UINT32 data[4])
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE(drv)

    (void) dirID;
    (void) data;

    // TODO: this is dummy so far
    // 1G free from 1.5G
    data[0] = htobe32(2 * 1024 * 1024);   // # free blocks
    data[1] = htobe32(3 * 1024 * 1024);   // # total blocks
    data[2] = htobe32(512); // sector size in bytes
    data[3] = htobe32(1);   // sectors per cluster

    DebugInfo2("() -> E_OK");
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Dwritelabel() - write volume name
 *
 * @param[in]  drv          Atari drive number 0..25
 * @param[in]  dd           directory on this drive, might be ignored
 *
 * @return E_OK or 32-bit negative error code
 *
 * @note This does not make much sense for the Host XFS, so we do not support it.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_wlabel(uint16_t drv, MXFSDD *dd, const unsigned char *name)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    (void) dd;
    (void) name;

    DebugInfo2("() -> EACCDN");
    return EACCDN;
}


/** **********************************************************************************************
 *
 * @brief For Dreadlabel(), get volume name
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  dd        Atari directory descriptor, usually for "X:\"
 * @param[out] name      buffer for name
 * @param[in]  bufsiz    size of name buffer, including zero byte
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_rlabel(uint16_t drv, MXFSDD *dd, unsigned char *name, uint16_t bufsiz)
{
    DebugInfo2("(drv = %u, bufsize = %u)", drv, bufsiz);
    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    const char *atari_name = drv_atari_name[drv];
    if (atari_name != nullptr)
    {
        if (bufsiz < strlen(atari_name) + 1)
        {
            return ATARIERR_ERANGE;
        }
        strcpy((char *) name, atari_name);
    }
    else
    {
        if (bufsiz < 12)
        {
            return ATARIERR_ERANGE;
        }
        sprintf((char *) name, "HOSTXFS.%u", drv);
    }

    DebugInfo2("() -> \"%s\"", name);

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Fsymlink(), create a symbolic link with name "to" that links to "name" in "dd"
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  dd        Atari directory descriptor where the new symlink shall be located
 * @param[in]  name      name for a symlink file to be created
 * @param[out] target    file or folder, hopefully existing, but does not have to
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_symlink(uint16_t drv, MXFSDD *dd, const unsigned char *name, const unsigned char *target)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE_WRITEABLE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    CONV8p3(drv, name, host_name)

    // convert Atari path to host path
    char host_target[1024];
    if (atariPath2HostPath((const unsigned char *) target, drv, host_target, 1024) >= 0)
    {
        target = (const unsigned char *) host_target;
    }
    else
    {
        DebugError2("() : cannot convert Atari path \"%s\" to host path", target);
    }

    if (symlinkat(host_target, dir_fd, host_name))
    {
        DebugError2("() : symlinkat(\"%s\", \"%s\") -> %s", host_name, target, strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Freadlink(), get the destination a symbolic link is pointing to
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  dd        Atari directory descriptor
 * @param[in]  name      name of existing symbolic link
 * @param[out] buf       read buffer
 * @param[in]  bufsiz    size of read buffer
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_readlink
(
    uint16_t drv,
    MXFSDD *dd,
    const unsigned char *name,
    unsigned char *buf,
    uint16_t bufsiz
)
{
    DebugInfo2("(drv = %u, name = \"%s\")", drv, name);
    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)

    CONV8p3(drv, name, host_name)

    char host_target[1024];
    int nbytes = readlinkat(dir_fd, host_name, host_target, sizeof(host_target) - 1);
    if (nbytes < 0)
    {
        DebugError2("() : readlinkat(\"%s\") -> %s", host_name, strerror(errno));
        return CConversion::host2AtariError(errno);
    }
    host_target[nbytes] = '\0';
    if (nbytes == sizeof(host_target) - 1)
    {
        DebugWarning2("() : The symlink target probably was truncated: \"%s\"", host_target);
    }

    // convert host path to Atari path
    if (hostPath2AtariPath(host_target, drv, (unsigned char *) buf, bufsiz) < 0)
    {
        DebugError2("() : cannot convert host path \"%s\" to Atari path", host_target);
        if (bufsiz < strlen(host_target) + 1)
        {
            return ATARIERR_ERANGE;  // buffer too small
        }
        strcpy((char *) buf, host_target);   // just copy host path
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For Dcntl(), various operations on files or folders referenced by path
 *
 * @param[in]  drv              Atari drive number 0..31
 * @param[in]  dd               Atari directory descriptor
 * @param[in]  name             name of existing file or folder or symbolic link
 * @param[in]  cmd              sub-command
 * @param[in]  pArg             command specific parameters
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return E_OK or negative error code
 *
 * @note Supports FUTIME and FSTAT
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dcntl
(
    uint16_t drv,
    MXFSDD *dd,
    const unsigned char *name,
    uint16_t cmd,
    void *pArg,
    uint8_t *addrOffset68k
)
{
    DebugInfo2("(drv = %u)", drv);
    CHK_DRIVE(drv)
    GET_hhdl_hostFD_dir_fd(dd, hhdl, hostFD, dir_fd)
    CONV8p3(drv, name, host_name)

    (void) addrOffset68k;

    struct stat statbuf;
    if ((cmd == FUTIME) || (cmd == FSTAT))
    {
		if (!pArg)
        {
		    return EINVFN;
        }
        int res = fstatat(dir_fd, host_name, &statbuf, AT_EMPTY_PATH);
        if (res < 0)
        {
            DebugWarning2("() : fstatat(\"%s\") -> %s", host_name, strerror(errno));
            return CConversion::host2AtariError(errno);
        }
    }

    switch(cmd)
    {
        case FUTIME:
        {
            struct XATTR temp;
            statbuf2xattr(&temp, &statbuf);
            struct mutimbuf *mutim = (struct mutimbuf *) pArg;
            mutim->acdate = temp.adate;
            mutim->actime = temp.atime;
            mutim->moddate = temp.mdate;
            mutim->modtime = temp.mtime;
            return E_OK;
        }

        case FSTAT:
            statbuf2xattr((XATTR *) pArg, &statbuf);
            return E_OK;

        case DCNTL_VFAT_CNFFLN:
            return EINVFN;      // only relevant for (V)FAT

        case DCNTL_MX_KER_DRVSTAT:
        {
            INT16_BE *args = (INT16_BE *) pArg;
            uint16_t arg0 = be16toh(args[0]);
            uint16_t arg1 = be16toh(args[1]);
            if (arg0 == 0)
            {
                return ((arg1 < NDRIVES) && drv_host_path[arg1] != nullptr) ? E_OK : EDRIVE;
            }
            else
            {
                return EINVFN;
            }
            break;
        }

        case DCNTL_MX_KER_XFSNAME:
            strcpy((char *) pArg, "HOST_XFS");
            return E_OK;

        default:
            DebugWarning2("() : unsupported command 0x%04x for \"%s\"", cmd, name);
            return EINVFN;
    }

    return EINVFN;
}



/*************************************************************/
/******************** File Driver ****************************/
/*************************************************************/


/// Helper to get the host file handle
#define GET_hhdl_AND_fd \
    uint32_t hhdl = be16toh(f->refnum); \
    HostFD *hostFD = getHostFD(hhdl); \
    if (hostFD == nullptr) \
    { \
        return EINTRN; \
    } \
    int fd = hostFD->fd; \
    if (fd < 0) \
    { \
        DebugError("invalid file handle"); \
        return EIHNDL; \
    }


/** **********************************************************************************************
 *
 * @brief decrement reference counter and close an open file, if zero
 *
 * @param[in] f      file descriptor
 *
 * @return E_OK or negative error code
 *
 * @note Decrementing the reference counter was extracted from the MagiC kernel for
 *       historical reasons. MagicMac used to flush the file whenever it was closed.
 *
 * @note Like in MagicMac(X) and AtariX, changing file modification time and date is done
 *       here deferred, not directory on dev_datime(). This function is used by the Atari
 *       when copying files with preserving modification dates. Here, also access date is
 *       updated to modification date.
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_close(MAC_FD *f)
{
    DebugInfo2("(fd = 0x%0x)", f);

    // decrement reference counter
    uint16_t refcnt = be16toh(f->fd.fd_refcnt);
    if (refcnt <= 0)
    {
        DebugError2("() -- invalid file refcnt");
        return EINTRN;
    }
    refcnt--;
    f->fd.fd_refcnt = htobe16(refcnt);

    INT32 aret = E_OK;

    if (refcnt == 0)
    {
        GET_hhdl_AND_fd

        // change date and time, if modified by dev_datime()
        if (f->mod_tdate_dirty)
        {
            // get host path from file descriptor
            char pathbuf[1024];
            aret = hostFd2Path(fd, pathbuf, sizeof(pathbuf));
            if (aret == E_OK)
            {
                struct utimbuf utim;
                CConversion::dosDateToHostDate(f->mod_time, f->mod_date, &utim.actime);
                CConversion::dosDateToHostDate(f->mod_time, f->mod_date, &utim.modtime);
                if (utime(pathbuf, &utim))
                {
                    DebugWarning2("() : utime(\"%s\") -> %s", pathbuf, strerror(errno));
                    aret = CConversion::host2AtariError(errno);
                }
            }
            f->mod_tdate_dirty = 0;
        }
        freeHostFD(hostFD);     // also closes hostFD->fd
    }

    return aret;
}


/** **********************************************************************************************
 *
 * @brief read from an open file
 *
 * @param[in]  f      file descriptor
 * @param[in]  count  number of bytes to read
 * @param[out] buf    destination buffer
 *
 * @return bytes read or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_read(MAC_FD *f, int32_t count, char *buf)
{
    GET_hhdl_AND_fd
    DebugInfo2("(fd = 0x%0x, count = %d) - host fd = %d", f, count, fd);

    ssize_t bytes = read(fd, buf, count);
    if (bytes < 0)
    {
        DebugWarning2("() : read() -> %s", strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    if (bytes > 0x7fffffff)
    {
        DebugError("file too large");
        return ATARIERR_ERANGE;
    }

    DebugInfo2("() => %d", (int32_t) bytes);
    return (int32_t) bytes;
}


/** **********************************************************************************************
 *
 * @brief write to an open file
 *
 * @param[in]  f      file descriptor
 * @param[in]  count  number of bytes to write
 * @param[out] buf    source buffer
 *
 * @return bytes read or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_write(MAC_FD *f, INT32 count, const char *buf)
{
    GET_hhdl_AND_fd
    DebugInfo2("(fd = 0x%0x, count = %d) - host fd = %d", f, count, fd);

    ssize_t bytes = write(fd, buf, count);
    if (bytes < 0)
    {
        DebugWarning2("() : write() -> %s", strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    if (bytes > 0x7fffffff)
    {
        DebugError("file too large");
        return ATARIERR_ERANGE;
    }

    DebugInfo2("() => %d", (int32_t) bytes);
    return (int32_t) bytes;
}


/** **********************************************************************************************
 *
 * @brief For Finstat() and Foutstat() - get read or write status of an open file
 *
 * @param[in]  f       file descriptor
 * @param[in]  unsel   (remnants of original MagiCMac's asynchronous I/O)
 * @param[in]  rwflag  0: get read status, 1: get write status
 * @param[in]  apcode  (remnants of original MagiCMac's asynchronous I/O)
 *
 * @return 1 or 0 or negative error code
 * @retval 1  file can be written to respectively read from
 * @retval 0  end-of-file for reading, or file is full or write proteced
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_stat(MAC_FD *f, void *unsel, uint16_t rwflag, INT32 apcode)
{
    GET_hhdl_AND_fd
    DebugInfo2("(fd = 0x%0x, rwflag = %d) - host fd = %d", f, rwflag, fd);
    (void) unsel;
    (void) apcode;

    if (rwflag)
    {
        // TODO: check if file was opened for reading only!
        return 1;   // we can always write
    }
    else
    {
        off_t curr_pos = lseek(fd, 0, SEEK_CUR);    // get current position
        if (curr_pos < 0)
        {
            DebugError2("() : lseek() -> %s", strerror(errno));
            return CConversion::host2AtariError(errno);
        }
        off_t len = lseek(fd, 0, SEEK_END);    // move to end
        if (len < 0)
        {
            DebugError2("() : lseek() -> %s", strerror(errno));
            return CConversion::host2AtariError(errno);
        }
        (void) lseek(fd, curr_pos, SEEK_SET);    // move to previous position
        return (curr_pos < len) ? 1 : 0;
    }
}


/** **********************************************************************************************
 *
 * @brief Set read and write position of an open file
 *
 * @param[in] f     file descriptor
 * @param[in] pos   position
 * @param[in] mode  seek mode (0=SET/1=CUR/2=END)
 *
 * @return absolute file position or negative error code
 *
 * @note The seek mode is the same in Atari and Unix.
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_seek(MAC_FD *f, int32_t pos, uint16_t mode)
{
    GET_hhdl_AND_fd
    DebugInfo2("(fd = 0x%0x, pos = %d) - host fd = %d", f, pos, fd);

    if (mode > 3)
    {
        DebugError2("() - invalid seek mode");
        return EINVFN;
    }

    off_t offs = lseek(fd, pos, mode);
    if (offs < 0)
    {
        DebugWarning2("() : lseek() -> %s", strerror(errno));
        return CConversion::host2AtariError(errno);
    }

    if (offs > 0x7fffffff)
    {
        DebugError2("() - file too large");
        return ATARIERR_ERANGE;
    }

    DebugInfo2("() => %d", (int32_t) offs);
    return (int32_t) offs;
}


/** **********************************************************************************************
 *
 * @brief For Fdatime() - get or set timestamps of an open file
 *
 * @param[in] f       file descriptor
 * @param[in] d       date and time
 * @param[in] rwflag  1: change, 0: get
 *
 * @return E_OK or negative error code
 *
 * @note Like in MagicMac(X) and AtariX, we cannot change the timestamps here, but store
 *       them in the file descriptor, so that they can be applied after the file has
 *       been closed.
 * @note We only allow timestamp changes if the file was opend with write permission,
 *       so we can block changes on write-protected drives.
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_datime(MAC_FD *f, UINT16 d[2], uint16_t rwflag)
{
    DebugInfo2("(fd = 0x%0x, rwflag = %d)", f, rwflag);

    if (rwflag)
    {
        if (f->fd.fd_mode & OM_WPERM)
        {
            // remember for later, when we close the file
            f->mod_time = be16toh(d[0]);
            f->mod_date = be16toh(d[1]);
            f->mod_tdate_dirty = 1;
        }
        else
        {
            DebugError2("() - attempt to change date/time on a read-only opened file");
            return EACCDN;
        }
    }
    else
    if (f->mod_tdate_dirty)
    {
        // already changed
        d[0] = htobe16(f->mod_time);
        d[1] = htobe16(f->mod_date);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Various operations on open files
 *
 * @param[in]  f     file descriptor
 * @param[in]  cmd   sub-command
 * @param[out] buf   depending on command
 *
 * @return absolute file position or negative error code
 *
 * @note The seek mode is the same in Atari and Unix.
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_ioctl(MAC_FD *f, uint16_t cmd, void *buf)
{
    DebugInfo2("(fd = 0x%0x, cmd = %d)", f, cmd);

    GET_hhdl_AND_fd

    switch(cmd)
    {
        case FSTAT:
        {
            if (buf == nullptr)
            {
                return EINVFN;
            }
            struct stat statbuf;
            int res = fstat(fd, &statbuf);
            if (res < 0)
            {
                DebugWarning2("() : fstat() -> %s", strerror(errno));
            }
            XATTR *pxattr = (XATTR *) buf;
            statbuf2xattr(pxattr, &statbuf);
            return E_OK;
            break;
        }

        case FUTIME:
        {
            if (buf == nullptr)
            {
                return EINVFN;
            }
            struct stat statbuf;
            int res = fstat(fd, &statbuf);
            if (res < 0)
            {
                DebugWarning2("() : fstat() -> %s", strerror(errno));
            }
            struct XATTR temp;
            statbuf2xattr(&temp, &statbuf);
            struct mutimbuf *mutim = (struct mutimbuf *) buf;
            mutim->acdate = temp.adate;
            mutim->actime = temp.atime;
            mutim->moddate = temp.mdate;
            mutim->modtime = temp.mtime;
            return E_OK;
        }

        case FTRUNCATE:
        {
            uint32_t newsize = be32toh(*((int32_t *) buf));
            int res = ftruncate(fd, newsize);
            if (res < 0)
            {
                DebugWarning2("() : ftruncate() -> %s", strerror(errno));
                return CConversion::host2AtariError(errno);
            }
        }

        // macOS specific commands are not supported
        case FMACOPENRES:
        case FMACGETTYCR:
        case FMACSETTYCR:
        case FMACMAGICEX:
            break;

        default:
            DebugWarning2("() : unsupported command 0x%04x", cmd);
            return EINVFN;
    }

    return EINVFN;
}


/** **********************************************************************************************
 *
 * @brief Read character from open file
 *
 * @param[in]  f     file descriptor
 * @param[in]  mode  bit 0: cooked, bit 1: echo
 *
 * @return character read or negative error code
 * @retval 0xff1a  end-of-file
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_getc(MAC_FD *f, uint16_t mode)
{
    DebugInfo2("(fd = 0x%0x, mode = %d)", f, mode);
    (void) mode;    // no cooking, no echo
    unsigned char c;
    INT32 ret;

    ret = dev_read(f, 1L, (char *) &c);
    if (ret < 0L)
    {
        // an error occurred
        return ret;
    }

    if (!ret)
    {
        // nothing read, return EOF
        return(0x0000ff1a);
    }

    // return the character read
    return c & 0x000000ff;
}


/** **********************************************************************************************
 *
 * @brief Read a line until LF from an open file
 *
 * @param[in]  f     file descriptor
 * @param[in]  buf   read buffer
 * @param[in]  size  size of read buffer
 * @param[in]  mode  bit 0: cooked, bit 1: echo
 *
 * @return number or characters read or negative error code
 *
 * @note LF is not stored to the buffer, and CR is ignored. The buffer is not zero terminated.
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_getline(MAC_FD *f, char *buf, INT32 size, uint16_t mode)
{
    DebugInfo2("(fd = 0x%0x, size = %d)", f, size);
    (void) mode;    // no cooking, no echo
    char c;
    INT32 processed, ret;

    for (processed = 0; processed < size;)
    {
        ret = dev_read(f, 1L, (char *) &c);
        if (ret < 0L)
        {
            // error
            return ret;
        }
        if (ret == 0L)
        {
            // end-of-file, stop here
            break;            // EOF
        }
        if (c == 0x0d)
        {
            // ignore CR
            continue;
        }
        if (c == 0x0a)
        {
            // LF, stop here
            break;
        }

        processed++;
        *buf++ = c;
    }

    return processed;
}


/** **********************************************************************************************
 *
 * @brief Write character to an open file
 *
 * @param[in]  f     file descriptor
 * @param[in]  mode  bit 0: cooked, bit 1: echo
 * @param[in]  val   character to write, only eight bits used
 *
 * @return 1 or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::dev_putc(MAC_FD *f, uint16_t mode, INT32 val)
{
    DebugInfo2("(fd = 0x%0x, mode = %d)", f, mode);
    (void) mode;    // no cooking, no echo
    char c;

    c = (char) val;
    return dev_write(f, 1L, (char *) & c);
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Dispatcher for file system driver
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return E_OK or negative error code
 *
 * @note There is no endian conversion of the return value, because this is already
 *       done inside the 68k emulator.
 *
 ************************************************************************************************/
INT32 CHostXFS::XFSFunctions(UINT32 param, uint8_t *addrOffset68k)
{
    UINT16 fncode;
    INT32 doserr;
    unsigned char *params = addrOffset68k + param;

    DebugInfo2("(param = %u)", param);

    fncode = getAtariBE16(params);
#ifdef DEBUG_VERBOSE
    DebugInfo("CHostXFS::XFSFunctions(%d)", (int) fncode);
    if (fncode == 7)
    {
/*
        if (!m68k_trace_trigger)
        {
            // ab dem ersten fopen() tracen
            m68k_trace_trigger = 1;
            _DumpAtariMem("AtariMemOnFirstXfsCall.data");
        }
*/
    }
#endif
    params += 2;
    switch(fncode)
    {
        case 0:
        {
            struct syncparm
            {
                UINT16 drv;
            } __attribute__((packed));
            syncparm *psyncparm = (syncparm *) params;
            doserr = xfs_sync(be16toh(psyncparm->drv));
            break;
        }

        case 1:
        {
            struct ptermparm
            {
                UINT32 pd;        // PD *
            } __attribute__((packed));
            ptermparm *pptermparm = (ptermparm *) params;
            xfs_pterm(be32toh(pptermparm->pd), addrOffset68k);
            doserr = E_OK;
            break;
        }

        case 2:
        {
            struct drv_openparm
            {
                UINT16 drv;
                UINT32 dd;        // MXFSDD *
                INT32 flg_ask_diskchange;    // in fact: DMD->D_XFS (68k-Pointer or NULL)
            } __attribute__((packed));
            drv_openparm *pdrv_openparm = (drv_openparm *) params;
            doserr = xfs_drv_open(
                    be16toh(pdrv_openparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pdrv_openparm->dd)),
                    be32toh(pdrv_openparm->flg_ask_diskchange));
            break;
        }

        case 3:
        {
            struct drv_closeparm
            {
                UINT16 drv;
                UINT16 mode;
            } __attribute__((packed));
            drv_closeparm *pdrv_closeparm = (drv_closeparm *) params;
            doserr = xfs_drv_close(be16toh(pdrv_closeparm->drv),
                                        be16toh(pdrv_closeparm->mode));
            break;
        }

        case 4:
        {
            struct path2DDparm
            {
                UINT16 mode;
                UINT16 drv;
                UINT32 rel_dd;    // MXFSDD *
                UINT32 pathname;        // char *
                UINT32 remain_path;        // char **
                UINT32 symlink_dd;    // MXFSDD *
                UINT32 symlink;        // char **
                UINT32 dd;        // MXFSDD *
                UINT32 dir_drive;
            } __attribute__((packed));
            const char *remain_path;
            const char *symlink;

            path2DDparm *ppath2DDparm = (path2DDparm *) params;
#ifdef DEBUG_VERBOSE
            __dump((const unsigned char *) ppath2DDparm, sizeof(*ppath2DDparm));
#endif
            doserr = xfs_path2DD(
                    be16toh(ppath2DDparm->mode),
                    be16toh(ppath2DDparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(ppath2DDparm->rel_dd)),
                    (char *) (addrOffset68k + be32toh(ppath2DDparm->pathname)),
                    &remain_path,
                    (MXFSDD *) (addrOffset68k + be32toh(ppath2DDparm->symlink_dd)),
                    &symlink,
                    (MXFSDD *) (addrOffset68k + be32toh(ppath2DDparm->dd)),
                    (UINT16 *) (addrOffset68k + be32toh(ppath2DDparm->dir_drive))
                    );

            // calculate Atari address from host address
            uint32_t emuAddr = remain_path - (char *) addrOffset68k;
            // store to result
            setAtariBE32(addrOffset68k + be32toh(ppath2DDparm->remain_path), emuAddr);
            emuAddr = symlink - (char *) addrOffset68k;
            setAtariBE32(addrOffset68k + be32toh(ppath2DDparm->symlink), emuAddr);
#ifdef DEBUG_VERBOSE
            __dump((const unsigned char *) ppath2DDparm, sizeof(*ppath2DDparm));
            if (doserr >= 0)
                DebugInfo(" restpath = „%s“", remain_path);
#endif
            break;
        }

        case 5:
        {
            struct sfirstparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT32 dta;        // MAC_DTA *
                UINT16 attrib;
            } __attribute__((packed));
            sfirstparm *psfirstparm = (sfirstparm *) params;
            doserr = xfs_sfirst(
                    be16toh(psfirstparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(psfirstparm->dd)),
                    (char *) (addrOffset68k + be32toh(psfirstparm->name)),
                    (MAC_DTA *) (addrOffset68k + be32toh(psfirstparm->dta)),
                    be16toh(psfirstparm->attrib)
                    );
            break;
        }

        case 6:
        {
            struct snextparm
            {
                UINT16 drv;
                UINT32 dta;        // MAC_DTA *
            } __attribute__((packed));
            snextparm *psnextparm = (snextparm *) params;
            doserr = xfs_snext(
                    be16toh(psnextparm->drv),
                    (MAC_DTA *) (addrOffset68k + be32toh(psnextparm->dta))
                    );
            break;
        }

        case 7:
        {
            struct fopenparm
            {
                UINT32 name;    // char *
                UINT16 drv;
                UINT32 dd;        //MXFSDD *
                UINT16 omode;
                UINT16 attrib;
            } __attribute__((packed));
            fopenparm *pfopenparm = (fopenparm *) params;
            doserr = xfs_fopen(
                    (unsigned char *) (addrOffset68k + be32toh(pfopenparm->name)),
                    be16toh(pfopenparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pfopenparm->dd)),
                    be16toh(pfopenparm->omode),
                    be16toh(pfopenparm->attrib)
                    );
            break;
        }

        case 8:
        {
            struct fdeleteparm
            {
                UINT16 drv;
                UINT32 dd;        //MXFSDD *
                UINT32 name;    // char *
            } __attribute__((packed));
            fdeleteparm *pfdeleteparm = (fdeleteparm *) params;
            doserr = xfs_fdelete(
                    be16toh(pfdeleteparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pfdeleteparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pfdeleteparm->name))
                    );
            break;
        }

        case 9:
        {
            struct flinkparm
            {
                UINT16 drv;
                UINT32 nam1;    // char *
                UINT32 nam2;    // char *
                UINT32 dd1;        // MXFSDD *
                UINT32 dd2;        // MXFSDD *
                UINT16 mode;
                UINT16 dst_drv;
            } __attribute__((packed));
            flinkparm *pflinkparm = (flinkparm *) params;
            doserr = xfs_link(
                    be16toh(pflinkparm->drv),
                    (unsigned char *) (addrOffset68k + be32toh(pflinkparm->nam1)),
                    (unsigned char *) (addrOffset68k + be32toh(pflinkparm->nam2)),
                    (MXFSDD *) (addrOffset68k + be32toh(pflinkparm->dd1)),
                    (MXFSDD *) (addrOffset68k + be32toh(pflinkparm->dd2)),
                    be16toh(pflinkparm->mode),
                    be16toh(pflinkparm->dst_drv)
                    );
            break;
        }

        case 10:
        {
            struct xattrparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT32 xattr;    // XATTR *
                UINT16 mode;
            } __attribute__((packed));
            xattrparm *pxattrparm = (xattrparm *) params;
            doserr = xfs_xattr(
                    be16toh(pxattrparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pxattrparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pxattrparm->name)),
                    (XATTR *) (addrOffset68k + be32toh(pxattrparm->xattr)),
                    be16toh(pxattrparm->mode)
                    );
            break;
        }

        case 11:
        {
            struct attribparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT16 rwflag;
                UINT16 attr;
            } __attribute__((packed));
            attribparm *pattribparm = (attribparm *) params;
            doserr = xfs_attrib(
                    be16toh(pattribparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pattribparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pattribparm->name)),
                    be16toh(pattribparm->rwflag),
                    be16toh(pattribparm->attr)
                    );
            break;
        }

        case 12:
        {
            struct chownparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT16 uid;
                UINT16 gid;
            } __attribute__((packed));
            chownparm *pchownparm = (chownparm *) params;
            doserr = xfs_fchown(
                    be16toh(pchownparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pchownparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pchownparm->name)),
                    be16toh(pchownparm->uid),
                    be16toh(pchownparm->gid)
                    );
            break;
        }

        case 13:
        {
            struct chmodparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT16 fmode;
            } __attribute__((packed));
            chmodparm *pchmodparm = (chmodparm *) params;
            doserr = xfs_fchmod(
                    be16toh(pchmodparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pchmodparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pchmodparm->name)),
                    be16toh(pchmodparm->fmode)
                    );
            break;
        }

        case 14:
        {
            struct dcreateparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
            } __attribute__((packed));
            dcreateparm *pdcreateparm = (dcreateparm *) params;
            if (be32toh((UINT32) (pdcreateparm->name)) >= mem68kSize)
            {
                DebugError("CHostXFS::xfs_dcreate() - invalid name ptr");
                return ERROR;
            }

            doserr = xfs_dcreate(
                    be16toh(pdcreateparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pdcreateparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pdcreateparm->name))
                    );
            break;
        }

        case 15:
        {
            struct ddeleteparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
            } __attribute__((packed));
            ddeleteparm *pddeleteparm = (ddeleteparm *) params;
            doserr = xfs_ddelete(
                    be16toh(pddeleteparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pddeleteparm->dd))
                    );
            break;
        }

        case 16:
        {
            struct dd2nameparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 buf;        // char *
                UINT16 bufsiz;
            } __attribute__((packed));
            dd2nameparm *pdd2nameparm = (dd2nameparm *) params;
            doserr = xfs_DD2name(
                    be16toh(pdd2nameparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pdd2nameparm->dd)),
                    (char *) (addrOffset68k + be32toh(pdd2nameparm->buf)),
                    be16toh(pdd2nameparm->bufsiz)
                    );
            break;
        }

        case 17:
        {
            struct dopendirparm
            {
                UINT32 dirh;        // MAC_DIRHANDLE *
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT16 tosflag;
            } __attribute__((packed));
            dopendirparm *pdopendirparm = (dopendirparm *) params;
            doserr = xfs_dopendir(
                    (HOST_DIRHANDLE *) (addrOffset68k + be32toh(pdopendirparm->dirh)),
                    be16toh(pdopendirparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pdopendirparm->dd)),
                    be16toh(pdopendirparm->tosflag)
                    );
            break;
        }

        case 18:
        {
            struct dreaddirparm
            {
                UINT32 dirh;        // MAC_DIRHANDLE *
                UINT16 drv;
                UINT16 size;
                UINT32 buf;        // char *
                UINT32 xattr;    // XATTR * oder NULL
                UINT32 xr;        // INT32 * oder NULL
            } __attribute__((packed));
            dreaddirparm *pdreaddirparm = (dreaddirparm *) params;
            doserr = xfs_dreaddir(
                    (HOST_DIRHANDLE *) (addrOffset68k + be32toh(pdreaddirparm->dirh)),
                    be16toh(pdreaddirparm->drv),
                    be16toh(pdreaddirparm->size),
                    (unsigned char *) (addrOffset68k + be32toh(pdreaddirparm->buf)),
                    (XATTR *) ((pdreaddirparm->xattr) ? addrOffset68k + be32toh(pdreaddirparm->xattr) : NULL),
                    (INT32 *) ((pdreaddirparm->xr) ? (addrOffset68k + be32toh(pdreaddirparm->xr)) : NULL)
                    );
            break;
        }

        case 19:
        {
            struct drewinddirparm
            {
                UINT32 dirh;        // MAC_DIRHANDLE *
                UINT16 drv;
            } __attribute__((packed));
            drewinddirparm *pdrewinddirparm = (drewinddirparm *) params;
            doserr = xfs_drewinddir(
                    (HOST_DIRHANDLE *) (addrOffset68k + be32toh(pdrewinddirparm->dirh)),
                    be16toh(pdrewinddirparm->drv)
                    );
            break;
        }

        case 20:
        {
            struct dclosedirparm
            {
                UINT32 dirh;        // MAC_DIRHANDLE *
                UINT16 drv;
            } __attribute__((packed));
            dclosedirparm *pdclosedirparm = (dclosedirparm *) params;
            doserr = xfs_dclosedir(
                    (HOST_DIRHANDLE *) (addrOffset68k + be32toh(pdclosedirparm->dirh)),
                    be16toh(pdclosedirparm->drv)
                    );
            break;
        }

        case 21:
        {
            struct dpathconfparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT16 which;
            } __attribute__((packed));
            dpathconfparm *pdpathconfparm = (dpathconfparm *) params;
            doserr = xfs_dpathconf(
                    be16toh(pdpathconfparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pdpathconfparm->dd)),
                    be16toh(pdpathconfparm->which)
                    );
            break;
        }

        case 22:
        {
            struct dfreeparm
            {
                UINT16 drv;
                INT32 dirID;
                UINT32 data;    // UINT32 data[4]
            } __attribute__((packed));
            dfreeparm *pdfreeparm = (dfreeparm *) params;
            doserr = xfs_dfree(
                    be16toh(pdfreeparm->drv),
                    pdfreeparm->dirID,
                    (UINT32 *) (addrOffset68k + be32toh(pdfreeparm->data))
                    );
#ifdef DEBUG_VERBOSE
            __dump((const unsigned char *) (AdrOffset68k + CFSwapInt32BigToHost(pdfreeparm->data)), 16);
#endif
            break;
        }

        case 23:
        {
            struct wlabelparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
            } __attribute__((packed));
            wlabelparm *pwlabelparm = (wlabelparm *) params;
            doserr = xfs_wlabel(
                    be16toh(pwlabelparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pwlabelparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pwlabelparm->name))
                    );
            break;
        }

        case 24:
        {
            struct rlabelparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT16 bufsiz;
            } __attribute__((packed));
            rlabelparm *prlabelparm = (rlabelparm *) params;
            doserr = xfs_rlabel(
                    be16toh(prlabelparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(prlabelparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(prlabelparm->name)),
                    be16toh(prlabelparm->bufsiz)
                    );
            break;
        }

        case 25:
        {
            struct symlinkparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT32 to;        // char *
            } __attribute__((packed));
            symlinkparm *psymlinkparm = (symlinkparm *) params;
            doserr = xfs_symlink(
                    be16toh(psymlinkparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(psymlinkparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(psymlinkparm->name)),
                    (unsigned char *) (addrOffset68k + be32toh(psymlinkparm->to))
                    );
            break;
        }

        case 26:
        {
            struct readlinkparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT32 buf;        // char *
                UINT16 bufsiz;
            } __attribute__((packed));
            readlinkparm *preadlinkparm = (readlinkparm *) params;
            doserr = xfs_readlink(
                    be16toh(preadlinkparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(preadlinkparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(preadlinkparm->name)),
                    (unsigned char *) (addrOffset68k + be32toh(preadlinkparm->buf)),
                    be16toh(preadlinkparm->bufsiz)
                    );
            break;
        }

        case 27:
        {
            struct dcntlparm
            {
                UINT16 drv;
                UINT32 dd;    // MXFSDD *
                UINT32 name;    // char *
                UINT16 cmd;
                INT32 arg;
            } __attribute__((packed));
            dcntlparm *pdcntlparm = (dcntlparm *) params;
            doserr = xfs_dcntl(
                    be16toh(pdcntlparm->drv),
                    (MXFSDD *) (addrOffset68k + be32toh(pdcntlparm->dd)),
                    (unsigned char *) (addrOffset68k + be32toh(pdcntlparm->name)),
                    be16toh(pdcntlparm->cmd),
                    addrOffset68k + be32toh(pdcntlparm->arg),
                    addrOffset68k
                    );
            break;
        }

        case 28:
        {
            struct freeddparm
            {
                UINT32 dd;    // XFS_DD *
            } __attribute__((packed));
            freeddparm *pfreeddparm = (freeddparm *) params;
            xfs_freeDD(
                    (XFS_DD *) (addrOffset68k + be32toh(pfreeddparm->dd)),
                    addrOffset68k
                    );
            doserr = E_OK;
            break;
        }

        default:
            doserr = EINVFN;
            break;
    }

#ifdef DEBUG_VERBOSE
    DebugInfo("CHostXFS::XFSFunctions => %d (= 0x%08x)", (int) doserr, (int) doserr);
#endif
    return doserr;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Dispatcher for file driver
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::XFSDevFunctions(UINT32 param, uint8_t *addrOffset68k)
{
    INT32 doserr;
    unsigned char *params = addrOffset68k + param;
    UINT32 ifd;


    DebugInfo2("(param = %u)", param);

    // first 2 bytes: function code
    uint16_t fncode = getAtariBE16(params + 0);
    params += 2;    // proceed to next parameter
    // next 4 bytes: pointer to MAC_FD
    ifd = *((UINT32 *) params);
    params += 4;
    MAC_FD *f = (MAC_FD *) (addrOffset68k + be32toh(ifd));

#ifdef DEBUG_VERBOSE
    DebugInfo("CHostXFS::XFSDevFunctions(%d)", (int) fncode);
    __dump((const unsigned char *) f, sizeof(*f));
#endif
    switch(fncode)
    {
        case 0:
        {
            doserr = dev_close(f);
            break;
        }

        case 1:
        {
            struct devreadparm
            {
//                UINT32 f;    // MAC_FD *
                INT32 count;
                UINT32 buf;        // char *
            } __attribute__((packed));
            devreadparm *pdevreadparm = (devreadparm *) params;
            doserr = dev_read(
                    f,
                    be32toh(pdevreadparm->count),
                    (char *) (addrOffset68k + be32toh(pdevreadparm->buf))
                    );
            break;
        }

        case 2:
        {
            struct devwriteparm
            {
//                UINT32 f;    // MAC_FD *
                INT32 count;
                UINT32 buf;        // char *
            } __attribute__((packed));
            devwriteparm *pdevwriteparm = (devwriteparm *) params;
            doserr = dev_write(
                    f,
                    be32toh(pdevwriteparm->count),
                    (char *) (addrOffset68k + be32toh(pdevwriteparm->buf))
                    );
            break;
        }

        case 3:
        {
            struct devstatparm
            {
//                UINT32 f;    // MAC_FD *
                UINT32 unsel;    //void *
                UINT16 rwflag;
                INT32 apcode;
            } __attribute__((packed));
            devstatparm *pdevstatparm = (devstatparm *) params;
            doserr = dev_stat(
                    f,
                    (void *) (addrOffset68k + pdevstatparm->unsel),
                    be16toh(pdevstatparm->rwflag),
                    be32toh(pdevstatparm->apcode)
                    );
            break;
        }

        case 4:
        {
            struct devseekparm
            {
//                UINT32 f;    // MAC_FD *
                INT32 pos;
                UINT16 mode;
            } __attribute__((packed));
            devseekparm *pdevseekparm = (devseekparm *) params;
            doserr = dev_seek(
                    f,
                    be32toh(pdevseekparm->pos),
                    be16toh(pdevseekparm->mode)
                    );
            break;
        }

        case 5:
        {
            struct devdatimeparm
            {
//                UINT32 f;    // MAC_FD *
                UINT32 d;        // UINT16[2]
                UINT16 rwflag;
            } __attribute__((packed));
            devdatimeparm *pdevdatimeparm = (devdatimeparm *) params;
            doserr = dev_datime(
                    f,
                    (UINT16 *) (addrOffset68k + be32toh(pdevdatimeparm->d)),
                    be16toh(pdevdatimeparm->rwflag)
                    );
            break;
        }

        case 6:
        {
            struct devioctlparm
            {
//                UINT32 f;    // MAC_FD *
                UINT16 cmd;
                UINT32 buf;        // void *
            } __attribute__((packed));
            devioctlparm *pdevioctlparm = (devioctlparm *) params;
            doserr = dev_ioctl(
                    f,
                    be16toh(pdevioctlparm->cmd),
                    (void *) (addrOffset68k + be32toh(pdevioctlparm->buf))
                    );
            break;
        }

        case 7:
        {
            struct devgetcparm
            {
//                UINT32 f;    // MAC_FD *
                UINT16 mode;
            } __attribute__((packed));
            devgetcparm *pdevgetcparm = (devgetcparm *) params;
            doserr = dev_getc(
                    f,
                    be16toh(pdevgetcparm->mode)
                    );
            break;
        }

        case 8:
        {
            struct devgetlineparm
            {
//                UINT32 f;    // MAC_FD *
                UINT32 buf;        // char *
                INT32 size;
                UINT16 mode;
            } __attribute__((packed));
            devgetlineparm *pdevgetlineparm = (devgetlineparm *) params;
            doserr = dev_getline(
                    f,
                    (char *) (addrOffset68k + be32toh(pdevgetlineparm->buf)),
                    be32toh(pdevgetlineparm->size),
                    be16toh(pdevgetlineparm->mode)
                    );
            break;
        }

        case 9:
        {
            struct devputcparm
            {
//                UINT32 f;    // MAC_FD *
                UINT16 mode;
                INT32 val;
            } __attribute__((packed));
            devputcparm *pdevputcparm = (devputcparm *) params;
            doserr = dev_putc(
                    f,
                    be16toh(pdevputcparm->mode),
                    be32toh(pdevputcparm->val)
                    );
            break;
        }

        default:
            doserr = EINVFN;
            break;
    }
#ifdef DEBUG_VERBOSE
    DebugInfo("CHostXFS::XFSDevFunctions => %d", (int) doserr);
#endif
    return doserr;
}


/** **********************************************************************************************
 *
 * @brief Collect XFS drives
 *
 ************************************************************************************************/
void CHostXFS::activateXfsDrives()
{
    struct stat statbuf;

    xfs_drvbits = 0;
    for (int i = 0; i < NDRIVES; i++)
    {
        const char *path = Preferences::drvPath[i];
        unsigned flags = Preferences::drvFlags[i];
        if (path != nullptr)
        {
            int res = stat(path, &statbuf);
            if (res < 0)
            {
                (void) showAlert("Invalid Atari drive path:", path);
                continue;
            }
            // we should not get symbolic links here, because of stat, not lstat...
            mode_t ftype = (statbuf.st_mode & S_IFMT);
            if (ftype != S_IFDIR)
            {
                DebugInfo2("() -- Atari drive %c: is no directory: \"%s\"", 'A' + i, path);
                path = nullptr;
            }
        }

        if (path != nullptr)
        {
            drv_type[i] = eHostDir;
            drv_host_path[i] = CConversion::copyString(path);
            drv_longNames[i] = (flags & DRV_FLAG_8p3) == 0;
            drv_readOnly[i] = (flags & DRV_FLAG_RDONLY);
            drv_caseInsens[i] = (flags & DRV_FLAG_CASE_INSENS);
            xfs_drvbits |= (1 << i);
        }
        else
        {
            drv_host_path[i] = nullptr;    // invalid
            drv_longNames[i] = false;
            drv_readOnly[i] = false;
            drv_caseInsens[i] = false;      // case-sensitive, typical Linux/Unix file systems
        }

        drv_must_eject[i] = 0;
        drv_changed[i] = 0;
        drv_dirID[i] = 0;             // ?
    }

    // TODO: read from Preferences
    drv_atari_name['C' - 'A'] = "MAGIC";
    drv_atari_name['H' - 'A'] = "HOME";
    drv_atari_name['M' - 'A'] = "ROOT";
}


/** **********************************************************************************************
 *
 * @brief Remove a host directory drive so that it can be re-assigned
 *
 * @param[in] drv        Atari drive number 0..25
 *
 ************************************************************************************************/

void CHostXFS::eject(uint16_t drv)
{
    if ((drv < NDRIVES) && (drv_host_path[drv] != nullptr))
    {
        free((void *) drv_host_path[drv]);
        drv_host_path[drv] = nullptr;
        xfs_drvbits &= ~(1 << drv);
    }
}
