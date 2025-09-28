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
#include "HostHandles.h"
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
        eHostRoot,      //< drive is the host root directory, used for M:
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


    #if 0
    /// file driver function table, here unused
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
    } __attribute__((packed));
    #endif

    /// non XFS specific part of a Directory Descriptor
    struct MX_DD
    {
        UINT32  dd_dmd;             // struct _mx_dmd *dd_dmd;
        UINT16  dd_refcnt;
    } __attribute__((packed));

    // Atari DTA Buffer (Disk Transfer Address)
    struct MX_DTA
    {
        char    dta_res[20];        // reserved
        uint8_t dta_drive;          // officially reserved, but in fact the drive A..Z
        uint8_t dta_attribute;      // file attribute
        UINT16  dta_time;           // file modification time (big endian)
        UINT16  dta_date;           // file modification date (big endian)
        UINT32  dta_len;            // file length (big endian)
        char    dta_name[14];       // file name, maximum 8+3 plus "." plus NUL
    } __attribute__((packed));

    /// non XFS specific part of a Drive Media Descriptor
    struct MX_DMD
    {
        UINT32  d_xfs;              // struct _mx_xfs *d_xfs;
        UINT16  d_drive;
        UINT32  d_root;             // MX_DD *d_root;
        UINT16  biosdev;
        UINT32  driver;
        UINT32  devcode;
    } __attribute__((packed));

    /// non XFS specific part of a File Descriptor
    struct MX_FD
    {
        UINT32  fd_dmd;                // struct _mx_dmd *fd_dmd
        UINT16  fd_refcnt;
        UINT16  fd_mode;
        UINT32  fd_dev;                // MX_DEV *fd_dev
    } __attribute__((packed));

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
         char     sname[11];    // name to search for
         uint8_t  sattr;        // search attribute
         int32_t  dirID;        // directory (host-endian)
         int16_t  vRefNum;      // MacOS volume (host-endian)
         int16_t  index;        // Index inside that directory (host-endian)
    } __attribute__((packed));


    /// File Descriptor for the Host XFS.
    /// Theoretically this could be expanded, because the Atari side of the XFS (MACXFS.S)
    /// usuall provides pointers to the host side, but unfortunately xfs_open() instead must
    /// return the refnum, which is stored as big-endian. This is a design flaw.
    struct MAC_FD
    {
         MX_FD     fd;              // common part, big endian
         UINT16_BE refnum;          // host part: handle (big-endian)
         uint16_t  mod_time_dirty;  // host part: Fdatime() had been called (host native endian)
         uint16_t  mod_time[2];     // host part: timecode for Fdatime() (DOS-Codes) (host native endian)
    } __attribute__((packed));

    /// non XFS specific part of a Directory Handle Descriptor
    struct MX_DHD
    {
        UINT32    dhd_dmd;            // struct _mx_dmd *dhd_dmd;
    } __attribute__((packed));

    struct MAC_DIRHANDLE
    {
         MX_DHD    dhd;             // common part, big endian
         int32_t   dirID;           // directory id (host native endian)
         uint16_t  vRefNum;         // MacOS volume (host native endian)
         uint16_t  index;           // read position (host native endian)
         uint16_t  tosflag;         // TOS mode, i.e. 8+3 and without inode (host native endian)
    } __attribute__((packed));

    /// DTA buffer for xfs_sfirst() and xfs_snext()
    union MAC_DTA
    {
         MX_DTA    mxdta;           // public part, big endian
         _MAC_DTA  macdta;          // private part
    } __attribute__((packed));

    struct MX_SYMLINK
    {
        UINT16    len;              // symlink length, including EOS, even
        char      data[256];
    } __attribute__((packed));

    /// HostXFS specific part of the Directory Descriptor.
    /// Unfortunately this cannot easily be expanded, because the Atari side of the XFS (MACXFS.S)
    /// does not provide a DD pointer to xfs_drv_open() on the host side and instead only deals with
    /// the MacOS specific structure members. They are copied from the stack to the newly
    /// created internal memory block. Or in other words: The Atari side of the XFS relies on
    /// exactly this structure. This is a design flaw.
    struct MXFSDD
    {
        int32_t dirID;          // host endian format
        uint16_t vRefNum;       // host endian format
    } __attribute__((packed));

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
    static void atariFnameToHostFname(const unsigned char *src, char *dst);
    static void hostFnameToAtariFname(const char *src, unsigned char *dst);
    static int fname_is_invalid(const char *name);
    static INT32 cnverr(int err);
    static bool filename8p3_match(const char *pattern, const char *fname);
    static bool pathElemToDTA8p3(const unsigned char *path, unsigned char *name);
    static bool nameto_8_3 (const char *host_fname,
                unsigned char *dosname,
                bool flg_longnames, bool toAtari);

    // XFS-Aufrufe

    INT32 xfs_sync(uint16_t drv);
    void xfs_pterm(PD *pd);
    INT32 xfs_drv_open(uint16_t drv, MXFSDD *dd, int32_t flg_ask_diskchange);
    INT32 xfs_drv_close(uint16_t drv, uint16_t mode);
    INT32 xfs_path2DD
    (
        uint16_t mode, uint16_t drv,
        const MXFSDD *rel_dd, const char *pathname,
        const char **remain_path, MXFSDD *symlink_dd, const char **symlink,
        MXFSDD *dd,
        UINT16 *dir_drive);
    INT32 xfs_sfirst(uint16_t drv, const MXFSDD *dd, const char *name, MAC_DTA *dta, uint16_t attrib);
    INT32 xfs_snext(uint16_t drv, MAC_DTA *dta);
    INT32 xfs_fopen(char *name, uint16_t drv, MXFSDD *dd,
                uint16_t omode, uint16_t attrib);
    INT32 xfs_fdelete(uint16_t drv, MXFSDD *dd, char *name);
    INT32 xfs_link(uint16_t drv, char *nam1, char *nam2,
                   MXFSDD *dd1, MXFSDD *dd2, uint16_t mode, uint16_t dst_drv);
    INT32 xfs_xattr(uint16_t drv, MXFSDD *dd, char *name,
                    XATTR *xattr, uint16_t mode);
    INT32 xfs_attrib(uint16_t drv, MXFSDD *dd, char *name, uint16_t rwflag, uint16_t attr);
    INT32 xfs_fchown(uint16_t drv, MXFSDD *dd, char *name, uint16_t uid, uint16_t gid);
    INT32 xfs_fchmod(uint16_t drv, MXFSDD *dd, char *name, uint16_t fmode);
    INT32 xfs_dcreate(uint16_t drv, MXFSDD *dd, char *name);
    INT32 xfs_ddelete(uint16_t drv, MXFSDD *dd);
    INT32 xfs_DD2name(uint16_t drv, MXFSDD *dd, char *buf, uint16_t bufsiz);
    INT32 xfs_dopendir(MAC_DIRHANDLE *dirh, uint16_t drv, MXFSDD *dd, uint16_t tosflag);
    INT32 xfs_dreaddir(MAC_DIRHANDLE *dirh, uint16_t drv,
            uint16_t size, char *buf, XATTR *xattr, INT32 *xr);
    INT32 xfs_drewinddir(MAC_DIRHANDLE *dirh, uint16_t drv);
    INT32 xfs_dclosedir(MAC_DIRHANDLE *dirh, uint16_t drv);
    INT32 xfs_dpathconf(uint16_t drv, MXFSDD *dd, uint16_t which);
    INT32 xfs_dfree(uint16_t drv, INT32 dirID, UINT32 data[4]);
    INT32 xfs_wlabel(uint16_t drv, MXFSDD *dd, char *name);
    INT32 xfs_rlabel(uint16_t drv, MXFSDD *dd, char *name, uint16_t bufsiz);
    INT32 xfs_readlink(uint16_t drv, MXFSDD *dd, char *name,
                    char *buf, uint16_t bufsiz);
    INT32 xfs_dcntl(uint16_t drv, MXFSDD *dd, char *name, uint16_t cmd, void *pArg, unsigned char *AdrOffset68k);

    // Gerätetreiber

    INT32 dev_close(MAC_FD *f);
    INT32 dev_read(MAC_FD *f, INT32 count, char *buf);
    INT32 dev_write(MAC_FD *f, INT32 count, char *buf);
    INT32 dev_stat(MAC_FD *f, void *unsel, uint16_t rwflag, INT32 apcode);
    INT32 dev_seek(MAC_FD *f, INT32 pos, uint16_t mode);
    INT32 dev_datime(MAC_FD *f, UINT16 d[2], uint16_t rwflag);
    INT32 dev_ioctl(MAC_FD *f, uint16_t cmd, void *buf);
    INT32 dev_getc( MAC_FD *f, uint16_t mode);
    INT32 dev_getline( MAC_FD *f, char *buf, INT32 size, uint16_t mode);
    INT32 dev_putc(MAC_FD *f, uint16_t mode, INT32 val);

    // Hilfsfunktionen

    INT32 hostpath2HostFD(HostFD *reldir, uint16_t rel_hhdl, const char *path, int flags, HostHandle_t *hhdl);
    int _snext(int dir_fd, const struct dirent *entry, MAC_DTA *dta);
    INT32 xfs_symlink(uint16_t drv, MXFSDD *dd, char *name, char *to);
    void statbuf2xattr(XATTR *xattr, const struct stat *statbuf);

    void setDrivebits (uint32_t newbits, uint8_t *AdrOffset68k);
};

#endif
