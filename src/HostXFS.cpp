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
// Programm-Header
#include "Debug.h"
#include "Globals.h"
#include "HostXFS.h"
#include "Atari.h"
#include "TextConversion.h"
#include "HostHandles.h"

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
    for (int i = 0; i < NDRVS; i++)
    {
        drv_host_path[i]= nullptr;    // invalid
        drv_must_eject[i] = 0;
        drv_changed[i] = 0;
        drv_longnames[i] = false;
    }

    // M: host root

    drv_type['M'-'A'] = eHostRoot;
    drv_valid['M'-'A'] = true;
    drv_longnames['M'-'A'] = true;
    drv_rvsDirOrder['M'-'A'] = true;
    drv_dirID['M'-'A'] = 0;
    drv_host_path['M'-'A'] = "/";

    // C: root FS

    drv_type['C'-'A'] = eHostDir;
    drv_valid['C'-'A'] = true;
    drv_longnames['C'-'A'] = false;
    drv_rvsDirOrder['C'-'A'] = false;   // ?
    drv_dirID['C'-'A'] = 0;             // ?
    drv_host_path['C'-'A'] = CGlobals::s_atariRootfsPath;

    HostHandles::init();

    DebugInfo("CHostXFS::CHostXFS() -- Drive %c: %s dir order, %s names", 'A' + ('M'-'A'), drv_rvsDirOrder['M'-'A'] ? "reverse" : "normal", drv_longnames['M'-'A'] ? "long" : "8+3");
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
* @brief Define 68k address range, including video memory and kernel
*
* @param[in]   AtariMemSize     Atari memory size in bytes
*
 ************************************************************************************************/
void CHostXFS::set68kAdressRange(uint32_t AtariMemSize)
{
    m_AtariMemSize = AtariMemSize;
}


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
void CHostXFS::atariFnameToHostFname(const unsigned char *src, unsigned char *dst)
{
    unsigned char *buf_start = dst;
    while (*src && (dst < buf_start + 1022))    // TODO: add overflow handling
    {
        if (*src == '\\')
        {
            src++;
            *dst++ = '/';
        }
        else
            *dst++ = CTextConversion::Atari2MacFilename(*src++);
    }
    *dst = EOS;
}


/** **********************************************************************************************
*
* @brief [static] Convert host filename to Atari filename
*
* @param[in]   src     host filename
* @param[out]  dst     Atari filename
*
 ************************************************************************************************/
void CHostXFS::hostFnameToAtariFname(const unsigned char *src, unsigned char *dst)
{
    unsigned char *buf_start = dst;
    while (*src && (dst < buf_start + 1022))    // TODO: add overflow handling
    {
        *dst++ = CTextConversion::Mac2AtariFilename(*src++);
    }
    *dst = EOS;
}


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


/*****************************************************************
*
*  (statisch) konvertiert Host-Fehlercodes in MagiC- Fehlercodes
*
******************************************************************/

INT32 CHostXFS::cnverr(int err)
{
    #if 0
    switch(err)
    {
          case noErr:         return(E_OK);       /* 0      no error  */
          case nsvErr:        return(EDRIVE);     /* -35    no such volume */
          case fnfErr:        return(EFILNF);     /* -43    file not found */
          case dirNFErr:      return(EPTHNF);     // -120   directory not found
          case ioErr:         return(EREADF);     /* -36    IO Error */
          case gfpErr:                            /* -52    get file pos error */
          case rfNumErr:                          /* -51    Refnum error */
          case paramErr:                          /* -50    error in user parameter list */
          case fnOpnErr:      return(EINTRN);     /* -38    file not open */
          case bdNamErr:                          /* -37    bad name in file system (>31 Zeichen)*/
          case posErr:                            /* -40    pos before start of file */
          case eofErr:        return(ATARIERR_ERANGE);     /* -39    end of file */
          case mFulErr:       return(ENSMEM);     /* -41    memory full */
          case tmfoErr:       return(ENHNDL);     /* -42    too many open files */
          case wPrErr:        return(EWRPRO);     /* -44    disk is write protected */
          case notAFileErr:                       /* -1302  is directory, no file! */
          case wrPermErr:                         /* -61    write permission error */

          case afpAccessDenied:                   // -5000  AFP access denied
          case afpDenyConflict:                   // -5006  AFP
          case afpVolLocked:                      // -5031  AFP

          case permErr:                           /* -54    permission error */
          case opWrErr:                           /* -49    file already open with write permission */
          case dupFNErr:                          /* -48    duplicate filename */
          case fBsyErr:                           /* -47    file is busy */
          case vLckdErr:                          /* -46    volume is locked */
          case fLckdErr:      return(EACCDN);     /* -45    file is locked */
          case volOffLinErr:  return(EDRVNR);     /* -53    volume off line (ejected) */
          case badMovErr:     return ENSAME;      // -122   not same volumes or folder move from/to file
          case dataVerErr:                        // read verify error
          case verErr:        return EBADSF;      // track format verify error
    }
    #endif

    return (err == 0) ? E_OK : ERROR;
}


/*********************************************************************
*
* (statisch)
* Vergleicht ein 12- stelliges Dateimuster mit einem Dateinamen.
* Die Stelle 11 ist das Datei- Attribut(-muster).
* Rueckgabe : 1 = MATCH, sonst 0
*
* Regeln zum Vergleich der Attribute:
*    1) ReadOnly und Archive werden bei dem Vergleich NIEMALS
*       beruecksichtigt.
*    2) Ist das Suchattribut 8, werden genau alle Dateien mit gesetztem
*       Volume- Bit gefunden (auch versteckte usw.).
*    3) Ist das Suchattribut nicht 8, werden normale Dateien IMMER
*       gefunden.
*    4) Ist das Suchattribut nicht 8, werden Ordner nur bei gesetztem
*       Bit 4 gefunden.
*    5) Ist das Suchattribut nicht 8, werden Volumes nur bei gesetztem
*       Bit 3 gefunden.
*    6) Ist das Suchattribut nicht 8, werden versteckte oder System-
*       dateien (auch Ordner oder Volumes) NUR gefunden, wenn das
*       entsprechende Bit im Suchattribut gesetzt ist.
*
* Beispiele (die Bits ReadOnly und Archive sind ohne Belang):
*    8    alle Dateien mit gesetztem Bit 3 (Volumes)
*    0    nur normale Dateien
*    2    normale und versteckte Dateien
*    6    normale, versteckte und System- Dateien
*  $10    normale Dateien, normale Ordner
*  $12    normale und versteckte Dateien und Ordner
*  $16    normale und versteckte und System- Dateien und -Ordner
*   $a    normale und versteckte Dateien und Volumes
*   $e    normale, versteckte und System- Dateien und -Volumes
*  $1e    alles
*
***************************************************************************/

bool CHostXFS::filename_match(const char *pattern, const char *fname)
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
bool CHostXFS::conv_path_elem(const char *path, char *name)
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


/*************************************************************
*
* (statisch)
* Wandelt Mac-Dateinamen nach 8.3-Namen fuer GEMDOS.
* Wenn <flg_longnames> == TRUE ist, wird nicht nach Gross-Schrift
* konvertiert, das Format ist aber in jedem Fall 8+3.
* Zeichen ' ' (Leerstelle) und '\' (Backslash) werden weggelassen.
*
* Neu (6.9.95): Rückgabe TRUE, wenn der Dateiname gekürzt
* werden mußte. Fsfirst/next sowie D(x)readdir können dann
* die entsprechende Datei ignorieren.
*
*************************************************************/

bool CHostXFS::nameto_8_3(const unsigned char *macname,
                unsigned char *dosname,
                bool flg_longnames, bool toAtari)
{
    bool truncated = false;


    /* max. 8 Zeichen fuer Dateinamen kopieren */
    int i = 0;
    while ((i < 8) && (*macname) && (*macname != '.'))
    {
        if ((*macname == ' ') || (*macname == '\\'))
        {
            macname++;
            continue;
        }
        if (toAtari)
        {
            if (flg_longnames)
                *dosname++ = CTextConversion::Mac2AtariFilename(*macname++);
            else    *dosname++ = (unsigned char) toUpper((char) CTextConversion::Mac2AtariFilename(*macname++));
        }
        else
        {
            if (flg_longnames)
                *dosname++ = *macname++;
            else    *dosname++ = (unsigned char) toUpper((char) (*macname++));
        }
        i++;
    }

    while ((*macname) && (*macname != '.'))
    {
        macname++;        // Rest vor dem '.' ueberlesen
        truncated = true;
    }
    if (*macname == '.')
        macname++;        // '.' ueberlesen
    *dosname++ = '.';            // '.' in DOS-Dateinamen einbauen

    /* max. 3 Zeichen fuer Typ kopieren */
    i = 0;
    while ((i < 3) && (*macname) && (*macname != '.'))
    {
        if ((*macname == ' ') || (*macname == '\\'))
        {
            macname++;
            continue;
        }
        if (toAtari)
        {
            if (flg_longnames)
                *dosname++ = CTextConversion::Mac2AtariFilename(*macname++);
            else
                *dosname++ = (unsigned char) toUpper((char) CTextConversion::Mac2AtariFilename(*macname++));
        }
        else
        {
            if (flg_longnames)
                *dosname++ = *macname++;
            else
                *dosname++ = (unsigned char) toUpper((char) (*macname++));
        }
        i++;
    }

    if (dosname[-1] == '.')        // trailing '.'
        dosname[-1] = EOS;        //   entfernen
    else
        *dosname = EOS;

    if (*macname)
        truncated = true;

    return truncated;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Synchronise a drive, i.e. write back caches
 *
 * @param[in] drv		Atari drive number 0..25
 *
 * @return 0 = OK   < 0 = error  > 0 in progress
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_sync(uint16_t drv)
{
	DebugInfo("%s(drv = %u)", __func__, drv);

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Tells the host that an Atari process has terminated
 *
 * @param[in] pd		Atari process descriptor
 *
 ************************************************************************************************/
void CHostXFS::xfs_pterm(PD *pd)
{
	DebugInfo("%s()", __func__);
    (void) pd;
}


/** **********************************************************************************************
 *
 * @brief Atari callback: Open a drive, i.e. return a descriptor for it, if valid
 *
 * @param[in]  drv		            Atari drive number 0..25 for A: .. Z:
 * @param[out] dd		            directory descriptor of the drive's root directory
 * @param[in]  flg_ask_diskchange	only ask if disk has been changed
 *
 * @return 0 for OK or negative error code
 *
 * @note Due to a bug in Calamus (Atari program), drive M: must never return an error code.
 *
 ************************************************************************************************/
INT32 CHostXFS::xfs_drv_open(uint16_t drv, MXFSDD *dd, int32_t flg_ask_diskchange)
{
	DebugInfo("%s(drv = %u (%c:))", __func__, drv, 'A' + drv);

	if (flg_ask_diskchange)
	{
		return (drv_changed[drv]) ? E_CHNG : E_OK;
	}

	drv_changed[drv] = false;		// Diskchange reset

    if (drv_changed[drv])
    {
        return E_CHNG;
    }

    const char *pathname = drv_host_path[drv];
    if (pathname == nullptr)
    {
        return EDRIVE;
    }

    // obtain handle for root directory

	DebugInfo("%s() : open directory \"%s\"", __func__, pathname);
#if 0

    union
    {
        struct file_handle fh;
        uint8_t dummy[MAX_HANDLE_SZ];
    } ffh;

    ffh.fh.handle_bytes = MAX_HANDLE_SZ;
    int mount_id;
    int ret = name_to_handle_at(
                    -1,     // unused, we provide a path instead
                     pathname,
                     &ffh.fh,
                     &mount_id, // ?
                     AT_HANDLE_FID);

    if (ret < 0)
    {
        ret = errno;
        perror("name_to_handle_at() -> ");
    }

    DebugInfo("%s() : handle_bytes = %u", __func__, ffh.fh.handle_bytes);

    unsigned real_size = sizeof(struct file_handle) + ffh.fh.handle_bytes;
    HostHandle_t hhdl = HostHandles::alloc(real_size);
    assert(hhdl != HOST_HANDLE_INVALID);      // TODO: error handling

    void *hdata = HostHandles::getData(hhdl);
    memcpy(hdata, &ffh.fh, real_size);
#else
    int root_fd = openat(/*ignored, when path is absolute*/-1, pathname, O_PATH);
    if (root_fd < 0)
    {
        perror("openat() -> ");
        return CTextConversion::Host2AtariError(errno);
    }
    HostHandle_t hhdl = HostHandles::alloc(sizeof(root_fd));
    assert(hhdl != HOST_HANDLE_INVALID);      // TODO: error handling
    void *hdata = HostHandles::getData(hhdl);
    memcpy(hdata, &root_fd, sizeof(root_fd));
#endif

    // TODO: remember mount_id for later
    // TODO: or replace with openat() ?
    // TODO: or use open() for root and openat() for others.

    dd->dirID = hhdl;           // host endian format
    dd->vRefNum = drv;          // host endian
	DebugInfo("%s() -> dirID %u", __func__, hhdl);

    return E_OK;
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

    // drive M: or host root may not be closed
    if ((drv == 'M'-'A') || (drv_type[drv] == eHostRoot))
    {
        return((mode) ? E_OK : EACCDN);
    }

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


/*************************************************************
*
* Rechnet einen Atari-Pfad um in einen Mac-Verzeichnis-Deskriptor.
* Das Laufwerk ist bereits vom MagiX-Kernel bearbeitet worden,
* d.h. der Pfad ist z.B.
*
*    "subdir1\magx_acc.zip".
*
* Die Datei liegt tatsächlich in
*    "AWS 95:magix:MAGIX_A:subdir1:magx_acc.zip"
*
* Dabei ist
*    "AWS 95:magix:MAGIX_A"
* Das Mac-Äquivalent zu "A:\"
*
* <reldir> ist bereits die DirID des Mac-Verzeichnisses.
* mode    = 0: <name> ist Datei
*        = 1: <name> ist selbst Verzeichnis
*
*    Ausgabeparameter:
*
*     1. Fall: Es ist ein Fehler aufgetreten
*
*         Gib den Fehlercode als Funktionswert zurueck
*
*     2. Fall: Ein Verzeichnis (DirID) konnte ermittelt werden
*
*         Gib die DirID als Funktionswert zurueck.
*
*         Gib in <restpfad> den Zeiger auf den restlichen Pfad ohne
*            beginnenden '\' bzw. '/'
*
*     3. Fall: Das XFS ist bei der Pfadauswertung auf einen symbolischen
*             Link gestoßen
*
*         Gib als Funktionswert den internen Mag!X- Fehlercode ELINK
*
*         Gib in <restpfad> den Zeiger auf den restlichen Pfad ohne
*            beginnenden '\' bzw. '/'
*
*         Gib in <symlink_dir> dir DirID des Pfades, in dem der symbolische
*                 Link liegt.
*
*         Gib in <symlink> den Zeiger auf den Link selbst. Ein Link beginnt
*                 mit einem Wort (16 Bit) fuer die Laenge des Pfads,
*                 gefolgt vom Pfad selbst.
*
*                 Achtung: Die Länge muß INKLUSIVE abschließendes
*                                Nullbyte und außerdem gerade sein. Der Link
*                                muß auf einer geraden Speicheradresse
*                                liegen.
*
*                 Der Puffer für den Link kann statisch oder auch
*                 fluechtig sein, da der Kernel die Daten sofort
*                 umkopiert, ohne daß zwischendurch ein Kontextwechsel
*                 stattfinden kann.
*
*                 Wird <symlink> == NULL übergeben, wird dem Kernel
*                 signalisiert, daß der Parent des
*                 Wurzelverzeichnisses angewählt wurde. Befindet sich
*                 der Pfad etwa auf U:\A, kann der Kernel auf U:\
*                 zurückgehen.
*
* Rückgabewert *dir_drive ist die Laufwerknummer, die ist
* normalerweise drv, weil wir im Pfad das Laufwerk nicht
* wechseln.
*
*************************************************************/

INT32 CHostXFS::xfs_path2DD
(
    uint16_t mode,
    uint16_t drv, const MXFSDD *rel_dd, const char *pathname,
    const char **remain_path, MXFSDD *symlink_dd, const char **symlink,
    MXFSDD *dd,
    UINT16 *dir_drive
)
{
	DebugInfo("%s(drv = %u (%c:), dirID = %d, vRefNum = %d, name = %s)",
         __func__, drv, 'A' + drv, rel_dd->dirID, rel_dd->vRefNum, pathname);
    char pathbuf[1024];

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    char *p;
    atariFnameToHostFname((const uint8_t *) pathname, (unsigned char *) pathbuf);
    if (mode == 0)
    {
        // The path refers to a file. Remove filename from path.
        p = strrchr(pathbuf, '/');
        if (p == nullptr)
        {
            return EPTHNF;
        }
        *p++ = 0;
    }
    else
    {
        p = pathbuf + strlen(pathbuf);
    }

    HostHandle_t hhdl_rel = rel_dd->dirID;  // host endian
    assert(hhdl_rel != HOST_HANDLE_INVALID);      // TODO: error handling
    void *hdata_rel = HostHandles::getData(hhdl_rel);
    int rel_fd;
    memcpy(&rel_fd, hdata_rel, sizeof(rel_fd));     // TODO: introduce union
    assert(rel_fd > 0);

    int dir_fd = openat(rel_fd, pathbuf, O_PATH);
    if (dir_fd < 0)
    {
        perror("openat() -> ");
        return CTextConversion::Host2AtariError(errno);
    }
    HostHandle_t hhdl = HostHandles::alloc(sizeof(dir_fd));
    assert(hhdl != HOST_HANDLE_INVALID);      // TODO: error handling
    void *hdata = HostHandles::getData(hhdl);
    memcpy(hdata, &dir_fd, sizeof(dir_fd));

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

	DebugInfo("%s() -> dirID %u", __func__, hhdl);
    return E_OK;
}



/*************************************************************
*
* Wird von xfs_sfirst und xfs_snext verwendet
* Unabhaengig von drv_longnames[drv] werden die Dateien
* immer in Gross-Schrift und 8+3 konvertiert.
* MiNT konvertiert je nach Pdomain() in Gross-Schrift oder nicht.
*
* 6.9.95:
* Lange Dateinamen werden nicht gefunden.
*
* 19.11.95:
* Aliase werden dereferenziert.
*
*************************************************************/

INT32 CHostXFS::_snext(uint16_t drv, MAC_DTA *dta)
{
    (void) dta;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Durchsucht ein Verzeichnis und merkt den Suchstatus
* für Fsnext.
* Der Mac benutzt für F_SUBDIR und F_RDONLY dieselben Bits
* wie der Atari.
*
*************************************************************/

INT32 CHostXFS::xfs_sfirst(uint16_t drv, MXFSDD *dd, char *name,
                    MAC_DTA *dta, uint16_t attrib)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) dta;
    (void) attrib;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Durchsucht ein Verzeichnis weiter und merkt den Suchstatus
* fuer Fsnext.
*
*************************************************************/

INT32 CHostXFS::xfs_snext(uint16_t drv, MAC_DTA *dta)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dta;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Öffnet eine Datei.
* Liefert Fehlercode oder RefNum (eine Art Handle).
*
* fd.mod_time_dirty wird dabei automatisch vom Kernel
* auf FALSE gesetzt.
*
* Aliase werden dereferenziert.
*
* Attention: There is a design flaw in the API. The vRefNum
* shall be stored in the host's native endian mode, but
* it will be automatically converted, because it is the
* function return value in D0. As a workaround the
* return value is here converted to big endian, so it
* will be re-converted to little-endian on a little-endian-CPU.
*
*************************************************************/

INT32 CHostXFS::xfs_fopen(char *name, uint16_t drv, MXFSDD *dd,
            uint16_t omode, uint16_t attrib)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) name;
    (void) dd;
    (void) omode;
    (void) attrib;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
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
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
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
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) nam1;
    (void) nam2;
    (void) dd1;
    (void) dd2;
    (void) mode;
    (void) dst_drv;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Fxattr
*
* MODE == 0: Folge Symlinks
*
*************************************************************/

INT32 CHostXFS::xfs_xattr(uint16_t drv, MXFSDD *dd, char *name,
                XATTR *xattr, uint16_t mode)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) xattr;
    (void) mode;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Fattrib
*
* Wertet zur Zeit nur die DOS-Attribute
*    F_RDONLY
*    F_HIDDEN
*    F_ARCHIVE
* aus.
*
* Aliase werden dereferenziert.
*
*************************************************************/

INT32 CHostXFS::xfs_attrib(uint16_t drv, MXFSDD *dd, char *name, uint16_t rwflag, uint16_t attr)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) rwflag;
    (void) attr;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Fchown
*
*************************************************************/

INT32 CHostXFS::xfs_fchown(uint16_t drv, MXFSDD *dd, char *name,
                    uint16_t uid, uint16_t gid)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) uid;
    (void) gid;

    // TODO: implement
    return(EINVFN);
}


/*************************************************************
*
* Fuer Fchmod
*
*************************************************************/

INT32 CHostXFS::xfs_fchmod(uint16_t drv, MXFSDD *dd, char *name, uint16_t fmode)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) fmode;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Dcreate
*
*************************************************************/

INT32 CHostXFS::xfs_dcreate(uint16_t drv, MXFSDD *dd, char *name)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Ddelete
*
* Aliase werden NICHT dereferenziert, d.h. es wird der Alias
* selbst geloescht.
*
*************************************************************/

INT32 CHostXFS::xfs_ddelete(uint16_t drv, MXFSDD *dd)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Dgetpath
*
*************************************************************/

INT32 CHostXFS::xfs_DD2name(uint16_t drv, MXFSDD *dd, char *buf, uint16_t bufsiz)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) buf;
    (void) bufsiz;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Dopendir.
*
* Aliase brauchen hier nicht dereferenziert zu werden, weil
* dies bereits bei path2DD haette passieren muessen.
*
*************************************************************/

INT32 CHostXFS::xfs_dopendir(MAC_DIRHANDLE *dirh, uint16_t drv, MXFSDD *dd,
                uint16_t tosflag)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dirh;
    (void) drv;
    (void) dd;
    (void) tosflag;

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer D(x)readdir.
* dirh        Verzeichnis-Deskriptor
* size        Groesse des Puffers fuer Namen + ggf. Inode
* buf        Puffer fuer Namen + ggf. Inode
* xattr    Ist != NULL, wenn Dxreaddir ausgefuehrt wird.
* xr        Fehlercode von Fxattr.
*
* 6.9.95:
* Ist das TOS-Flag im <dirh> gesetzt, werden lange Dateinamen
* nicht gefunden.
*
*************************************************************/

INT32 CHostXFS::xfs_dreaddir(MAC_DIRHANDLE *dirh, uint16_t drv,
        uint16_t size, char *buf, XATTR *xattr, INT32 *xr)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dirh;
    (void) size;
    (void) buf;
    (void) xattr;
    (void) xr;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Fuer Drewinddir
*
*************************************************************/

INT32 CHostXFS::xfs_drewinddir(MAC_DIRHANDLE *dirh, uint16_t drv)
{
	DebugInfo("%s(drv = %u)", __func__, drv);

    if (drv_rvsDirOrder[drv])
    {
        return(xfs_dopendir(dirh, drv, (MXFSDD*) (&dirh->dirID), dirh->tosflag));
    }
    dirh -> index = 1;
    return(E_OK);
}


/*************************************************************
*
* Fuer Dclosedir
*
*************************************************************/

INT32 CHostXFS::xfs_dclosedir(MAC_DIRHANDLE *dirh, uint16_t drv)
{
	DebugInfo("%s(drv = %u)", __func__, drv);

    (void) drv;
    dirh -> dirID = -1L;
    return(E_OK);
}


/*************************************************************
*
* Fuer Dpathconf
*
* mode = -1:   max. legal value for n in Dpathconf(n)
*         0:   internal limit on the number of open files
*         1:   max. number of links to a file
*         2:   max. length of a full path name
*         3:   max. length of an individual file name
*         4:   number of bytes that can be written atomically
*         5:   information about file name truncation
*              0 = File names are never truncated; if the file name in
*                  any system call affecting  this  directory  exceeds
*                  the  maximum  length (returned by mode 3), then the
*                  error value ERANGE is  returned  from  that  system
*                  call.
*
*              1 = File names are automatically truncated to the maxi-
*                  mum length.
*
*              2 = File names are truncated according  to  DOS  rules,
*                  i.e. to a maximum 8 character base name and a maxi-
*                  mum 3 character extension.
*         6:   0 = case-sensitiv
*              1 = nicht case-sensitiv, immer in Gross-Schrift
*              2 = nicht case-sensitiv, aber unbeeinflusst
*         7:   Information ueber unterstuetzte Attribute und Modi
*         8:   information ueber gueltige Felder in XATTR
*
*      If any  of these items are unlimited, then 0x7fffffffL is
*      returned.
*
* Aliase brauchen hier nicht dereferenziert zu werden, weil
* dies bereits bei path2DD haette passieren muessen.
*
*************************************************************/

INT32 CHostXFS::xfs_dpathconf(uint16_t drv, MXFSDD *dd, uint16_t which)
{
	DebugInfo("%s(drv = %u)", __func__, drv);

    (void) dd;
    switch(which)
    {
        case    DP_MAXREQ:    return(DP_XATTRFIELDS);
        case    DP_IOPEN:    return(100);    // ???
        case    DP_MAXLINKS:    return(1);
        case    DP_PATHMAX:    return(128);
        case    DP_NAMEMAX:    return((drv_longnames[drv]) ? 31 : 12);
        case    DP_ATOMIC:    return(512);    // ???
        case    DP_TRUNC:    return((drv_longnames[drv]) ? DP_AUTOTRUNC : DP_DOSTRUNC);
        case    DP_CASE:        return((drv_longnames[drv]) ? DP_CASEINSENS : DP_CASECONV);
        case    DP_MODEATTR:    return(F_RDONLY+F_SUBDIR+F_ARCHIVE+F_HIDDEN
                            +DP_FT_DIR+DP_FT_REG+DP_FT_LNK);
        case    DP_XATTRFIELDS:return(DP_INDEX+DP_DEV+DP_NLINK+DP_BLKSIZE
                            +DP_SIZE+DP_NBLOCKS
                            +DP_CTIME+DP_MTIME);
    }
    return(EINVFN);
}


/*************************************************************
*
* Fuer Dfree
*
* data = free,total,secsiz,clsiz
*
* Aliase brauchen hier nicht dereferenziert zu werden, weil
* dies bereits bei path2DD haette passieren muessen.
*
*************************************************************/

INT32 CHostXFS::xfs_dfree(uint16_t drv, INT32 dirID, UINT32 data[4])
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dirID;
    (void) data;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
}


/*************************************************************
*
* Disklabel.
* Zurueckgeliefert wird der Macintosh-Name.
* Das Aendern des Labels ist z.Zt. nicht moeglich.
*
* Bei allen Operationen, die Dateinamen zurueckliefern, ist
* darauf zu achten, dass die Angabe der Puffergroesse immer
* INKLUSIVE End-of-String ist.
*
*************************************************************/

INT32 CHostXFS::xfs_wlabel(uint16_t drv, MXFSDD *dd, char *name)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return(EINVFN);
}

INT32 CHostXFS::xfs_rlabel(uint16_t drv, MXFSDD *dd, char *name, uint16_t bufsiz)
{
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) bufsiz;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    // TODO: implement
    return EINVFN;
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
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) to;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
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
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) buf;
    (void) bufsiz;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
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
	DebugInfo("%s(drv = %u)", __func__, drv);
    (void) dd;
    (void) name;
    (void) cmd;
    (void) pArg;
    (void) addrOffset68kXFS;

    if (drv_changed[drv])
    {
        return(E_CHNG);
    }
    if (drv_host_path[drv] == nullptr)
    {
        return(EDRIVE);
    }

    return(EINVFN);
}



/*************************************************************/
/******************* Dateitreiber ****************************/
/*************************************************************/

INT32 CHostXFS::dev_close(MAC_FD *f)
{
    (void) f;

    // TODO: implement
    return EINVFN;
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

INT32 CHostXFS::dev_read(MAC_FD *f, INT32 count, char *buf)
{
    (void) f;
    (void) count;
    (void) buf;

    // TODO: implement
    return EINVFN;
}


INT32 CHostXFS::dev_write(MAC_FD *f, INT32 count, char *buf)
{
    (void) f;
    (void) count;
    (void) buf;

    // TODO: implement
    return EINVFN;
}


INT32 CHostXFS::dev_stat(MAC_FD *f, void *unsel, uint16_t rwflag, INT32 apcode)
{
    (void) f;
    (void) unsel;
    (void) rwflag;
    (void) apcode;

    // TODO: implement
    return EINVFN;
}


INT32 CHostXFS::dev_seek(MAC_FD *f, INT32 pos, uint16_t mode)
{
    (void) f;
    (void) pos;
    (void) mode;

    // TODO: implement
    return EINVFN;
}


INT32 CHostXFS::dev_datime(MAC_FD *f, UINT16 d[2], uint16_t rwflag)
{
    (void) f;
    (void) d;
    (void) rwflag;

    // TODO: implement
    return EINVFN;
}


INT32 CHostXFS::dev_ioctl(MAC_FD *f, uint16_t cmd, void *buf)
{
    (void) f;
    (void) cmd;
    (void) buf;
    // TODO: implement
    return EINVFN;
}

INT32 CHostXFS::dev_getc(MAC_FD *f, uint16_t mode)
{
    (void) mode;
    unsigned char c;
    INT32 ret;

    ret = dev_read(f, 1L, (char *) &c);
    if (ret < 0L)
        return(ret);            // Fehler
    if (!ret)
        return(0x0000ff1a);        // EOF
    return(c & 0x000000ff);
}


INT32 CHostXFS::dev_getline(MAC_FD *f, char *buf, INT32 size, uint16_t mode)
{
    (void) mode;
    char c;
    INT32 gelesen,ret;

    for    (gelesen = 0L; gelesen < size;)
        {
        ret = dev_read(f, 1L, (char *) &c);
        if (ret < 0L)
            return(ret);            // Fehler
        if (ret == 0L)
            break;            // EOF
        if (c == 0x0d)
            continue;
        if (c == 0x0a)
            break;
        gelesen++;
        *buf++ = c;
        }
    return(gelesen);
}


INT32 CHostXFS::dev_putc(MAC_FD *f, uint16_t mode, INT32 val)
{
    (void) mode;
    char c;

    c = (char) val;
    return(dev_write(f, 1L, (char *) &c));
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
            if (be32toh((UINT32) (pdcreateparm->name)) >= m_AtariMemSize)
            {
                DebugError("CHostXFS::xfs_dcreate() - invalid name ptr");
                return(ERROR);
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
    return(doserr);
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
    newbits |= (1L << ('m'-'a'));   // virtual drive M: is always present
    val &= -1L - xfs_drvbits;       // clear old bits
    val |= newbits;                 // set new bits
    setAtariBE32(AdrOffset68k + _drvbits, val);
    xfs_drvbits = newbits;
}


/** **********************************************************************************************
 *
 * @brief Define an Atari drive as host directory
 *
 * @param[in] drv               drive number 0..31
 * @param[in] drvType           drive type
 * @param[in] path              host path
 * @param[in] longnames         false: support only 8+3 file names
 * @param[in] reverseDirOrder   TODO: discuss and remove if possible
 * @param[in] AdrOffset68k      Host address of 68k memory
 *
 ************************************************************************************************/
void CHostXFS::SetXFSDrive
(
    uint16_t drv,
    HostXFSDrvType drvType,
    const char *path,
    bool bLongNames,
    bool bReverseDirOrder,
    uint8_t *AdrOffset68k
)
{
    DebugInfo("CHostXFS::%s(%c: has type %s)", __FUNCTION__, 'A' + drv, xfsDrvTypeToStr(drvType));

    uint32_t newbits = xfs_drvbits;

#ifdef SPECIALDRIVE_AB
    if (drv >= 2)
    {
#endif
    if (drvType == eNoHostXFS)
    {
        // The drive no longer is managed by host, remove it.
        newbits &= ~(1L << drv);
    }
    else
    {
        // The drive is a HostXFS drive
        newbits |= 1L << drv;
    }

#ifdef SPECIALDRIVE_AB
    }
#endif

    drv_changed[drv] = true;
    drv_host_path[drv] = path;      // TODO: clarify who maintains the string memory
    drv_type[drv] = drvType;

    if (drvType == eHostDir)
    {
        drv_valid[drv] = true;
    }
    else
    {
        drv_valid[drv] = false;
        drv_host_path[drv] = nullptr;
    }

    drv_longnames[drv] = bLongNames;
    drv_rvsDirOrder[drv] = bReverseDirOrder;

    DebugInfo("CHostXFS::SetXFSDrive() -- Drive %c: %s dir order, %s names", 'A' + drv, drv_rvsDirOrder[drv] ? "reverse" : "normal", drv_longnames[drv] ? "long" : "8+3");

#ifdef SPECIALDRIVE_AB
    if (drv>=2)
#endif
    setDrivebits(newbits, AdrOffset68k);
}


/** **********************************************************************************************
 *
 * @brief Change host drive configuration
 *
 * @param[in] drv               drive number 0..31
 * @param[in] bLongNames        false: support only 8+3 file names
 * @param[in] bReverseDirOrder  TODO: discuss and remove if possible
 *
 ************************************************************************************************/
void CHostXFS::ChangeXFSDriveFlags
(
    uint16_t drv,
    bool bLongNames,
    bool bReverseDirOrder
)
{
    drv_longnames[drv] = bLongNames;
    drv_rvsDirOrder[drv] = bReverseDirOrder;

    DebugInfo("CHostXFS::ChangeXFSDriveFlags() -- Drive %c: %s dir order, %s names", 'A' + drv, drv_rvsDirOrder[drv] ? "reverse" : "normal", drv_longnames[drv] ? "long" : "8+3");
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
        return((INT32) 0x00010000 | (drv + 1));    // liefert 1 bzw. 2
    }

    const char *vol = drv_host_path[drv];
    if (vol == nullptr)
    {
        // maybe an AHDI-Drive? Not supported yet.
        return 0;
        // return((INT32) (0x00020000 | drv));
    }
    else
    {
        // The drive is a host directory.
        // Ejecting host volumes is not supported yet.
        return 0;
        // return((INT32) (0x00010000 | (UINT16) drvNum));
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
    return(ret);
}
