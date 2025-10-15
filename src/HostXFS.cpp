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
** (TAB-Setting: 5)
** (Font = Chicago 12)
**
**
** This is the host part of Host-XFS for MagiCLinux
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
// System-Header
#include <endian.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
// Programm-Header
#include "Debug.h"
#include "Globals.h"
#include "HostXFS.h"
#include "Atari.h"
#include "emulation_globals.h"
#include "conversion.h"

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

// suppress Host XFS debug info
#undef DebugInfo
#define DebugInfo(...)
#undef DebugInfo2
#define DebugInfo2(...)


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

/*
    // M: host root

    drv_type['M'-'A'] = eHostRoot;
    drv_valid['M'-'A'] = true;
    drv_longnames['M'-'A'] = true;
    drv_dirID['M'-'A'] = 0;
    drv_host_path['M'-'A'] = "/";
*/
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
* @brief [static] Convert Atari lowercase character to uppercase
*
* @param[in]   c     Atari lowercase character
*
* @return Atari uppercase character, if convertible, otherwise c
*
 ************************************************************************************************/
char CHostXFS::toUpper(char c)
{
    /* äöüçé(a°)(ae)(oe)à(ij)(n˜)(a˜)(o/)(o˜) */
    static const char lowers[] = {'\x84','\x94','\x81','\x87','\x82','\x86','\x91','\xb4','\x85','\xc0','\xa4','\xb0','\xb3','\xb1',0};
    static const char uppers[] = {'\x8e','\x99','\x9a','\x80','\x90','\x8f','\x92','\xb5','\xb6','\xc1','\xa5','\xb7','\xb2','\xb8',0};

    if (c >= 'a' && c <= 'z')
    {
        return((char) (c & '\x5f'));
    }

    const char *found;
    if (((unsigned char) c >= 128) && ((found = strchr(lowers, c)) != nullptr))
    {
        return uppers[found - lowers];
    }

    return(c);
}


/** **********************************************************************************************
*
* @brief [static] Convert Atari filename to host filename, including path separator
*
* @param[in]   src     Atari filename
* @param[out]  dst     host filename
*
* @note: Maximum output path length is 1024 bytes, including end-of-string.
*
 ************************************************************************************************/
void CHostXFS::atariFnameToHostFname(const unsigned char *src, char *dst)
{
    char *buf_start = dst;
    while (*src && (dst < buf_start + 1022))    // TODO: add overflow handling
    {
        if (*src == '\\')
        {
            src++;
            *dst++ = '/';
        }
        else
            *dst++ = CConversion::Atari2MacFilename(*src++);
    }
    *dst = EOS;
}


/** **********************************************************************************************
*
* @brief [static] Convert host filename to Atari filename
*
* @param[in]   src     host filename
* @param[out]  dst     Atari filename (max 256 bytes including zero)
*
 ************************************************************************************************/
void CHostXFS::hostFnameToAtariFname(const char *src, unsigned char *dst)
{
    unsigned char *buf_start = dst;
    while (*src && (dst < buf_start + 255))
    {
        if (*src == '/')
        {
            src++;
            *dst++ = '\\';
        }
        else
            *dst++ = CConversion::Mac2AtariFilename(*src++);
    }

    *dst = EOS;
    if (*src)
    {
        DebugError("file name length overflow: %s", buf_start);
    }
}



#if 0
/*****************************************************************
*
*  (statisch) Testet, ob ein Dateiname gültig ist. Verboten sind Dateinamen, die
*  nur aus '.'-en bestehen.
*
******************************************************************/

int CHostXFS::fname_is_invalid(const char *name)
{
    while ((*name) == '.')        // führende Punkte weglassen
        name++;
    return !(*name);            // Name besteht nur aus Punkten
}
#endif


/** **********************************************************************************************
*
* @brief [static] Check if an 8+3 filename matches an 8+3 search pattern (each 12 bytes)
*
* @param[in]   pattern  internal 8+3 representation of search pattern, for Fsfirst() and Fsnext()
* @param[out]  fname    internal 8+3 representation of filename
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
*  @note match examples (bits RadOnly and Archive are ignored):
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
bool CHostXFS::filename8p3_match(const char *pattern, const char *fname)
{
    if (fname[0] == '\xe5')     /* Suche nach geloeschter Datei */
    {
        return((pattern[0] == '?') || (pattern[0] == fname[0]));
    }

    /* vergleiche 11 Zeichen */

    for (int i = 10; i >= 0; i--)
    {
        char c1 = *pattern++;
        char c2 = *fname++;
        if (c1 != '?')
        {
            if (toUpper(c1) != toUpper(c2))
                return false;
        }
    }

    /* vergleiche Attribut */

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

    return((bool) (c1 & c2));
}


/** **********************************************************************************************
*
* @brief [static] Convert first Atari path element to internal 8+3 format in Atari DTA
*
* @param[in]   path     Atari path
* @param[out]  name     internal 8+3 representation, for Fsfirst() and Fsnext()
*
* @return true: the path element was truncated
*
* The first path element (before '\' or EOS) is converted to the 8+3 format that is internally
* stored inside the DTA.
*
 ************************************************************************************************/
bool CHostXFS::pathElemToDTA8p3(const unsigned char *path, unsigned char *name)
{
    bool truncated = false;

    // copy a maximum of eight characters for filename

    int i;
    for (i = 0; (i < 8) && (*path) &&
         (*path != '\\') && (*path != '*') && (*path != '.') && (*path != ' '); i++)
    {
        *name++ = toUpper(*path++);
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
        *name++ = toUpper(*path++);
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


/** **********************************************************************************************
 *
 * @brief Converts host filename to 8+3 for GEMDOS
 *
 * @param[in]  host_fname       host filename
 * @param[out] dosname          8+3 GEMDOS filename
 * @param[in]  flg_longnames    true: convert to 8+3 uppercase, false: convert to 8+3
 * @param[in]  to_atari_charset true: convert host charset (UTF-8) to Atari charset
 *
 * @return true: filename was shortened
 *
 * @note Characters ' ' and '\' are skipped
 *
 ************************************************************************************************/
bool CHostXFS::nameto_8_3
(
    const char *host_fname,
    unsigned char *dosname,
    bool flg_longnames,
    bool to_atari_charset
)
{
    bool truncated = false;


    // convert up to 8 chars for filename
    int i = 0;
    while ((i < 8) && (*host_fname) && (*host_fname != '.'))
    {
        if ((*host_fname == ' ') || (*host_fname == '\\'))
        {
            host_fname++;
            continue;
        }
        if (to_atari_charset)
        {
            if (flg_longnames)
            {
                *dosname++ = CConversion::Mac2AtariFilename(*host_fname++);
            }
            else
            {
                *dosname++ = (unsigned char) toUpper((char) CConversion::Mac2AtariFilename(*host_fname++));
            }
        }
        else
        {
            if (flg_longnames)
            {
                *dosname++ = *host_fname++;
            }
            else
            {
                *dosname++ = (unsigned char) toUpper((char) (*host_fname++));
            }
        }
        i++;
    }

    while ((*host_fname) && (*host_fname != '.'))
    {
        host_fname++;        // Rest vor dem '.' ueberlesen
        truncated = true;
    }
    if (*host_fname == '.')
        host_fname++;        // '.' ueberlesen
    *dosname++ = '.';            // '.' in DOS-Dateinamen einbauen

    // convert up to 3 chars for filename extension
    i = 0;
    while ((i < 3) && (*host_fname) && (*host_fname != '.'))
    {
        if ((*host_fname == ' ') || (*host_fname == '\\'))
        {
            host_fname++;
            continue;
        }
        if (to_atari_charset)
        {
            if (flg_longnames)
                *dosname++ = CConversion::Mac2AtariFilename(*host_fname++);
            else
                *dosname++ = (unsigned char) toUpper((char) CConversion::Mac2AtariFilename(*host_fname++));
        }
        else
        {
            if (flg_longnames)
                *dosname++ = *host_fname++;
            else
                *dosname++ = (unsigned char) toUpper((char) (*host_fname++));
        }
        i++;
    }

    if (dosname[-1] == '.')        // trailing '.'
        dosname[-1] = EOS;        //   remove
    else
        *dosname = EOS;

    if (*host_fname)
        truncated = true;

    return truncated;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Synchronise a drive, i.e. write back caches
 *
 * @param[in] drv        Atari drive number 0..25
 *
 * @return 0 = OK   < 0 = error  > 0 in progress
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_sync(uint16_t drv)
{
    DebugError("NOT IMPLEMENTED %s(drv = %u)", __func__, drv);

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    // TODO: implement
    return EINVFN;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Tells the host that an Atari process has terminated
 *
 * @param[in] pd        Atari process descriptor
 *
 ************************************************************************************************/
void CHostXFS::xfs_pterm(PD *pd)
{
    DebugInfo("%s()", __func__);
    (void) pd;
}


/** **********************************************************************************************
 *
 * @brief Helper function to open a host path, relative or absolute
 *
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
            return CConversion::Host2AtariError(errno);
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

        hostFD->fd = openat(rel_fd, path, flags);
        if (hostFD->fd < 0)
        {
            DebugWarning2("() : openat(\"%s\") -> %s", path, strerror(errno));
            *hhdl = HOST_HANDLE_INVALID;
            return CConversion::Host2AtariError(errno);
        }

        int ret = fstat(hostFD->fd, &statbuf);
        if (ret < 0)
        {
            close(hostFD->fd);
            DebugWarning2("() : fstat(\"%s\") -> %s", path, strerror(errno));
            *hhdl = HOST_HANDLE_INVALID;
            return CConversion::Host2AtariError(errno);
        }

        //DebugInfo("%s() : dev=%d, ino=%d)", __func__, statbuf.st_dev, statbuf.st_ino);
        hostFD->dev = statbuf.st_dev;
        hostFD->ino = statbuf.st_ino;

        *hhdl = allocHostFD(&hostFD);
        DebugInfo2("() : host fd = %d", hostFD->fd);
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Open a drive, i.e. fill in a descriptor for it, if valid
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
 * @note Unfortunately, the XFS function mxfs_freeDD() in MACXFS.S just frees the internal
 *       memory block (IMB) instead of calling the host before. This is a severe design
 *       flaw in MAC_XFS and has historical reasons, because classic MacOS could address
 *       each file or folder in the system with a combination of a 16-bit volume reference
 *       number and a 32-bit file number, kind of inode.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_drv_open(uint16_t drv, MXFSDD *dd, int32_t flg_ask_diskchange)
{
    DebugInfo2("(drv = %u (%c:), flg = 0x%08x)", drv, 'A' + drv, flg_ask_diskchange);

    if (flg_ask_diskchange)
    {
        DebugInfo("%s() -> %d", __func__, (drv_changed[drv]) ? E_CHNG : E_OK);
        return (drv_changed[drv]) ? E_CHNG : E_OK;
    }

    drv_changed[drv] = false;        // Diskchange reset

    if (drv_changed[drv])
    {
        DebugInfo("%s() -> E_CHNG", __func__);
        return E_CHNG;
    }

    const char *pathname = drv_host_path[drv];
    if (pathname == nullptr)
    {
        DebugInfo("%s() -> EDRIVE", __func__);
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

    DebugInfo("%s() : open directory \"%s\"", __func__, pathname);

    HostHandle_t hhdl;
    INT32 atari_ret = hostpath2HostFD(nullptr, -1, pathname, /* O_PATH*/ O_DIRECTORY | O_RDONLY, &hhdl);

    dd->dirID = hhdl;           // host endian format
    dd->vRefNum = drv;          // host endian
    DebugInfo("%s() -> dirID %u", __func__, hhdl);

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
 ************************************************************************************************/
INT32 CHostXFS::xfs_drv_close(uint16_t drv, uint16_t mode)
{
    DebugInfo("%s(drv = %u, mode = %u)", __func__, drv, mode);

    /*
    // drive M: or host root may not be closed
    if ((drv == 'M'-'A') || (drv_type[drv] == eHostRoot))
    {
        return((mode) ? E_OK : EACCDN);
    }
    */

    if (drv_changed[drv])
    {
        return((mode) ? E_OK : E_CHNG);
    }

    if (drv_host_path[drv] == nullptr)
    {
        return((mode) ? E_OK : EDRIVE);
    }

    // "close" drive
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
    DebugInfo("%s(drv = %u (%c:), mode = %d, dirID = %d, vRefNum = %d, name = \"%s\")",
         __func__, drv, 'A' + drv, mode, rel_dd->dirID, rel_dd->vRefNum, pathname);
    char pathbuf[1024];

    if (drv_changed[drv])
    {
        DebugWarning("%s() -> E_CHNG", __func__);
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning("%s() -> EDRIVE", __func__);
        return EDRIVE;
    }

    char *p;
    atariFnameToHostFname((const uint8_t *) pathname, pathbuf);
    DebugInfo("%s() - host path is \"%s\"", __func__, pathbuf);
    if (mode == 0)
    {
        // The path refers to a file. Remove filename from path.
        p = strrchr(pathbuf, '/');
        if (p != nullptr)
        {
            *p++ = 0;
        }
        else
        {
            p = pathbuf;    // the file is directly located inside the given directory
            *p = '\0';
        }
        DebugInfo("%s() - remaining host path with filename removed is \"%s\"", __func__, pathbuf);
    }
    else
    {
        p = pathbuf + strlen(pathbuf);
    }

    HostHandle_t hhdl_rel = rel_dd->dirID;  // host endian
    HostFD *rel_hostFD = getHostFD(hhdl_rel);
    if (rel_hostFD == nullptr)
    {
        DebugWarning("%s() -> EINTRN", __func__);
        return EINTRN;
    }

    // O_PATH or O_DIRECTORY | O_RDONLY?
    HostHandle_t hhdl;
    INT32 atari_ret = hostpath2HostFD(rel_hostFD, hhdl_rel, pathbuf, /*O_PATH?*/ O_DIRECTORY | O_RDONLY, &hhdl);

    /*
    // Note that O_DIRECTORY is essential, otherwise fdopendir() will refuse
    int dir_fd = openat(rel_fd, pathbuf, O_DIRECTORY | O_RDONLY);
    if (dir_fd < 0)
    {
        DebugWarning("%s() : openat() -> %s", __func__, strerror(errno));
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

    int len = p - pathbuf;  // length of consumed path
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

    DebugInfo("%s() -> dirID %u", __func__, hhdl);
    return atari_ret;
}


/** **********************************************************************************************
 *
 * @brief Check if a host directory entry matches Fsfirst/next search pattern
 *
 * @param[in]  dir_fd       directory
 * @param[in]  entry        directory entry
 * @param[out] dta          file found and internal data for Fsnext
 *
 * @return 0: found, 1: mismatch, <0: error
 *
 ************************************************************************************************/
int CHostXFS::_snext(int dir_fd, const struct dirent *entry, MAC_DTA *dta)
{
    unsigned char atariname[256];   // long filename in Atari charset
    unsigned char dosname[14];      // internal, 8+3

    DebugInfo("%s() - %d \"%s\"", __func__, entry->d_type, entry->d_name);
    hostFnameToAtariFname(entry->d_name, atariname);    // convert character set..
    if (pathElemToDTA8p3(atariname, dosname))  // .. and to 8+3
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
        DebugWarning("unhandled file type %d", entry->d_type);
        return -2;   // unhandled file type
    }

    if (!filename8p3_match(dta->macdta.sname, (char *) dosname))
    {
        return 1;
    }

    int fd = openat(dir_fd, entry->d_name, O_RDONLY);
    if (fd < 0)
    {
        DebugWarning("%s() : openat(\"%s\") -> %s", __func__, entry->d_name, strerror(errno));
        return -3;
    }
    struct stat statbuf;
    int ret = fstat(fd, &statbuf);
    close(fd);
    if (ret < 0)
    {
        DebugWarning("%s() : fstat(\"%s\") -> %s", __func__, entry->d_name, strerror(errno));
        return -4;
    }
    // DebugInfo("%s() - file size = %lu\n", __func__, statbuf.st_size);

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
        DebugWarning("file size > 4 GB, shown as zero length");
        dta->mxdta.dta_len = 0;
    }
    else
    {
        dta->mxdta.dta_len = htobe32((uint32_t) statbuf.st_size);
    }

    dta->mxdta.dta_attribute = (char) dosname[11];
    nameto_8_3(entry->d_name, (unsigned char *) dta->mxdta.dta_name, false, true);
    const struct timespec *mtime = &statbuf.st_mtim;
    uint16_t time, date;
    CConversion::hostDateToDosDate(mtime->tv_sec, &time, &date);
    dta->mxdta.dta_time = htobe16(time);
    dta->mxdta.dta_date = htobe16(date);

    return 0;
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
    pathElemToDTA8p3((const unsigned char *) name, (unsigned char *) dta->macdta.sname);    // search pattern -> DTA
    dta->mxdta.dta_name[0] = 0;                    // nothing found yet
    dta->macdta.sattr = (char) attrib;            // search attribute

    if (drv_changed[drv])
    {
        DebugWarning("%s() -> E_CHNG", __func__);
        return E_CHNG ;
    }
    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning("%s() -> EDRIVE", __func__);
        return EDRIVE;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning("%s() -> EINTRN", __func__);
        return EINTRN;
    }

    // Get host fd for the directory.
    // Note that an fdopendir(), followed by readdir(), advances the file
    // read pointer while walking through the directory entries. Thus,
    // we must rewind it here, because there might have been
    // Fsfirst/Fsnext operations here before.

    int dir_fd = hostFD->fd;
    off_t lret = lseek(dir_fd, 0, SEEK_SET);
    if (lret < 0)
    {
        DebugWarning("%s() : lseek() -> %s", __func__, strerror(errno));
    }
    DebugInfo("%s() - open directory from host fd %d", __func__, dir_fd);
    int dup_dir_fd = dup(dir_fd);
    DIR *dir = fdopendir(dup_dir_fd);
    if (dir == nullptr)
    {
        DebugWarning("%s() : fdopendir() -> %s", __func__, strerror(errno));
        close(dup_dir_fd);
        return CConversion::Host2AtariError(errno);
    }

    for (;;)
    {
        errno = 0;  // strange, but following advice in man page
        struct dirent *entry = readdir(dir);
        if (entry == nullptr)
        {
            if (errno != 0)
            {
                DebugWarning("%s() : readdir() -> %s", __func__, strerror(errno));
            }
            break;  // end of directory
        }

        int match = _snext(dir_fd, entry, dta);
        if (match == 0)
        {
            //long pos = telldir(dir);
            //DebugInfo("%s() : directory read position %ld", __func__, pos);

            dta->macdta.vRefNum = (int16_t) HostHandles::snextSet(dir, hhdl, dup_dir_fd);
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
    (void) dta;

    if (drv_changed[drv])
    {
        DebugWarning("%s() -> E_CHNG", __func__);
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning("%s() -> EDRIVE", __func__);
        return EDRIVE;
    }

    if (!dta->macdta.sname[0])
    {
        DebugWarning("%s() -> ENMFIL", __func__);
        return ENMFIL;
    }

    HostHandle_t hhdl = dta->macdta.dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning("%s() -> EINTRN", __func__);
        return EINTRN;
    }

    int dir_fd = hostFD->fd;
    DebugInfo("%s() - using host fd %d", __func__, dir_fd);
    if (dir_fd == -1)
    {
        DebugWarning("%s() -> EINTRN", __func__);
        return EINTRN;
    }

    HostHandle_t hhdl2;
    DIR *dir;
    uint16_t snextHdl = (uint16_t) dta->macdta.vRefNum;
    if (HostHandles::snextGet(snextHdl, &hhdl2, &dir))
    {
        DebugWarning("%s() -> EINTRN", __func__);
        return EINTRN;
    }
    if (hhdl != hhdl2)
    {
        DebugError("%s() - dir_fd mismatch", __func__);
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
                DebugWarning("%s() : readdir() -> %s", __func__, strerror(errno));
            }
            break;  // end of directory
        }

        int match = _snext(dir_fd, entry, dta);
        if (match == 0)
        {
            //long pos = telldir(dir);
            //DebugInfo("%s() : directory read position %ld", __func__, pos);
            DebugInfo("%s() -> E_OK", __func__);
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
 * @brief Open a file
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
 * @note The Atari part of the file system driver sets fd.mod_time_dirty to FALSE.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fopen
(
    char *name,
    uint16_t drv,
    MXFSDD *dd,
    uint16_t omode,
    uint16_t attrib
)
{
    DebugInfo2("(name = \"%s\", drv = %u, omode = %d, attrib = %d)", name, drv, omode, attrib);
    unsigned char dosname[20];

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    if (!drv_longnames[drv])
    {
        // no long filenames supported, convert to upper case 8+3
        nameto_8_3(name, dosname, false, false);
        name = (char *) dosname;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        return EINTRN;
    }
    int dir_fd = hostFD->fd;
    DebugInfo("%s() - open file in directory with host fd %d", __func__, dir_fd);
    if (dir_fd == -1)
    {
        return EINTRN;
    }

    int host_omode = -1;


    // TODO: allow writing
    if (omode & OM_RPERM)
    {
        host_omode = O_RDONLY;
    }
    if (omode & OM_WPERM)
    {
        // TODO: support later
        host_omode = (host_omode == O_RDONLY) ? O_RDWR : O_WRONLY;
        return EACCDN;
    }
    if (omode & _ATARI_O_APPEND)
    {
        // TODO: support later
        return EACCDN;
    }
    if (omode & _ATARI_O_CREAT)
    {
        // TODO: support later
        (void) attrib;
        return EACCDN;
    }
    if (omode & _ATARI_O_TRUNC)
    {
        // TODO: support later
        return EACCDN;
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

    // TODO: this is a hack. It creates an endless loop
    //off_t lret = lseek(dir_fd, 0, SEEK_SET);

    file_hostFD->fd = openat(dir_fd, name, host_omode);
    if (file_hostFD->fd < 0)
    {
        DebugWarning2("() : openat(\"%s\") -> %s", name, strerror(errno));
        return CConversion::Host2AtariError(errno);
    }
    DebugInfo2("() - host fd %d", file_hostFD->fd);

    struct stat statbuf;
    int ret = fstat(file_hostFD->fd, &statbuf);
    if (ret < 0)
    {
        close(file_hostFD->fd);
        DebugWarning2("() : fstat(\"%s\") -> %s", name, strerror(errno));
        return CConversion::Host2AtariError(errno);
    }
    file_hostFD->dev = statbuf.st_dev;
    file_hostFD->ino = statbuf.st_ino;

    hhdl = allocHostFD(&file_hostFD);

    assert (hhdl <= 0xfffff);
    DebugInfo2("() -> %d", hhdl);
    return hhdl;
}


/*************************************************************
*
* Löscht eine Datei.
*
* Aliase werden NICHT dereferenziert, d.h. es wird der Alias
* selbst gelöscht.
*
*************************************************************/

INT32 CHostXFS::xfs_fdelete(uint16_t drv, MXFSDD *dd, char *name)
{
    DebugError("NOT IMPLEMENTED %s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Datei umbenennen und verschieben bzw. Hard Link erstellen.
*
*    mode == 1        Hardlink
*        == 0        Umbenennen ("move")
*
* Aliase werden NICHT dereferenziert, d.h. es wird der Alias
* selbst umbenannt.
*
* ACHTUNG: Damit <dst_drv> gültig ist, muß ein neuer MagiC-
* Kernel verwendet werden, die alten übergeben diesen
* Parameter nicht.
*
*************************************************************/

INT32 CHostXFS::xfs_link(uint16_t drv, char *nam1, char *nam2,
               MXFSDD *dd1, MXFSDD *dd2, uint16_t mode, uint16_t dst_drv)
{
    DebugError("NOT IMPLEMENTED %s(drv = %u)", __func__, drv);
    (void) nam1;
    (void) nam2;
    (void) dd1;
    (void) dd2;
    (void) mode;
    (void) dst_drv;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    // TODO: implement
    return EINVFN;
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
    pxattr->mode = htobe16(ast_mode);
    pxattr->index = htobe32(0);   // not (yet) supported;
    pxattr->dev = htobe16(0);   // not (yet) supported
    pxattr->reserved1 = htobe16(0);   // not (yet) supported
    pxattr->nlink = htobe16(0);   // not (yet) supported
    pxattr->uid = htobe16(0);   // not (yet) supported
    pxattr->gid = htobe16(0);   // not (yet) supported
    if (pstat->st_size > 0xffffffff)
    {
        DebugWarning("file size > 4 GB, shown as zero length");
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
    char *name,
    XATTR *xattr,
    uint16_t mode
)
{
    DebugInfo2("(name = \"%s\", drv = %u, mode = %d)", name, drv, mode);
    (void) mode;    // support later, if symbolic links are available
    unsigned char dosname[20];

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    if (!drv_longnames[drv])
    {
        // no long filenames supported, convert to upper case 8+3
        nameto_8_3(name, dosname, false, false);
        name = (char *) dosname;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        return EINTRN;
    }
    int dir_fd = hostFD->fd;
    DebugInfo("%s() - get information about file in directory with host fd %d", __func__, dir_fd);
    if (dir_fd == -1)
    {
        return EINTRN;
    }

    struct stat statbuf;
    int res = fstatat(dir_fd, name, &statbuf, AT_EMPTY_PATH);
    if (res < 0)
    {
        DebugWarning("%s() : fstatat() -> %s", __func__, strerror(errno));
        memset(xattr, 0, sizeof(*xattr));
        return CConversion::Host2AtariError(errno);
    }
    statbuf2xattr(xattr, &statbuf);

    DebugInfo("%s() -> E_OK", __func__);
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
 * @note TODO: We could set or reset the F_RDONLY atribute, but nothing else.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_attrib(uint16_t drv, MXFSDD *dd, char *name, uint16_t rwflag, uint16_t attr)
{
    DebugInfo2("(drv = %u, name = %s, rwflag = %u, attr = %u)", drv, name, rwflag, attr);
    unsigned char dosname[20];
    uint16_t old_attr;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }
    if (rwflag && drv_readOnly[drv])
    {
        return EWRPRO;
    }

    if (!drv_longnames[drv])
    {
        // no long filenames supported, convert to upper case 8+3
        nameto_8_3(name, dosname, false, false);
        name = (char *) dosname;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        return EINTRN;
    }
    int dir_fd = hostFD->fd;
    if (dir_fd == -1)
    {
        return EINTRN;
    }

    struct stat statbuf;
    int res = fstatat(dir_fd, name, &statbuf, AT_EMPTY_PATH);
    if (res < 0)
    {
        DebugWarning("%s() : fstatat() -> %s", __func__, strerror(errno));
        return CConversion::Host2AtariError(errno);
    }

    old_attr = 0;
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
        (void) attr;
        DebugError2("() changing attributes not implemented, yet");
        return EACCDN;
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
 * @note TODO: We could that implement later, but it has not much use.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fchown(uint16_t drv, MXFSDD *dd, char *name,
                    uint16_t uid, uint16_t gid)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }
    if (drv_readOnly[drv])
    {
        return EWRPRO;
    }
    (void) dd;
    (void) name;
    (void) uid;
    (void) gid;

    DebugWarning2("() -> EINVFN", drv, name);
    return EINVFN;
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
 * @note TODO: We could that implement later, but it has not much use.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_fchmod(uint16_t drv, MXFSDD *dd, char *name, uint16_t fmode)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }
    if (drv_readOnly[drv])
    {
        return EWRPRO;
    }
    (void) dd;
    (void) name;
    (void) fmode;

    DebugWarning2("() -> EINVFN");
    return EINVFN;
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
INT32 CHostXFS::xfs_dcreate(uint16_t drv, MXFSDD *dd, char *name)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }
    if (drv_readOnly[drv])
    {
        return EWRPRO;
    }

    unsigned char dosname[20];
    if (!drv_longnames[drv])
    {
        // no long filenames supported, convert to upper case 8+3
        nameto_8_3(name, dosname, false, false);
        name = (char *) dosname;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        return EINTRN;
    }
    int dir_fd = hostFD->fd;
    if (dir_fd == -1)
    {
        return EINTRN;
    }

    // create directory with rwxrwxrwx access, which will then be ANDed with umask
    if (mkdirat(dir_fd, name, 0777) < 0)
    {
        DebugError2("() : openat(\"%s\") -> %s", name, strerror(errno));
        return CConversion::Host2AtariError(errno);
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
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_ddelete(uint16_t drv, MXFSDD *dd)
{
    DebugInfo2("(drv = %u)", drv);
    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }
    if (drv_readOnly[drv])
    {
        return EWRPRO;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        return EINTRN;
    }

    // We cannot remove the directory via its DD or fd, instead we need a path
    // TODO: we might need the parent directory
    char pathbuf[1024];
    INT32 aret = xfs_DD2hostPath(dd, pathbuf, 1023);
    if (aret != E_OK)
    {
        DebugError2("() -> %d", aret);
        return aret;
    }

    // TODO: Change concept and keep track of parent and siblings, then
    // use unlinkat().

    if (hostFD->ref_cnt != 1)
    {
        DebugWarning2("() -- set dd.refcnt from %d to 1", hostFD->ref_cnt);
    }
    freeHostFD(hostFD);
    if (rmdir(pathbuf))
    {
        DebugWarning2("() : rmdir(\"%s\") -> %s", pathbuf, strerror(errno));
        return CConversion::Host2AtariError(errno);
    }

    DebugInfo2("() -> E_OK");
    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Get host directory path, provides functionality for Dgetpath() and Ddelete()
 *
 * @param[in] drv       Atari drive number 0..31
 * @param[in] dd        Atari directory descriptor
 * @param[in] buf       buffer for host path
 * @param[in] bufsiz    buffer size
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_DD2hostPath(MXFSDD *dd, char *pathbuf, uint16_t bufsiz)
{
    DebugInfo2("(drv = %u, dd->dirID = %u)", drv, dd->dirID);

    pathbuf[0] = '\0';      // in case of error...

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        return EINTRN;
    }

    int dir_fd = hostFD->fd;
    DebugInfo2("() : dir_fd = %d", dir_fd);
    if (dir_fd == -1)
    {
        return EINTRN;
    }

    // get host path from directory file descriptor

    char pathname[32];
    sprintf(pathname, "/proc/self/fd/%u", dir_fd);
    ssize_t size = readlink(pathname, pathbuf, bufsiz);
    if (size < 0)
    {
        DebugWarning2("() : readlink() -> %s", strerror(errno));
        return EINTRN;
    }
    pathbuf[size] = '\0';   // necessary

    DebugInfo2("() -> \"%s\"", pathbuf);
    return E_OK;
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

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (bufsiz < 64)
    {
        return EPTHOV;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
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
    hostFnameToAtariFname(atari_path, (unsigned char *) p);
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
 * @param[in]  tosflag   0: long names, >0: filenames in 8+3
 *
 * @return E_OK or negative error code
 *
 * @note Symbolic links already have been resolved in path2DD (hopefully...)
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dopendir
(
    MAC_DIRHANDLE *dirh,
    uint16_t drv,
    MXFSDD *dd,
    uint16_t tosflag
)
{
    DebugInfo2("(drv = %u, tosflag = %d)", drv, tosflag);
    if (drv_changed[drv])
    {
        DebugWarning2("() => E_CHNG");
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning2("() => EDRIVE");
        return EDRIVE;
    }

    HostHandle_t hhdl = dd->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning2("() => EINTRN");
        return EINTRN;
    }

    int dir_fd = hostFD->fd;
    off_t lret = lseek(dir_fd, 0, SEEK_SET);    // necessary if directory has been scanned before?
    if (lret < 0)
    {
        DebugWarning2("() : lseek() -> %s", strerror(errno));
    }
    DebugInfo("%s() - open directory from host fd %d", __func__, dir_fd);
    int dup_dir_fd = dup(dir_fd);
    DIR *dir = fdopendir(dup_dir_fd);
    if (dir == nullptr)
    {
        DebugWarning2("() : fdopendir() -> %s", strerror(errno));
        close(dup_dir_fd);
        return CConversion::Host2AtariError(errno);
    }

    dirh->dirID = hhdl; // unused
    dirh->vRefNum = (int16_t) HostHandles::snextSet(dir, hhdl, dup_dir_fd);
    dirh->index = 0;  // unused
    dirh -> tosflag = tosflag;

    DebugInfo("() -> E_OK");

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief For D(x)readdir(), read a directory, with long name support
 *
 * @param[in]  dirh      directory handle from Dopendir()
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  size      buffer size for name and, if requested, i-node
 * @param[out] buf       buffer for name and, if requested, i-node
 * @param[out] xattr     file information for Dxreaddir(), is nullptr for Dreaddir()
 * @param[out] xr        error code from Fxattr()
 *
 * @return E_OK or negative error code
 *
 * @note If the directory had been opened in 8+3 mode, long filenames are ignored.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_dreaddir
(
    MAC_DIRHANDLE *dirh,
    uint16_t drv,
    uint16_t size,
    char *buf,
    XATTR *xattr,
    INT32 *xr
)
{
    DebugInfo2("(drv = %u, size = %u)", drv);


    if ((dirh == nullptr) || (dirh->vRefNum == 0xffff))
    {
        DebugWarning2("() -> EIHNDL");
        return EIHNDL;
    }

    if ((dirh->tosflag) && (size < 13))
    {
        DebugWarning2("() : name buffer is too small for 8+3, ignore all entries");
        return ATARIERR_ERANGE;
    }

    if (drv_changed[drv])
    {
        DebugWarning2("() -> E_CHNG");
        return E_CHNG;
    }

    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning2("() -> EDRIVE");
        return EDRIVE;
    }

    HostHandle_t hhdl = dirh->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning2("() -> EINTRN");
        return EINTRN;
    }

    int dir_fd = hostFD->fd;
    DebugInfo("%s() - using host fd %d", __func__, dir_fd);
    if (dir_fd == -1)
    {
        DebugWarning2("() -> EINTRN");
        return EINTRN;
    }

    HostHandle_t hhdl2;
    DIR *dir;
    uint16_t snextHdl = (uint16_t) dirh->vRefNum;
    if (HostHandles::snextGet(snextHdl, &hhdl2, &dir))
    {
        DebugWarning2("() -> EINTRN");
        return EINTRN;
    }
    if (hhdl != hhdl2)
    {
        DebugError2("%s() - dir_fd mismatch");
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
            if (nameto_8_3(entry->d_name, (unsigned char *) buf, false, true))
            {
                DebugWarning2("() : filename \"%s\" does not fit to 8+3 scheme", entry->d_name);
                continue;   // filename was shortened, so it was too long for 8+3
            }
        }

        if (xattr != nullptr)
        {
            int fd = openat(dir_fd, entry->d_name, O_RDONLY);
            if (fd >= 0)
            {
                struct stat statbuf;
                int ret = fstat(fd, &statbuf);
                close(fd);
                if (ret >= 0)
                {
                    statbuf2xattr(xattr, &statbuf);
                }
                else
                {
                    DebugWarning2("() : fstat(\"%s\") -> %s", entry->d_name, strerror(errno));
                    atari_stat_err = CConversion::Host2AtariError(errno);
                    break;
                }
            }
            else
            {
                DebugWarning2("() : openat(\"%s\") -> %s", entry->d_name, strerror(errno));
                atari_stat_err = CConversion::Host2AtariError(errno);
            }
        }

        DebugInfo2("() - found \"%s\"", entry->d_name);
        // buf needs space for 4 bytes i-node plus filename plus NUL byte
		if (size >= strlen(entry->d_name) + 5)
        {
            strncpy(buf, (char *) &entry->d_ino, 4);
            buf += 4;
            hostFnameToAtariFname(entry->d_name, (unsigned char *) buf);
        }
        else
        {
            DebugError2("() : buffer size %u too small => ERANGE", size);
            atari_err = ATARIERR_ERANGE;
        }

        break;
    }

    if (xr != nullptr)
    {
        *xr = atari_stat_err;
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
INT32 CHostXFS::xfs_drewinddir(MAC_DIRHANDLE *dirh, uint16_t drv)
{
    DebugInfo2("(drv = %u)", drv);

    if ((dirh == nullptr) || (dirh->vRefNum == 0xffff))
    {
        DebugWarning2("() -> EIHNDL");
        return EIHNDL;
    }

    if (drv_changed[drv])
    {
        DebugWarning2("() -> E_CHNG");
        return E_CHNG;
    }

    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning2("() -> EDRIVE");
        return EDRIVE;
    }

    HostHandle_t hhdl = dirh->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning2("() -> EIHNDL");
        return EIHNDL;
    }

    int dir_fd = hostFD->fd;
    DebugInfo2("() - using host fd %d", dir_fd);
    if (dir_fd == -1)
    {
        DebugWarning2("() -> EINTRN");
        return EINTRN;
    }

    HostHandle_t hhdl2;
    DIR *dir;
    uint16_t snextHdl = (uint16_t) dirh->vRefNum;
    if (HostHandles::snextGet(snextHdl, &hhdl2, &dir))
    {
        DebugWarning2("s() -> EINTRN");
        dirh->vRefNum = -1;
        return EINTRN;
    }
    if (hhdl != hhdl2)
    {
        DebugError2("() - dir_fd mismatch");
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
INT32 CHostXFS::xfs_dclosedir(MAC_DIRHANDLE *dirh, uint16_t drv)
{
    DebugInfo2("(drv = %u)", drv);

    if ((dirh == nullptr) || (dirh->vRefNum == 0xffff))
    {
        DebugWarning("%s() -> EIHNDL", __func__);
        return EIHNDL;
    }

    INT32 atari_err = E_OK;
    if (drv_changed[drv])
    {
        DebugWarning("%s() -> E_CHNG", __func__);
        atari_err = E_CHNG;
    }
    else
    if (drv_host_path[drv] == nullptr)
    {
        DebugWarning("%s() -> EDRIVE", __func__);
        atari_err = EDRIVE;
    }

    HostHandle_t hhdl = dirh->dirID;
    HostFD *hostFD = getHostFD(hhdl);
    if (hostFD == nullptr)
    {
        DebugWarning("%s() -> EINTRN", __func__);
        atari_err = EIHNDL;
    }
    else
    {
        int dir_fd = hostFD->fd;
        DebugInfo("%s() - using host fd %d", __func__, dir_fd);
        if (dir_fd == -1)
        {
            DebugWarning("%s() -> EINTRN", __func__);
            atari_err = EINTRN;
        }
    }

    HostHandle_t hhdl2;
    DIR *dir;
    uint16_t snextHdl = (uint16_t) dirh->vRefNum;
    if (HostHandles::snextGet(snextHdl, &hhdl2, &dir))
    {
        DebugWarning("%s() -> EINTRN", __func__);
        dirh->vRefNum = -1;
        return EINTRN;
    }
    if (hhdl != hhdl2)
    {
        DebugError("%s() - dir_fd mismatch", __func__);
        atari_err = EINTRN;
    }

    HostHandles::snextClose(snextHdl);  // also does closedir()
    dirh->dirID = -1;
    dirh->vRefNum = -1;

    DebugInfo("%s() -> %d", __func__, atari_err);

    return atari_err;
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
        case DP_NAMEMAX:     return (drv_longnames[drv]) ? 31 : 12;
        case DP_ATOMIC:      return 512;    // ???
        case DP_TRUNC:       return (drv_longnames[drv]) ? DP_AUTOTRUNC : DP_DOSTRUNC;
        case DP_CASE:        return (drv_longnames[drv]) ? DP_CASEINSENS : DP_CASECONV;
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
    (void) dirID;
    (void) data;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    // TODO: this is dummy so far
    // 2G free, in particular 4 M blocks à 512 bytes
    data[0] = htobe32((2 * 1024) * 2 * 1024);   // # free blocks
    data[1] = htobe32((2 * 1024) * 2 * 1024);   // # total blocks
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
INT32 CHostXFS::xfs_wlabel(uint16_t drv, MXFSDD *dd, char *name)
{
    DebugInfo2("(drv = %u, name = %s)", drv, name);
    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }
    if (drv_readOnly[drv])
    {
        return EWRPRO;
    }

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
 * @param[in]  dd        Atari directory descriptor, usually for "C:\"
 * @param[out] name      buffer for name
 * @param[in]  bufsiz    size of name buffer, including zero byte
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_rlabel(uint16_t drv, MXFSDD *dd, char *name, uint16_t bufsiz)
{
    DebugInfo2("(drv = %u, bufsize = %u)", drv, bufsiz);
    (void) dd;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    const char *atari_name = drv_atari_name[drv];
    if (atari_name != nullptr)
    {
        if (bufsiz < strlen(atari_name) + 1)
        {
            return ATARIERR_ERANGE;
        }
        strcpy(name, atari_name);
    }
    else
    {
        if (bufsiz < 12)
        {
            return ATARIERR_ERANGE;
        }
        sprintf(name, "HOSTXFS.%u", drv);
    }

    DebugInfo2("() -> \"%s\"", name);

    return E_OK;
}


/*************************************************************
*
* Fuer Fsymlink
*
* Unter dem Namen <name> wird im Ordner <dirID> ein
* Alias erstellt, der die Datei <to> repraesentiert.
*
*************************************************************/

INT32 CHostXFS::xfs_symlink(uint16_t drv, MXFSDD *dd, char *name, char *to)
{
    DebugError("NOT IMPLEMENTED %s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) to;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Freadlink
*
*************************************************************/

INT32 CHostXFS::xfs_readlink(uint16_t drv, MXFSDD *dd, char *name,
                char *buf, uint16_t bufsiz)
{
   DebugError("NOT IMPLEMENTED %s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) buf;
    (void) bufsiz;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* XFS-Funktion 27 (Dcntl())
*
* Der Parameter <pArg> ist bereits umgerechnet worden von der Atari-Adresse
* in die Mac-Adresse.
*
*************************************************************/

INT32 CHostXFS::xfs_dcntl
(
    uint16_t drv,
    MXFSDD *dd,
    char *name,
    uint16_t cmd,
    void *pArg,
    uint8_t *addrOffset68kXFS
)
{
    DebugError("NOT IMPLEMENTED %s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) cmd;
    (void) pArg;
    (void) addrOffset68kXFS;

    if (drv_changed[drv])
    {
        return E_CHNG;
    }
    if (drv_host_path[drv] == nullptr)
    {
        return EDRIVE;
    }

    return EINVFN;
}



/*************************************************************/
/******************* Dateitreiber ****************************/
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
 ************************************************************************************************/
INT32 CHostXFS::dev_close(MAC_FD *f)
{
    DebugInfo("%s(fd = 0x%0x)", __func__, f);

    // decrement reference counter
    uint16_t refcnt = be16toh(f->fd.fd_refcnt);
    if (refcnt <= 0)
    {
        DebugError("invalid file refcnt");
        return EINTRN;
    }
    refcnt--;
    f->fd.fd_refcnt = htobe16(refcnt);

    if (refcnt == 0)
    {
        GET_hhdl_AND_fd
        freeHostFD(hostFD);     // also closes hostFD->fd
    }

    return E_OK;
}


/*
*
* pread() und pwrite() für "Hintergrund-DMA".
*
* Der Atari-Teil des XFS legt den ParamBlockRec (50 Bytes) auf dem Stapel
* an und initialisiert <ioCompletion>, <ioBuffer> und <ioReqCount>.
* Die Completion-Routine befindet sich
* auf der "Atari-Seite" in "macxfs.s", wird jedoch im Mac-Modus aufgerufen;
* diesen Umstand habe ich berücksichtigt.
* Die Completion-Routine erhält in a0 einen Zeiger auf den ParamBlockRec und
* in d0 (== ParamBlockrec.ioResult) den Fehlercode. Die Routine darf d0-d2 und
* a0-a1 verändern (PureC-Konvention) und ist "void". a5 ist undefiniert.
* Mit dem Trick:
*
*    INT32 geta0 ( void )
*        = 0x2008;            // MOVE.L    A0,D0
*
*    static pascal void dev_p_complete( void )
*    {
*        ParamBlockRec *pb = (ParamBlockRec *) geta0();
*    }
*
* könnte man die Routine auch in C schreiben. Den ParamBlockRec kann man
* beliebig für eigene Zwecke erweitern (z.B. a5 ablegen).
*
* Rückgabe von dev_pread() und dev_pwrite():
*
* >=0        Transfer läuft und ist beendet, wenn ioComplete den
*        Fehlercode enthält.
* <0        Fehler
*
*/

/*
static INT32 CHostXFS::dev_pwrite( MAC_FD *f, ParamBlockRec *pb )
{
    OSErr err;


    pb->ioParam.ioRefNum = f->refnum;        // Datei-Handle
    pb->ioParam.ioPosMode = 0;                // ???

    pb->ioParam.ioResult = 1;                    // warte, bis <= 0
    err = PBWriteAsync (pb);                // asynchron!
    if (err)
        return(cnverr(err));
    return(pb->ioParam.ioActCount);
}


static INT32 CHostXFS::dev_pread( MAC_FD *f, ParamBlockRec *pb )
{
    OSErr err;


    pb->ioParam.ioRefNum = f->refnum;        // Datei-Handle
    pb->ioParam.ioPosMode = 0;                // ???

    pb->ioParam.ioResult = 1;                    // warte, bis <= 0
    err = PBReadAsync (pb);                // asynchron!
    if (err == eofErr)
        err = 0;                    // nur Teil eingelesen, kein Fehler!
    if (err)
        return(cnverr(err));
    return(pb->ioParam.ioActCount);
}
*/


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
        DebugWarning("%s() : read() -> %s", __func__, strerror(errno));
        return CConversion::Host2AtariError(errno);
    }

    if (bytes > 0x7fffffff)
    {
        DebugError("file too large");
        return ATARIERR_ERANGE;
    }

    DebugInfo2("() => %d", (int32_t) bytes);
    return (int32_t) bytes;
}


INT32 CHostXFS::dev_write(MAC_FD *f, INT32 count, char *buf)
{
    DebugError("NOT IMPLEMENTED %s(fd = 0x%0x, count = %d)", __func__, f, count);
    (void) f;
    (void) count;
    (void) buf;

    // TODO: implement
    return EINVFN;
}


INT32 CHostXFS::dev_stat(MAC_FD *f, void *unsel, uint16_t rwflag, INT32 apcode)
{
    DebugError("NOT IMPLEMENTED %s(fd = 0x%0x, rwflag = %d)", __func__, f, rwflag);
    (void) f;
    (void) unsel;
    (void) rwflag;
    (void) apcode;

    // TODO: implement
    return EINVFN;
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
        return CConversion::Host2AtariError(errno);
    }

    if (offs > 0x7fffffff)
    {
        DebugError2("() - file too large");
        return ATARIERR_ERANGE;
    }

    DebugInfo2("() => %d", (int32_t) offs);
    return (int32_t) offs;
}


INT32 CHostXFS::dev_datime(MAC_FD *f, UINT16 d[2], uint16_t rwflag)
{
    DebugError("NOT IMPLEMENTED %s(fd = 0x%0x, rwflag = %d)", __func__, f, rwflag);
    (void) f;
    (void) d;
    (void) rwflag;

    // TODO: implement
    return EINVFN;
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
    DebugInfo("%s(fd = 0x%0x, cmd = %d)", __func__, f, cmd);

    GET_hhdl_AND_fd

    switch(cmd)
    {
        case FSTAT:
        {
            if (buf == nullptr)
            {
                return EINVFN;
            }
            struct stat stat;
            int res = fstat(fd, &stat);
            if (res < 0)
            {
                DebugWarning("%s() : fstat() -> %s", __func__, strerror(errno));
            }
            XATTR *pxattr = (XATTR *) buf;
            statbuf2xattr(pxattr, &stat);
            return E_OK;
            break;
        }

        case FTRUNCATE:
        {
            uint32_t newsize = be32toh(*((int32_t *) buf));
            DebugError("FTRUNCATE to size %d not yet supported", newsize);
            break;
        }

        case FMACOPENRES:
            // not supported
            break;

        case FMACGETTYCR:
            // not supported
            break;

        case FMACSETTYCR:
            // not supported
            break;

        case FMACMAGICEX:
        {
            MMEXRec *mmex = (MMEXRec *) buf;
            switch (mmex->funcNo)
            {
                case MMEX_INFO:
                    mmex->longVal = 1;
                    //  mmex->destPtr = MM_VersionPtr;
                    mmex->destPtr = 0;
                    return E_OK;

                case MMEX_GETFREFNUM:
                    // Mac-Datei-Handle liefern
                    mmex->longVal = (long) f->refnum;
                    return E_OK;
            }
        }
    }

    return EINVFN;
}

INT32 CHostXFS::dev_getc(MAC_FD *f, uint16_t mode)
{
    DebugInfo("%s(fd = 0x%0x, mode = %d)", __func__, f, mode);
    (void) mode;
    unsigned char c;
    INT32 ret;

    ret = dev_read(f, 1L, (char *) &c);
    if (ret < 0L)
        return ret;            // Fehler
    if (!ret)
        return(0x0000ff1a);        // EOF
    return c & 0x000000ff;
}


INT32 CHostXFS::dev_getline(MAC_FD *f, char *buf, INT32 size, uint16_t mode)
{
    DebugInfo("%s(fd = 0x%0x, size = %d)", __func__, f, size);
    (void) mode;
    char c;
    INT32 processed, ret;

    for (processed = 0L; processed < size;)
    {
        ret = dev_read(f, 1L, (char *) &c);
        if (ret < 0L)
            return ret;            // Fehler
        if (ret == 0L)
            break;            // EOF
        if (c == 0x0d)
            continue;
        if (c == 0x0a)
            break;
        processed++;
        *buf++ = c;
    }

    return processed;
}


INT32 CHostXFS::dev_putc(MAC_FD *f, uint16_t mode, INT32 val)
{
    DebugInfo("%s(fd = 0x%0x, mode = %d)", __func__, f, mode);
    (void) mode;
    char c;

    c = (char) val;
    return dev_write(f, 1L, (char *) & c);
}


/*************************************************************
*
* Dispatcher für XFS-Funktionen
*
* params        Zeiger auf Parameter (68k-Adresse)
* AdrOffset68k    Offset für 68k-Adresse
*
* Note that here is no endian conversion of the return
* value, because this is already done inside the 68k emulator.
*
*************************************************************/

INT32 CHostXFS::XFSFunctions(UINT32 param, uint8_t *AdrOffset68k)
{
    UINT16 fncode;
    INT32 doserr;
    unsigned char *params = AdrOffset68k + param;

    DebugInfo("%s(param = %u)", __func__, param);

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
            xfs_pterm((PD *) (AdrOffset68k + be32toh(pptermparm->pd)));
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pdrv_openparm->dd)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(ppath2DDparm->rel_dd)),
                    (char *) (AdrOffset68k + be32toh(ppath2DDparm->pathname)),
                    &remain_path,
                    (MXFSDD *) (AdrOffset68k + be32toh(ppath2DDparm->symlink_dd)),
                    &symlink,
                    (MXFSDD *) (AdrOffset68k + be32toh(ppath2DDparm->dd)),
                    (UINT16 *) (AdrOffset68k + be32toh(ppath2DDparm->dir_drive))
                    );

            // calculate Atari address from host address
            uint32_t emuAddr = remain_path - (char *) AdrOffset68k;
            // store to result
            setAtariBE32(AdrOffset68k + be32toh(ppath2DDparm->remain_path), emuAddr);
            emuAddr = symlink - (char *) AdrOffset68k;
            setAtariBE32(AdrOffset68k + be32toh(ppath2DDparm->symlink), emuAddr);
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
                    (MXFSDD *) (AdrOffset68k + be32toh(psfirstparm->dd)),
                    (char *) (AdrOffset68k + be32toh(psfirstparm->name)),
                    (MAC_DTA *) (AdrOffset68k + be32toh(psfirstparm->dta)),
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
                    (MAC_DTA *) (AdrOffset68k + be32toh(psnextparm->dta))
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
                    (char *) (AdrOffset68k + be32toh(pfopenparm->name)),
                    be16toh(pfopenparm->drv),
                    (MXFSDD *) (AdrOffset68k + be32toh(pfopenparm->dd)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pfdeleteparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pfdeleteparm->name))
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
                    (char *) (AdrOffset68k + be32toh(pflinkparm->nam1)),
                    (char *) (AdrOffset68k + be32toh(pflinkparm->nam2)),
                    (MXFSDD *) (AdrOffset68k + be32toh(pflinkparm->dd1)),
                    (MXFSDD *) (AdrOffset68k + be32toh(pflinkparm->dd2)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pxattrparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pxattrparm->name)),
                    (XATTR *) (AdrOffset68k + be32toh(pxattrparm->xattr)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pattribparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pattribparm->name)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pchownparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pchownparm->name)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pchmodparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pchmodparm->name)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pdcreateparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pdcreateparm->name))
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pddeleteparm->dd))
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pdd2nameparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pdd2nameparm->buf)),
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
                    (MAC_DIRHANDLE *) (AdrOffset68k + be32toh(pdopendirparm->dirh)),
                    be16toh(pdopendirparm->drv),
                    (MXFSDD *) (AdrOffset68k + be32toh(pdopendirparm->dd)),
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
                    (MAC_DIRHANDLE *) (AdrOffset68k + be32toh(pdreaddirparm->dirh)),
                    be16toh(pdreaddirparm->drv),
                    be16toh(pdreaddirparm->size),
                    (char *) (AdrOffset68k + be32toh(pdreaddirparm->buf)),
                    (XATTR *) ((pdreaddirparm->xattr) ? AdrOffset68k + be32toh(pdreaddirparm->xattr) : NULL),
                    (INT32 *) ((pdreaddirparm->xr) ? (AdrOffset68k + be32toh(pdreaddirparm->xr)) : NULL)
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
                    (MAC_DIRHANDLE *) (AdrOffset68k + be32toh(pdrewinddirparm->dirh)),
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
                    (MAC_DIRHANDLE *) (AdrOffset68k + be32toh(pdclosedirparm->dirh)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pdpathconfparm->dd)),
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
                    (UINT32 *) (AdrOffset68k + be32toh(pdfreeparm->data))
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pwlabelparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pwlabelparm->name))
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
                    (MXFSDD *) (AdrOffset68k + be32toh(prlabelparm->dd)),
                    (char *) (AdrOffset68k + be32toh(prlabelparm->name)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(psymlinkparm->dd)),
                    (char *) (AdrOffset68k + be32toh(psymlinkparm->name)),
                    (char *) (AdrOffset68k + be32toh(psymlinkparm->to))
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
                    (MXFSDD *) (AdrOffset68k + be32toh(preadlinkparm->dd)),
                    (char *) (AdrOffset68k + be32toh(preadlinkparm->name)),
                    (char *) (AdrOffset68k + be32toh(preadlinkparm->buf)),
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
                    (MXFSDD *) (AdrOffset68k + be32toh(pdcntlparm->dd)),
                    (char *) (AdrOffset68k + be32toh(pdcntlparm->name)),
                    be16toh(pdcntlparm->cmd),
                    AdrOffset68k + be32toh(pdcntlparm->arg),
                    AdrOffset68k
                    );
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
 * @brief Dispatcher for file driver
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] AdrOffset68k      Host address of 68k memory
 *
 * @return E_OK or negative error code
 *
 ************************************************************************************************/
INT32 CHostXFS::XFSDevFunctions(UINT32 param, uint8_t *AdrOffset68k)
{
    INT32 doserr;
    unsigned char *params = AdrOffset68k + param;
    UINT32 ifd;


    DebugInfo("%s(param = %u)", __func__, param);

    // first 2 bytes: function code
    uint16_t fncode = getAtariBE16(params + 0);
    params += 2;    // proceed to next parameter
    // next 4 bytes: pointer to MAC_FD
    ifd = *((UINT32 *) params);
    params += 4;
    MAC_FD *f = (MAC_FD *) (AdrOffset68k + be32toh(ifd));

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
                    (char *) (AdrOffset68k + be32toh(pdevreadparm->buf))
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
                    (char *) (AdrOffset68k + be32toh(pdevwriteparm->buf))
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
                    (void *) (AdrOffset68k + pdevstatparm->unsel),
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
                    (UINT16 *) (AdrOffset68k + be32toh(pdevdatimeparm->d)),
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
                    (void *) (AdrOffset68k + be32toh(pdevioctlparm->buf))
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
                    (char *) (AdrOffset68k + be32toh(pdevgetlineparm->buf)),
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
 * @brief Updates both Atari and host bitmaps of valid drives
 *
 * @param[in] newbits           valid drives as bit map
 * @param[in] AdrOffset68k      Host address of 68k memory
 *
 * Updates both Atari (_drvbits) and host variable (xfs_drvbits)
 *
 ************************************************************************************************/
void CHostXFS::setDrivebits(uint32_t newbits, uint8_t *AdrOffset68k)
{
    uint32_t val = getAtariBE32(AdrOffset68k + _drvbits);
    //newbits |= (1L << ('m'-'a'));   // virtual drive M: is always present
    val &= -1L - xfs_drvbits;       // clear old bits
    val |= newbits;                 // set new bits
    setAtariBE32(AdrOffset68k + _drvbits, val);
    xfs_drvbits = newbits;
}


/** **********************************************************************************************
 *
 * @brief Tell Atari about all XFS drives
 *
 * @param[in] AdrOffset68k      Host address of 68k memory
 *
 ************************************************************************************************/
void CHostXFS::activateXfsDrives(uint8_t *AdrOffset68k)
{
    xfs_drvbits = 0;
    for (int i = 0; i < NDRIVES; i++)
    {
        const char *path = Preferences::drvPath[i];
        unsigned flags = Preferences::drvFlags[i];
        if (path != nullptr)
        {
            drv_type[i] = eHostDir;
            drv_host_path[i] = path;
            drv_longnames[i] = (flags & 2) == 0;
            drv_readOnly[i] = (flags & 1);
            xfs_drvbits |= (1 << i);
        }
        else
        {
            drv_host_path[i] = nullptr;    // invalid
            drv_longnames[i] = false;
            drv_readOnly[i] = false;
        }

        drv_must_eject[i] = 0;
        drv_changed[i] = 0;
        drv_dirID[i] = 0;             // ?
    }

    // TODO: read from Preferences
    drv_atari_name['C' - 'A'] = "MAGIC";
    drv_atari_name['H' - 'A'] = "HOME";
    drv_atari_name['M' - 'A'] = "ROOT";

    setDrivebits(xfs_drvbits, AdrOffset68k);
}


/*************************************************************
*
* Rechnet einen Laufwerkbuchstaben um ein einen "device code".
* Wird vom Atari benötigt, um das richtige Medium auszuwerfen.
*
* Dies ist ein direkter Einsprungpunkt vom Emulator.
*
*************************************************************/

INT32 CHostXFS::Drv2DevCode(UINT32 params, uint8_t *AdrOffset68k)
{
    uint16_t drv = getAtariBE16(AdrOffset68k + params);
    assert(drv < NDRVS);

    if (drv <= 1)
    {
        // floppy disk A: & B:
        return (INT32) 0x00010000 | (drv + 1);    // liefert 1 bzw. 2
    }

    const char *vol = drv_host_path[drv];
    if (vol == nullptr)
    {
        // maybe an AHDI-Drive? Not supported yet.
        return 0;
        // return (INT32) (0x00020000 | drv);
    }
    else
    {
        // The drive is a host directory.
        // Ejecting host volumes is not supported yet.
        return 0;
        // return (INT32) (0x00010000 | (UINT16) drvNum);
    }
}


/*************************************************************
*
* Erledigt Gerätefunktionen, hier nur: Auswerfen eines
* Mediums
*
*************************************************************/
INT32 CHostXFS::RawDrvr(UINT32 param, uint8_t *AdrOffset68k)
{
    INT32 ret;
    struct sparams
    {
        UINT16 opcode;
        UINT32 device;
    };


    sparams *params = (sparams *) (AdrOffset68k + param);

    switch(params->opcode)
    {
        case 0:
        if (params->device == 0)    // ??
            ret = EDRIVE;
        else
        if ((params->device >> 16)  == 1)
            ret = EDRIVE;        // Mac-Medium
        else
            ret = EDRIVE;        // AHDI-Medium auswerfen
        break;

        default:
        ret = EINVFN;
        break;
    }
    return ret;
}
