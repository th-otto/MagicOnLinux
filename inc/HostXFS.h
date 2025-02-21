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
 * HostXFS.h
 * =========
 *
 * Structures for data exchange between host machine and hostXFS of MagiCLinux
 *
 */

#ifndef _MACXFS_H_INCLUDED_
#define _MACXFS_H_INCLUDED_

// System-Header
#include <stdint.h>
// Programm-Header
#include "osd_cpu.h"
#include "Atari.h"
#include "MAC_XFS.H"
// Definitionen
#define NDRVS  32
//#define SPECIALDRIVE_AB
#ifndef ELINK
#define ELINK -300
#endif


class CHostXFS
{
   public:

    /// keep track which Atari "drives" we are responsible for
    enum HostXFSDrvType
    {
        eNoHostXFS,     //< drive is NOT a host directory
        eHostRoot,      //< drive is the host root directory
        eHostDir        //< drive is a host directory
    };

    #if defined(_DEBUG)
    const char *xfsDrvTypeToStr(HostXFSDrvType type)
    {
        switch(type)
        {
            case eNoHostXFS: return "NoHostXFS"; break;
            case eHostRoot: return "HostRoot"; break;
            case eHostDir: return "HostDir"; break;
            default: return "UNKNOWN"; break;
        }
    }
    #endif

    CHostXFS();
    ~CHostXFS();

    void set68kAdressRange(uint32_t AtariMemSize);
    INT32 XFSFunctions(uint32_t params, uint8_t *AdrOffset68k);
    INT32 XFSDevFunctions(uint32_t params, uint8_t *AdrOffset68k);
    INT32 Drv2DevCode(uint32_t params, uint8_t *AdrOffset68k);
    INT32 RawDrvr(uint32_t params, uint8_t *AdrOffset68k);
    void SetXFSDrive(
            uint16_t drv,
            HostXFSDrvType drvType,
            const char *path,
            bool bLongNames,
            bool bReverseDirOrder,
            uint8_t *AdrOffset68k);
    void ChangeXFSDriveFlags(
            uint16_t drv,
            bool bLongNames,
            bool bReverseDirOrder);

   private:

    uint32_t m_AtariMemSize;
    typedef void PD;

#pragma options align=packed

	struct MX_DHD
	{
		UINT32	dhd_dmd;            // struct _mx_dmd *dhd_dmd;
	};

	struct MX_DEV
	{
		INT32   dev_close;
		INT32   dev_read;
		INT32   dev_write;
		INT32   dev_stat;
		INT32   dev_seek;
		INT32   dev_datime;
		INT32   dev_ioctl;
		INT32   dev_getc;
		INT32   dev_getline;
		INT32   dev_putc;
	};

    struct MX_DD
    {
        UINT32  dd_dmd;                // struct _mx_dmd *dd_dmd;
        UINT16  dd_refcnt;
    };

    // Atari DTA Buffer (Disk Transfer Address)
    struct MX_DTA
    {
        char    dta_res[20];        // reserved
        char    dta_drive;          // officially reserved, but in fact the drive A..Z
        char    dta_attribute;      // file attribute
        UINT16  dta_time;           // file modification time (big endian)
        UINT16  dta_date;           // file modification date (big endian)
        UINT32  dta_len;            // file length (big endian)
        char    dta_name[14];       // file name, maximum 8+3 plus "." plus NUL
    };

    struct MX_DMD
    {
        UINT32        d_xfs;                // struct _mx_xfs *d_xfs;
        UINT16      d_drive;
        UINT32        d_root;                // MX_DD     *d_root;
        UINT16      biosdev;
        UINT32      driver;
        UINT32      devcode;
    };

    struct MX_FD
    {
        UINT32        fd_dmd;                // struct _mx_dmd *fd_dmd;
        UINT16      fd_refcnt;
        UINT16      fd_mode;
        UINT32        fd_dev;                // MX_DEV    *fd_dev;
    };

    /* Open- Modus von Dateien (Mag!X- intern)                                 */
    /* NOINHERIT wird nicht unterstuetzt, weil nach TOS- Konvention nur die     */
    /* Handles 0..5 vererbt werden                                             */
    /* HiByte wie unter MiNT verwendet                                         */

    #define   OM_RPERM       1
    #define   OM_WPERM       2
    #define   OM_EXEC        4
    #define   OM_APPEND      8
    #define   OM_RDENY       16
    #define   OM_WDENY       32
    #define   OM_NOCHECK     64

    struct _MAC_DTA
    {
         char      sname[11];        /* Suchname */
         char      sattr;            /* Suchattribut */
         INT32      dirID;            /* Verzeichnis */
         INT16    vRefNum;        /* Mac-Volume */
         UINT16      index;        /* Index innerhalb des Verzeichnis */
    };

    struct MAC_FD
    {
         MX_FD    fd;            /* allgemeiner Teil (big endian) */
         short    refnum;        /* Mac-Teil: Handle (host native endian) */
         UINT16    mod_time_dirty;    /* Mac-Teil: Fdatime war aufgerufen (host native endian) */
         UINT16    mod_time[2];    /* Mac-Teil: Zeit fuer Fdatime (DOS-Codes) (host native endian) */
    };

    struct MAC_DIRHANDLE
    {
         MX_DHD    dhd;            /* allgemeiner Teil */
         INT32    dirID;            /* Verzeichniskennung (host native endian) */
         short    vRefNum;        // Mac-Volume (host native endian)
         UINT16    index;        /* Position des Lesezeigers (host native endian) */
         UINT16    tosflag;        /* TOS-Modus, d.h. 8+3 und ohne Inode (host native endian) */
    };

    union MAC_DTA
    {
         MX_DTA    mxdta;
         _MAC_DTA  macdta;
    };

    struct MX_SYMLINK
    {
      UINT16    len;            /* Symlink-Laenge inklusive EOS, gerade */
      char        data[256];
    };

    struct MXFSDD
    {
        long dirID;
        short vRefNum;
    };

    #pragma options align=reset

    //bool GetXFSRootDir (short drv, short *vRefNum, long *dirID);
    UINT32 DriveToDeviceCode (short drv);
    long EjectDevice (short opcode, long device);

    // lokale Variablen:
    // -----------------

    bool drv_changed[NDRVS];
    bool drv_must_eject[NDRVS];
    uint32_t xfs_drvbits;
/*
* NDRVS Laufwerke werden vom XFS verwaltet
* Fuer jedes Laufwerk gibt es einen FSSpec, der
* das MAC-Verzeichnis repraesentiert, das fuer
* "x:\" steht.
* Ung�ltige FSSpec haben die Volume-Id 0.
*/
/*
    FSSpec drv_fsspec[NDRVS];        // => macSys, damit MagiC die volume-ID
                            // ermitteln kann.

    FSRef xfs_path[NDRVS];        // nur auswerten, wenn drv_valid = true
*/
    const char *drv_host_path[NDRVS];     // nullptr, if not valid
    bool drv_valid[NDRVS];            // zeigt an, ob alias g�ltig ist.
    long drv_dirID[NDRVS];
    bool drv_longnames[NDRVS];            // initialisiert auf 0en
    bool drv_rvsDirOrder[NDRVS];
    bool drv_readOnly[NDRVS];
    HostXFSDrvType drv_type[NDRVS];
    /* Zur Rueckgabe an den MagiC-Kernel: */
    MX_SYMLINK mx_symlink;

    // statische Funktionen

    static char toUpper(char c);
    static void AtariFnameToMacFname(const unsigned char *src, unsigned char *dst);
    static void MacFnameToAtariFname(const unsigned char *src, unsigned char *dst);
    static int fname_is_invalid( char *name);
    static INT32 cnverr(int err);
    static bool filename_match(char *muster, char *fname);
    static bool conv_path_elem(const char *path, char *name);
    static bool nameto_8_3 (const unsigned char *macname,
                unsigned char *dosname,
                bool flg_longnames, bool toAtari);

    // XFS-Aufrufe

    INT32 xfs_sync(UINT16 drv);
    void xfs_pterm (PD *pd);
    INT32 xfs_drv_open (UINT16 drv, MXFSDD *dd, INT32 flg_ask_diskchange);
    INT32 xfs_drv_close(UINT16 drv, UINT16 mode);
    INT32 xfs_path2DD(UINT16 mode, UINT16 drv, MXFSDD *rel_dd, char *pathname,
                  char **restpfad, MXFSDD *symlink_dd, char **symlink,
                   MXFSDD *dd,
                   UINT16 *dir_drive );
    INT32 xfs_sfirst(UINT16 drv, MXFSDD *dd, char *name, MAC_DTA *dta, UINT16 attrib);
    INT32 xfs_snext(UINT16 drv, MAC_DTA *dta);
    INT32 xfs_fopen(char *name, UINT16 drv, MXFSDD *dd,
                UINT16 omode, UINT16 attrib);
    INT32 xfs_fdelete(UINT16 drv, MXFSDD *dd, char *name);
    INT32 xfs_link(UINT16 drv, char *nam1, char *nam2,
                   MXFSDD *dd1, MXFSDD *dd2, UINT16 mode, UINT16 dst_drv);
    INT32 xfs_xattr(UINT16 drv, MXFSDD *dd, char *name,
                    XATTR *xattr, UINT16 mode);
    INT32 xfs_attrib(UINT16 drv, MXFSDD *dd, char *name, UINT16 rwflag, UINT16 attr);
    INT32 xfs_fchown(UINT16 drv, MXFSDD *dd, char *name, UINT16 uid, UINT16 gid);
    INT32 xfs_fchmod(UINT16 drv, MXFSDD *dd, char *name, UINT16 fmode);
    INT32 xfs_dcreate(UINT16 drv, MXFSDD *dd, char *name);
    INT32 xfs_ddelete(UINT16 drv, MXFSDD *dd);
    INT32 xfs_DD2name(UINT16 drv, MXFSDD *dd, char *buf, UINT16 bufsiz);
    INT32 xfs_dopendir(MAC_DIRHANDLE *dirh, UINT16 drv, MXFSDD *dd, UINT16 tosflag);
    INT32 xfs_dreaddir(MAC_DIRHANDLE *dirh, UINT16 drv,
            UINT16 size, char *buf, XATTR *xattr, INT32 *xr);
    INT32 xfs_drewinddir(MAC_DIRHANDLE *dirh, UINT16 drv);
    INT32 xfs_dclosedir(MAC_DIRHANDLE *dirh, UINT16 drv);
    INT32 xfs_dpathconf(UINT16 drv, MXFSDD *dd, UINT16 which);
    INT32 xfs_dfree(UINT16 drv, INT32 dirID, UINT32 data[4]);
    INT32 xfs_wlabel(UINT16 drv, MXFSDD *dd, char *name);
    INT32 xfs_rlabel(UINT16 drv, MXFSDD *dd, char *name, UINT16 bufsiz);
    INT32 xfs_readlink(UINT16 drv, MXFSDD *dd, char *name,
                    char *buf, UINT16 bufsiz);
    INT32 xfs_dcntl(UINT16 drv, MXFSDD *dd, char *name, UINT16 cmd, void *pArg, unsigned char *AdrOffset68k);

    // Gerätetreiber

    INT32 dev_close( MAC_FD *f );
    INT32 dev_read( MAC_FD *f, INT32 count, char *buf );
    INT32 dev_write( MAC_FD *f, INT32 count, char *buf );
    INT32 dev_stat( MAC_FD *f, void *unsel, UINT16 rwflag, INT32 apcode );
    INT32 dev_seek( MAC_FD *f, INT32 pos, UINT16 mode );
    INT32 dev_datime( MAC_FD *f, UINT16 d[2], UINT16 rwflag );
    INT32 dev_ioctl( MAC_FD *f, UINT16 cmd, void *buf );
    INT32 dev_getc( MAC_FD *f, UINT16 mode );
    INT32 dev_getline( MAC_FD *f, char *buf, INT32 size, UINT16 mode );
    INT32 dev_putc( MAC_FD *f, UINT16 mode, INT32 val );

    // Hilfsfunktionen

    INT32 _snext(UINT16 drv, MAC_DTA *dta);
    INT32 xfs_symlink(UINT16 drv, MXFSDD *dd, char *name, char *to);

    void setDrivebits (uint32_t newbits, uint8_t *AdrOffset68k);
};

#endif
