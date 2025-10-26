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

#include <stdint.h>
#include "Atari.h"
#include "HostHandles.h"
#include "MAC_XFS.H"

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
        //eHostRoot,      //< drive is the host root directory, used for M:
        eHostDir        //< drive is a host directory
    };

    #if defined(_DEBUG)
    const char *xfsDrvTypeToStr(HostXFSDrvType type)
    {
        switch(type)
        {
            case eNoHostXFS: return "NoHostXFS"; break;
            //case eHostRoot: return "HostRoot"; break;
            case eHostDir: return "HostDir"; break;
            default: return "UNKNOWN"; break;
        }
    }
    #endif

    CHostXFS();
    ~CHostXFS();

    INT32 XFSFunctions(uint32_t params, uint8_t *addrOffset68k);
    INT32 XFSDevFunctions(uint32_t params, uint8_t *addrOffset68k);
    INT32 Drv2DevCode(uint32_t params, uint8_t *addrOffset68k);
    INT32 RawDrvr(uint32_t params, uint8_t *addrOffset68k);
    void activateXfsDrives(uint8_t *addrOffset68k);

   private:

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

    /*
     * MagiC internal file open modes.
     * NOINHERIT is not supported, because according to GEMDOS rules only
     * handles 0..5 are inherited to child processes.
     * HiByte used like in MiNT
     */

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
         uint16_t  mod_tdate_dirty; // host part: Fdatime() had been called (host native endian)
         uint16_t  mod_time;        // host part: time code for Fdatime() (DOS-Codes) (host native endian)
         uint16_t  mod_date;        // host part: date code for Fdatime() (DOS-Codes) (host native endian)
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

    //
    // attributes
    //

    bool drv_changed[NDRIVES];
    bool drv_must_eject[NDRIVES];
    uint32_t xfs_drvbits;
    const char *drv_host_path[NDRIVES];       // nullptr, if not valid
    const char *drv_atari_name[NDRIVES];      // nullptr, if not valid
    const uint32_t new_file_perm = 0600;      // Unix permissions for new files: rw-rw---- (user and group have rw access)
    long drv_dirID[NDRIVES];
    bool drv_longNames[NDRIVES];              // initialised with zeros
    bool drv_readOnly[NDRIVES];
    bool drv_caseInsens[NDRIVES];           // case-insensitive file system, like Apple's HFS(+)
    HostXFSDrvType drv_type[NDRIVES];
    // Information to be passed back to MagiC kernel:
    MX_SYMLINK mx_symlink;

    // static functions

    static bool nameto_8_3(const char *fname,
                unsigned char *dosname,
                bool upperCase, bool toAtari);
    static INT32 hostFd2Path(int dir_fd, char *pathbuf, uint16_t bufsiz);
    static int getDrvNo(char c);
    int atariPath2HostPath(const unsigned char *src, unsigned default_drv, char *dst, unsigned bufsiz);
    int hostPath2AtariPath(const char *src, unsigned default_drv, char unsigned *dst, unsigned bufsiz);

    static int atariFnameToHostFname(const unsigned char *src, char *dst, unsigned bufsiz);
    int atariFnameToHostFnameCond8p3(uint16_t drv, const unsigned char *atari_fname, char *host_fname, unsigned bufsiz);
    static int hostFnameToAtariFname(const char *src, unsigned char *dst, unsigned bufsiz);
    static bool hostFnameToAtariFname8p3(const char *host_fname, unsigned char *dosname, bool upperCase);
    static bool filename8p3_match(const char *pattern, const char *fname, bool upperCase);
    static bool pathElemToDTA8p3(const unsigned char *path, unsigned char *name, bool upperCase);
    static void statbuf2xattr(XATTR *xattr, const struct stat *statbuf);

    // XFS calls

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
    INT32 xfs_fopen(const unsigned char *name, uint16_t drv, MXFSDD *dd,
                uint16_t omode, uint16_t attrib);
    INT32 xfs_fdelete(uint16_t drv, MXFSDD *dd, const unsigned char *name);
    INT32 xfs_link(uint16_t drv, const unsigned char *name_from, const unsigned char *name_to,
                   MXFSDD *dd_from, MXFSDD *dd_to, uint16_t mode, uint16_t dst_drv);
    INT32 xfs_xattr(uint16_t drv, MXFSDD *dd, const unsigned char *name,
                    XATTR *xattr, uint16_t mode);
    INT32 xfs_attrib(uint16_t drv, MXFSDD *dd, const unsigned char *name, uint16_t rwflag, uint16_t attr);
    INT32 xfs_fchown(uint16_t drv, MXFSDD *dd, const unsigned char *name, uint16_t uid, uint16_t gid);
    INT32 xfs_fchmod(uint16_t drv, MXFSDD *dd, const unsigned char *name, uint16_t fmode);
    INT32 xfs_dcreate(uint16_t drv, MXFSDD *dd, const unsigned char *name);
    INT32 xfs_ddelete(uint16_t drv, MXFSDD *dd);
    INT32 xfs_DD2hostPath(MXFSDD *dd, char *buf, uint16_t bufsiz);
    INT32 xfs_DD2name(uint16_t drv, MXFSDD *dd, char *buf, uint16_t bufsiz);
    INT32 xfs_dopendir(MAC_DIRHANDLE *dirh, uint16_t drv, MXFSDD *dd, uint16_t tosflag);
    INT32 xfs_dreaddir(MAC_DIRHANDLE *dirh, uint16_t drv,
            uint16_t bufsiz, unsigned char *buf, XATTR *xattr, INT32 *xr);
    INT32 xfs_drewinddir(MAC_DIRHANDLE *dirh, uint16_t drv);
    INT32 xfs_dclosedir(MAC_DIRHANDLE *dirh, uint16_t drv);
    INT32 xfs_dpathconf(uint16_t drv, MXFSDD *dd, uint16_t which);
    INT32 xfs_dfree(uint16_t drv, INT32 dirID, UINT32 data[4]);
    INT32 xfs_wlabel(uint16_t drv, MXFSDD *dd, const unsigned char *name);
    INT32 xfs_rlabel(uint16_t drv, MXFSDD *dd, unsigned char *name, uint16_t bufsiz);
    INT32 xfs_readlink(uint16_t drv, MXFSDD *dd, const unsigned char *name,
                    unsigned char *buf, uint16_t bufsiz);
    INT32 xfs_dcntl(uint16_t drv, MXFSDD *dd, const unsigned char *name, uint16_t cmd, void *pArg, uint8_t *addrOffset68k);
    INT32 xfs_symlink(uint16_t drv, MXFSDD *dd, const unsigned char *name, const unsigned char *target);

    // File driver

    INT32 dev_close(MAC_FD *f);
    INT32 dev_read(MAC_FD *f, INT32 count, char *buf);
    INT32 dev_write(MAC_FD *f, INT32 count, const char *buf);
    INT32 dev_stat(MAC_FD *f, void *unsel, uint16_t rwflag, INT32 apcode);
    INT32 dev_seek(MAC_FD *f, INT32 pos, uint16_t mode);
    INT32 dev_datime(MAC_FD *f, UINT16 d[2], uint16_t rwflag);
    INT32 dev_ioctl(MAC_FD *f, uint16_t cmd, void *buf);
    INT32 dev_getc( MAC_FD *f, uint16_t mode);
    INT32 dev_getline( MAC_FD *f, char *buf, INT32 size, uint16_t mode);
    INT32 dev_putc(MAC_FD *f, uint16_t mode, INT32 val);

    // auxiliar functions

    INT32 hostpath2HostFD(uint16_t drv, HostFD *reldir, uint16_t rel_hhdl, const char *path, int flags, HostHandle_t *hhdl);
    int _snext(uint16_t drv, int dir_fd, const struct dirent *entry, MAC_DTA *dta);

    void setDrivebits (uint32_t newbits, uint8_t *addrOffset68k);
};

#endif
