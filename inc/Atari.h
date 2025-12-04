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
*
* Defines constants and strutures for the emulated Atari
*
*/

#ifndef ATARI_H_INCLUDED
#define ATARI_H_INCLUDED

#include <stdint.h>

// define UINT32 et cetera
#include "osd_cpu.h"

// Make clear that all struct members are stored in
// natural order (big endian) and that Atari pointers
// occupy 32 bits.

#define UINT64_BE    UINT64
#define INT64_BE     INT64
#define UINT32_BE    UINT32
#define INT32_BE     INT32
#define UINT16_BE    UINT16
#define INT16_BE     INT16
#define PTR32_BE     UINT32         // Atari pointer

typedef uint32_t PTR32_HOST;        // host pointer, unfortunately only 32 bit, implement as jump table index
typedef uint32_t PTR32x4_HOST[4];   // might be used as 64 bit function pointer and "this" argument

// Atari struct members are NOT aligned to 32 bit!
// So we must use __attribute__((packed)) for every struct.

/* File Attributes */

#define F_RDONLY 0x01
#define F_HIDDEN 0x02
#define F_SYSTEM 0x04
#define F_VOLUME 0x08
#define F_SUBDIR 0x10
#define F_ARCHIVE 0x20

/* GEMDOS (MiNT) Fopen modes */

#define _ATARI_O_RDONLY    0
#define _ATARI_O_WRONLY    1
#define _ATARI_O_RDWR      2
#define _ATARI_O_APPEND    8
#define _ATARI_O_COMPAT    0
#define _ATARI_O_DENYRW    0x10
#define _ATARI_O_DENYW     0x20
#define _ATARI_O_DENYR     0x30
#define _ATARI_O_DENYNONE  0x40
#define _ATARI_O_CREAT     0x200
#define _ATARI_O_TRUNC     0x400
#define _ATARI_O_EXCL      0x800

/* supported Dcntl modes (Mag!X specific!) */
#define KER_GETINFO 	        0x0100
#define KER_INSTXFS             0x0200
#define KER_SETWBACK            0x0300
#define DFS_GETINFO             0x1100
#define DFS_INSTDFS             0x1200
#define DEV_M_INSTALL           0xcd00
#define CDROMEJECT              0x4309      // supported by MagicMac
#define DCNTL_VFAT_CNFDFLN      0x5600      // activate long VFAT names when mounted in future
#define DCNTL_VFAT_CNFFLN       0x5601      // activate long VFAT names when already mounted
#define DCNTL_MX_KER_DRVSTAT    0x6d04
#define DCNTL_MX_KER_XFSNAME    0x6d05

/* supported Fcntl modes */
#define FSTAT              0x4600
#define FIONREAD           0x4601
#define FIONWRITE          0x4602
#define FUTIME             0x4603
#define FTRUNCATE          0x4604

#define  TIOCGPGRP         (('T'<< 8) | 6)
#define  TIOCSPGRP         (('T'<< 8) | 7)
#define  TIOCFLUSH         (('T'<< 8) | 8)
#define  TIOCIBAUD         (('T'<< 8) | 18)
#define  TIOCOBAUD         (('T'<< 8) | 19)
#define  TIOCGFLAGS        (('T'<< 8) | 22)
#define  TIOCSFLAGS        (('T'<< 8) | 23)
#define TIOCBUFFER         (('T'<<8) | 128)
#define TIOCCTLMAP         (('T'<<8) | 129)
#define TIOCCTLGET         (('T'<<8) | 130)
#define TIOCCTLSET         (('T'<<8) | 131)



/* Modes and codes for Dpathconf() (-> MiNT) */

#define DP_MAXREQ      0xffff
#define DP_IOPEN       0
#define DP_MAXLINKS    1
#define DP_PATHMAX     2
#define DP_NAMEMAX     3
#define DP_ATOMIC      4
#define DP_TRUNC       5
#define    DP_NOTRUNC    0
#define    DP_AUTOTRUNC  1
#define    DP_DOSTRUNC   2
#define DP_CASE        6
#define    DP_CASESENS   0
#define    DP_CASECONV   1
#define    DP_CASEINSENS 2
/* New inventions from Julian: */
#define DP_MODEATTR     7
#define  DP_ATTRBITS   0x000000ff
#define  DP_MODEBITS   0x000fff00
#define  DP_FILETYPS   0xfff00000
#define   DP_FT_DIR    0x00100000
#define   DP_FT_CHR    0x00200000
#define   DP_FT_BLK    0x00400000
#define   DP_FT_REG    0x00800000
#define   DP_FT_LNK    0x01000000
#define   DP_FT_SOCK   0x02000000
#define   DP_FT_FIFO   0x04000000
#define   DP_FT_MEM    0x08000000
#define DP_XATTRFIELDS 8
#define  DP_INDEX      0x0001
#define  DP_DEV        0x0002
#define  DP_RDEV       0x0004
#define  DP_NLINK      0x0008
#define  DP_UID        0x0010
#define  DP_GID        0x0020
#define  DP_BLKSIZE    0x0040
#define  DP_SIZE       0x0080
#define  DP_NBLOCKS    0x0100
#define  DP_ATIME      0x0200
#define  DP_CTIME      0x0400
#define  DP_MTIME      0x0800

/// 16-bit time format
#define GEMDOS_TIME_SECOND 0x001f      //  0: 5 bits, 0..29, two-seconds resolution
#define GEMDOS_TIME_MINUTE 0x07e0      //  5: 6 bits, 0..59
#define GEMDOS_TIME_HOUR   0xf800      // 11: 5 bits, 0..23

/// 16-bit date format
#define GEMDOS_DATE_DAY    0x001f      //  0: 5 bits, 1..31, note that it's not 0..30
#define GEMDOS_DATE_MONTH  0x01e0      //  5: 4 bits, 1..12, note that it's not 0..11
#define GEMDOS_DATE_YEAR   0xfe00      //  9: 7 bits, 0..127 for 1980..2127

/// data for D/Fcntl(FUTIME,...)
struct mutimbuf
{
    UINT16_BE  actime;          ///< access time
    UINT16_BE  acdate;
    UINT16_BE  modtime;         ///< latest modification
    UINT16_BE  moddate;
} __attribute__((packed));

/// structure for getxattr (-> MiNT)
struct XATTR
{
    UINT16_BE mode;
    /* file types */
    #define _ATARI_S_IFMT  0170000       /* mask to select file type */
    #define _ATARI_S_IFCHR 0020000       /* BIOS special file */
    #define _ATARI_S_IFDIR 0040000       /* directory file */
    #define _ATARI_S_IFREG 0100000       /* regular file */
    #define _ATARI_S_IFIFO 0120000       /* FIFO */
    #define _ATARI_S_IMEM  0140000       /* memory region or process */
    #define _ATARI_S_IFLNK 0160000       /* symbolic link */

    /* special bits: setuid, setgid, sticky bit */
    #define _ATARI_S_ISUID 04000
    #define _ATARI_S_ISGID 02000
    #define _ATARI_S_ISVTX 01000

    /* file access modes for user, group, and other*/
    #define _ATARI_S_IRUSR 0400
    #define _ATARI_S_IWUSR 0200
    #define _ATARI_S_IXUSR 0100
    #define _ATARI_S_IRGRP 0040
    #define _ATARI_S_IWGRP 0020
    #define _ATARI_S_IXGRP 0010
    #define _ATARI_S_IROTH 0004
    #define _ATARI_S_IWOTH 0002
    #define _ATARI_S_IXOTH 0001
    #define _ATARI_DEFAULT_DIRMODE (0777)
    #define _ATARI_DEFAULT_MODE    (0666)
    UINT32_BE index;
    UINT16_BE dev;
    UINT16_BE reserved1;
    UINT16_BE nlink;
    UINT16_BE uid;
    UINT16_BE gid;
    UINT32_BE size;
    UINT32_BE blksize, nblocks;
    UINT16_BE mtime, mdate;
    UINT16_BE atime, adate;
    UINT16_BE ctime, cdate;
    UINT16_BE attr;
    UINT16_BE reserved2;
    UINT32_BE reserved3[2];
} __attribute__((packed));

/// GEMDOS basepage, not MagiC specific
struct BasePage
{
    PTR32_BE p_lowtpa;       //   0 (0x00)
    PTR32_BE p_hitpa;        //   4 (0x04)
    PTR32_BE p_tbase;        //   8 (0x08)
    UINT32_BE p_tlen;        //   12 (0x0c)
    PTR32_BE p_dbase;        //   16 (0x10)
    UINT32_BE p_dlen;        //   20 (0x14)
    PTR32_BE p_bbase;        //   24 (0x18)
    UINT32_BE p_blen;
    PTR32_BE p_dta;
    PTR32_BE p_parent;
    UINT32_BE unused1;
    PTR32_BE p_env;
    INT8 p_devx[6];
    INT8 unused2;
    INT8 p_defdrv;
    UINT32_BE unused[2+16];
    UINT8 p_cmdline[128];
} __attribute__((packed));

/// file header for GEMDOS executable files, not MagiC specific
struct ExeHeader
{
    UINT16_BE code;         // 0x00: magic value 0x601a (big endian)
    UINT32_BE tlen;         // 0x02: TEXT length (code)
    UINT32_BE dlen;         // 0x06: DATA length
    UINT32_BE blen;         // 0x0a: BSS length, not stored in file
    UINT32_BE slen;         // 0x0e: symbol table length
    INT32_BE  unused;       // 0x12:
    INT32_BE  flags;        // 0x16: see PF_xxxx
    INT16_BE  relmod;       // 0x1a: non-zero, if there is no relocation table in the file
} __attribute__((packed));

#define PF_FASTLOAD     1   // only clear BSS, not heap on process start
#define PF_TTRAMLOAD    2   // program may be loaded to TT RAM
#define PF_TTRAMMEM     4   // programm may get TT RAM on Malloc()
#define PF_PROT      0x18   // protection mode

/// BIOS Parameter Block for a mounted volume
struct BPB
{
    UINT16_BE b_recsiz;     // 0x00: bytes per sector
    UINT16_BE b_clsiz;      // 0x02: sectors per cluster
    UINT16_BE b_clsizb;     // 0x04: bytes per cluster (redundant)
    UINT16_BE b_rdlen;      // 0x06: sectors for root directory
    UINT16_BE b_fsiz;       // 0x08: sectors per FAT (file allocation table), zero for FAT32
    UINT16_BE b_fatrec;     // 0x0a: sector number of second FAT
    UINT16_BE b_datrec;     // 0x0c: sector number of first data cluster
    UINT16_BE b_numcl;      // 0x0e: number of data clusters. If more than 65525, then FAT32
    UINT16_BE b_flags[8];   // 0x10: bit 0 of flag 0: 0=FAT12, 1=FAT16, bit 1: single FAT
};


/// kbshift bits
#define KBSHIFT_SHIFT_RIGHT     0x01
#define KBSHIFT_SHIFT_LEFT      0x02
#define KBSHIFT_CTRL            0x04
#define KBSHIFT_ALT             0x08
#define KBSHIFT_CAPS_LOCK       0x10
#define KBSHIFT_MBUTTON_RIGHT   0x20
#define KBSHIFT_MBUTTON_LEFT    0x40
#define KBSHIFT_ALTGR           0x80    // Milan TOS 4.06


/// The system variable _sysbase (0x4F2L) points to this structure,
/// which is not MagiC specific.
struct SYSHDR
{
    UINT16_BE os_entry;     /* $00 BRA to reset handler             */
    UINT16_BE os_version;   /* $02 TOS version number               */
    UINT32_BE os_start;     /* $04 -> reset handler                 */
    UINT32_BE os_base;      /* $08 -> baseof OS                     */
    UINT32_BE os_membot;    /* $0c -> end BIOS/GEMDOS/VDI ram usage */
    UINT32_BE os_rsv1;      /* $10 << unused,reserved >>            */
    UINT32_BE os_magic;     /* $14 -> GEM memory usage parm. block   */
    UINT32_BE os_gendat;    /* $18 Date of system build($MMDDYYYY)  */
    UINT16_BE os_palmode;   /* $1c OS configuration bits            */
    UINT16_BE os_gendatg;   /* $1e DOS-format date of system build   */
    /*
     * These members were added with TOS 1.2:
    */
    UINT32_BE _root;        /* $20 -> base of OS pool               */
    UINT32_BE kbshift;      /* $24 -> 68 address of Atari system variable "kbshift" and "kbrepeat" */
    UINT32_BE _run;         /* $28 -> GEMDOS PID of current process */
    UINT32_BE p_rsv2;       /* $2c << unused, reserved >>           */
} __attribute__((packed));

/* interrupt vectors */

#define INTV_0_RESET_SSP        0x000    /* ssp on reset */
#define INTV_1_RESET_PC         0x004    /* pc on reset */
#define INTV_2_BUS_ERROR        0x008    /* bus fault */
#define INTV_3_ADDRESS_ERROR    0x00c    /* address fault */
#define INTV_4_ILLEGAL          0x010    /* invalid CPU opcode */
#define INTV_5_DIV_BY_ZERO      0x014    /* integer division by zero */
#define INTV_6_CHK              0x018    /* chk opcode */
#define INTV_7_TRAPV            0x01c    /* trapv instruction */
#define INTV_8_PRIV_VIOL        0x020    /* privilege violation */
#define INTV_9_TRACE            0x024    /* trace */
#define INTV_10_LINE_A          0x028    /* LineA opcode */
#define INTV_11_LINE_F          0x02c    /* LineF opcode */
#define INTV_12                 0x030    /* reserved */
#define INTV_13                 0x034    /* reserved */
#define INTV_14                 0x038    /* reserved */
#define INTV_15                 0x03c    /* reserved */
#define INTV_16                 0x040    /* reserved */
#define INTV_17                 0x044    /* reserved */
#define INTV_18                 0x048    /* reserved */
#define INTV_19                 0x04c    /* reserved */
#define INTV_20                 0x050    /* reserved */
#define INTV_21                 0x054    /* reserved */
#define INTV_22                 0x058    /* reserved */
#define INTV_23                 0x05c    /* reserved */
#define INTV_24_SPURIOUS        0x060    /* interrupt of unknown reason */
#define INTV_25_AUTV_1          0x064    /* not used by Atari */
#define INTV_26_AUTV_2          0x068    /* ST: Hblank */
#define INTV_27_AUTV_3          0x06c    /* not used by Atari */
#define INTV_28_AUTV_4          0x070    /* ST: VBlank */
#define INTV_29_AUTV_5          0x074    /* not used by Atari */
#define INTV_30_AUTV_6          0x078    /* not used by Atari */
#define INTV_31_AUTV_7          0x07c    /* not used by Atari */
#define INTV_32_TRAP_0          0x080    /* Trap #0 */
#define INTV_33_TRAP_1          0x084    /* Trap #1 */
#define INTV_34_TRAP_2          0x088    /* Trap #2 */
#define INTV_35_TRAP_3          0x08c    /* Trap #3 */
#define INTV_36_TRAP_4          0x090    /* Trap #4 */
#define INTV_37_TRAP_5          0x094    /* Trap #5 */
#define INTV_38_TRAP_6          0x098    /* Trap #6 */
#define INTV_39_TRAP_7          0x09c    /* Trap #7 */
#define INTV_40_TRAP_8          0x0a0    /* Trap #8 */
#define INTV_41_TRAP_9          0x0a4    /* Trap #9 */
#define INTV_42_TRAP_10         0x0a8    /* Trap #10 */
#define INTV_43_TRAP_11         0x0ac    /* Trap #11 */
#define INTV_44_TRAP_12         0x0b0    /* Trap #12 */
#define INTV_45_TRAP_13         0x0b4    /* Trap #13 */
#define INTV_46_TRAP_14         0x0b8    /* Trap #14 */
#define INTV_47_TRAP_15         0x0bc    /* Trap #15 */
#define INTV_48                 0x0c0    /* reserved */
#define INTV_49                 0x0c4    /* reserved */
#define INTV_50                 0x0c8    /* reserved */
#define INTV_51                 0x0cc    /* reserved */
#define INTV_52                 0x0d0    /* reserved */
#define INTV_53                 0x0d4    /* reserved */
#define INTV_54                 0x0d8    /* reserved */
#define INTV_55                 0x0dc    /* reserved */
#define INTV_56                 0x0e0    /* reserved */
#define INTV_57                 0x0e4    /* reserved */
#define INTV_58                 0x0e8    /* reserved */
#define INTV_59                 0x0ec    /* reserved */
#define INTV_60                 0x0f0    /* reserved */
#define INTV_61                 0x0f4    /* reserved */
#define INTV_62                 0x0f8    /* reserved */
#define INTV_63                 0x0fc    /* reserved */
#define INTV_MFP0_CENTBUSY      0x100    /* centronics busy */
#define INTV_MFP1_DCD           0x104    /* rs232 carrier detect */
#define INTV_MFP2_CTS           0x108    /* rs232 clear to send */
#define INTV_MFP3_GPU_DONE      0x10c    /* blitter */
#define INTV_MFP4_BAUDGEN       0x110    /* baud rate generator */
#define INTV_MFP5_HZ200         0x114    /* 200Hz timer */
#define INTV_MFP6_IKBD_MIDI     0x118    /* IKBD/MIDI */
#define INTV_MFP7_FDC_ACSI      0x11c    /* FDC/ACSI */
#define INTV_MFP8               0x120    /* display enable (?) */
#define INTV_MFP9_TX_ERR        0x124    /* transmission fault RS232 */
#define INTV_MFP10_SND_EMPT     0x128    /* RS232 transmission buffer empty */
#define INTV_MFP11_RX_ERR       0x12c    /* receive fault RS232 */
#define INTV_MFP12_RCV_FULL     0x130    /* RS232 receive buffer full */
#define INTV_MFP13              0x134    /* unused */
#define INTV_MFP14_RING_IND     0x138    /* RS232: incoming call */
#define INTV_MFP15_MNCHR        0x13c    /* monochrome monitor detect */

/* System variables */

#define proc_lives              0x380       // Validates System Crash Page if 0x12345678
#define proc_regs               0x384       // Saved 68k registers d0-d7,a0-a7
#define proc_pc                 0x3c4       // Vector number of crash exception
#define proc_usp                0x3c8       // Save USP
#define proc_stk                0x3cc       // Saved 16 16-bit words from exception stack
#define etv_timer               0x400       // GEM Event timer vector
#define etv_critic              0x404       // GEM Critical error handler
#define etv_term                0x408       // GEM Program termination vector
#define etv_xtra                0x40c       // 5 GEM Additional vectors (unused)
#define memvalid                0x420       // Validates memory configuration if 0x752019F3
#define memctrl                 0x424       // Copy of contents of 0xFF8001
#define resvalid                0x426       // Validates resvector if $31415926
#define resvector               0x42a       // Reset vector
#define phystop                 0x42e       // Physical top of RAM
#define _membot                 0x432       // Start of TPA (user memory)
#define _memtop                 0x436       // End of TPA (user memory)
#define memval2                 0x43a       // Validates memcntrl and memconf if 0x237698AA
#define flock                   0x43e       // If nonzero, floppy disk VBL routine is disabled
#define seekrate                0x440       // Floppy Seek rate - 0:6ms, 1:12ms, 2:2ms, 3:3ms
#define _timer_ms               0x442       // Time between two timer calls (in milliseconds)
#define _fverify                0x444       // If not zero, verify floppy disk writes
#define _bootdev                0x446       // Default boot device
#define palmode                 0x448       // 0 - NTSC (60hz), <>0 - PAL (50hz) (unused in emulator)
#define defshiftmd              0x44a       // Default video resolution
#define sshiftmd                0x44c       // Byte: Copy of contents of 0xFF8260 (0: ST Low, 1: ST Mid, 2: ST High)
#define _v_bas_ad               0x44e       // Pointer to video RAM (logical screen base)
#define vblsem                  0x452       // If not zero, VBL routine is not executed
#define nvbls                   0x454       // Number of vertical blank routines
#define _vblqueue               0x456       // Pointer to list of vertical blank routines
#define colorptr                0x45a       // If not zero, points to color palette to be loaded
#define screenpt                0x45e       // If not zero, points to video ram for next VBL
#define _vbclock                0x462       // Counter for number of VBLs
#define _frclock                0x466       // Number of VBL routines executed
#define hdv_init                0x46a       // Vector for hard disk initialisation
#define swv_vec                 0x46e       // Vector for resolution change
#define hdv_bpb                 0x472       // Vector for getbpb for hard disk
#define hdv_rw                  0x476       // Vector for read/write routine for hard disk
#define hdv_boot                0x47a       // Vector for hard disk boot
#define hdv_mediach             0x47e       // Vector for hard disk media change
#define _cmdload                0x482       // If not zero, attempt to load "COMMAND.PRG" on boot
#define conterm                 0x484       // Attribute vector for console output (bit 0: click, bit 1: repeat, bit 2: bell, 3: return kbshift on BIOS conin)
#define trp14ret                0x486       // Return address for TRAP #14 (unused)
#define os_chksum               trp14ret    // Mag!X: checksum over system
#define criticret               0x48a       // MagiC 6.01: DOS-Event-Critic is active
#define themd                   0x48e       // Memory descriptor block
#define ____md                  0x49e       // Space for additional memory descriptors (unused)
#define fstrm_beg               ____md      // Mag!X: begin of TT RAM
#define savptr                  0x4a2       // Pointer to BIOS save registers block
#define _nflops                 0x4a6       // Number of connected floppy disk drives (usually 0..2)
#define con_state               0x4a8       // Vector for screen output (here unused)
#define save_row                0x4ac       // Temporary storage for cursor line position (here unused)
#define sav_context             0x4ae       // Pointer to save area for exception processing (here unused)
#define _bufl                   0x4b2       // 2 Pointers to buffer control block for GEMDOS data and fat/dir (here unused)
#define _hz_200                 0x4ba       // Counter for 200hz system clock
#define the_env                 0x4be       // Pointer to default environment string (here unused)
#define _drvbits                0x4c2       // Bit allocation for physical drives (bit 0=A, 1=B..)
#define _dskbufp                0x4c6       // pointer to a 4096-byte (TOS: 1024-byte) disk buffer, used by BIOS GetBpb()
#define _autopath               0x4ca       // Pointer to autoexecute path (here unused)
#define _vbl_list               0x4ce       // 8 Pointers to VBL routines
#define _dumpflg                0x4ee       // Flag for screen -> printer dump
#define _prtabt                 0x4f0       // Printer abort flag (here unused)
#define _sysbase                0x4f2       // Pointer to start of OS
#define _shell_p                0x4f6       // Global shell pointer (here unused)
#define end_os                  0x4fa       // Pointer to end of OS
#define exec_os                 0x4fe       // Pointer to entry point of OS
#define scr_dump                0x502       // Pointer to screen dump routine
#define prv_lsto                0x506       // Pointer to _lstostat()
#define prv_lst                 0x50a       // Pointer to _lstout()
#define prv_auxo                0x50e       // Pointer to _auxostat()
#define prv_aux                 0x512       // Pointer to _auxout()
#define pun_ptr                 0x516       // If AHDI, pointer to pun_info (here unused)
#define memval3                 0x51a       // If $5555AAAA, reset
#define dev_vecs                0x51e       // UINT32_BE dev_vecs[8*4], 4*8 Pointers to input/output/status routines
#define cpu_typ                 0x59e       // UINT16_BE cpu_typ, If not 0, then not 68000 - use long stack frames
#define _p_cookies              0x5a0       // PTR32_BE, Atari pointer to cookie
#define fstrm_top               0x5a4       // Pointer to end of FastRam
#define fstrm_valid             0x5a8       // Validates ramtop if 0x1357BD13
#define bell_hook               0x5ac       // PTR32_BE, Atari pointer to pling
#define kcl_hook                0x5b0       // PTR32_BE, Atari pointer to key-klick

/* BIOS level errors */

#define E_OK        0L    // OK, no error
#define ERROR      -1L    // basic, fundamental error
#define EDRVNR     -2L    // drive not ready
#define EUNCMD     -3L    // unknown command
#define E_CRC      -4L    // CRC error
#define EBADRQ     -5L    // bad request
#define E_SEEK     -6L    // seek error
#define EMEDIA     -7L    // unknown media
#define ESECNF     -8L    // sector not found
#define EPAPER     -9L    // no paper
#define EWRITF    -10L    // write fault
#define EREADF    -11L    // read fault
#define EGENRL    -12L    // general error
#define EWRPRO    -13L    // write protect
#define E_CHNG    -14L    // media change
#define EUNDEV    -15L    // unknown device
#define EBADSF    -16L    // bad sectors on format
#define EOTHER    -17L    // insert other disk

/* BDOS level errors */

#define EINVFN    -32L    // invalid function number          1
#define EFILNF    -33L    // file not found                   2
#define EPTHNF    -34L    // path not found    (0xffde)       3
#define ENHNDL    -35L    // no handles left                  4
#define EACCDN    -36L    // access denied                    5
#define EIHNDL    -37L    // invalid handle                   6
#define ENSMEM    -39L    // insufficient memory              8
#define EIMBA     -40L    // invalid memory block address     9
#define EDRIVE    -46L    // invalid drive was specified     15
#define ENSAME    -48L    // MV between two different drives 17
#define ENMFIL    -49L    // no more files                   18
#define ATARIERR_ERANGE    -64L    // range error            33
#define EINTRN    -65L    // internal error                  34
#define EPLFMT    -66L    // invalid program load format     35
#define EGSBF     -67L    // setblock failure                36
#define EBREAK    -68L    // user break (^C)                 37
#define EXCPT     -69L    // 68000- exception ("bombs")      38
#define EPTHOV    -70L    // path overflow             MAG!X

// Keyboard Scancodes

// key                                    scancode        // international
#define ATARI_KBD_SCANCODE_ESCAPE       1
#define ATARI_KBD_SCANCODE_1            2
#define ATARI_KBD_SCANCODE_2            3
#define ATARI_KBD_SCANCODE_3            4
#define ATARI_KBD_SCANCODE_4            5
#define ATARI_KBD_SCANCODE_5            6
#define ATARI_KBD_SCANCODE_6            7
#define ATARI_KBD_SCANCODE_7            8
#define ATARI_KBD_SCANCODE_8            9
#define ATARI_KBD_SCANCODE_9            10
#define ATARI_KBD_SCANCODE_0            11
#define ATARI_KBD_SCANCODE_MINUS        12                // de: 'ß'  us: '-'
#define ATARI_KBD_SCANCODE_EQUALS       13                // de: '`'  us: '='
#define ATARI_KBD_SCANCODE_BACKSPACE    14
#define ATARI_KBD_SCANCODE_TAB          15
#define ATARI_KBD_SCANCODE_Q            16
#define ATARI_KBD_SCANCODE_W            17
#define ATARI_KBD_SCANCODE_E            18
#define ATARI_KBD_SCANCODE_R            19
#define ATARI_KBD_SCANCODE_T            20
#define ATARI_KBD_SCANCODE_Y            21                // de: 'Z'  us: 'Y'
#define ATARI_KBD_SCANCODE_U            22
#define ATARI_KBD_SCANCODE_I            23
#define ATARI_KBD_SCANCODE_O            24
#define ATARI_KBD_SCANCODE_P            25
#define ATARI_KBD_SCANCODE_LEFTBRACKET  26                // de: 'ü' us: '['
#define ATARI_KBD_SCANCODE_RIGHTBRACKET 27                // de: '+' us: ']'
#define ATARI_KBD_SCANCODE_RETURN       28
#define ATARI_KBD_SCANCODE_CONTROL      29
#define ATARI_KBD_SCANCODE_A            30
#define ATARI_KBD_SCANCODE_S            31
#define ATARI_KBD_SCANCODE_D            32
#define ATARI_KBD_SCANCODE_F            33
#define ATARI_KBD_SCANCODE_G            34
#define ATARI_KBD_SCANCODE_H            35
#define ATARI_KBD_SCANCODE_J            36
#define ATARI_KBD_SCANCODE_K            37
#define ATARI_KBD_SCANCODE_L            38
#define ATARI_KBD_SCANCODE_SEMICOLON    39                // de: 'ö'  us: ';'
#define ATARI_KBD_SCANCODE_APOSTROPHE   40                // de: 'ä'  us: '''
#define ATARI_KBD_SCANCODE_NUMBER       41                // de: '#'
#define ATARI_KBD_SCANCODE_LSHIFT       42
#define ATARI_KBD_SCANCODE_BACKSLASH    43                // de: --   us: '\'
#define ATARI_KBD_SCANCODE_Z            44                // de: 'Y'  us: 'Z'
#define ATARI_KBD_SCANCODE_X            45
#define ATARI_KBD_SCANCODE_C            46
#define ATARI_KBD_SCANCODE_V            47
#define ATARI_KBD_SCANCODE_B            48
#define ATARI_KBD_SCANCODE_N            49
#define ATARI_KBD_SCANCODE_M            50
#define ATARI_KBD_SCANCODE_COMMA        51
#define ATARI_KBD_SCANCODE_PERIOD       52
#define ATARI_KBD_SCANCODE_SLASH        53                // de: '-'  us: '/'
#define ATARI_KBD_SCANCODE_RSHIFT       54
// 55
#define ATARI_KBD_SCANCODE_ALT          56
#define ATARI_KBD_SCANCODE_SPACE        57
#define ATARI_KBD_SCANCODE_CAPSLOCK     58
#define ATARI_KBD_SCANCODE_F1           59
#define ATARI_KBD_SCANCODE_F2           60
#define ATARI_KBD_SCANCODE_F3           61
#define ATARI_KBD_SCANCODE_F4           62
#define ATARI_KBD_SCANCODE_F5           63
#define ATARI_KBD_SCANCODE_F6           64
#define ATARI_KBD_SCANCODE_F7           65
#define ATARI_KBD_SCANCODE_F8           66
#define ATARI_KBD_SCANCODE_F9           67
#define ATARI_KBD_SCANCODE_F10          68
// 69
// 70
#define ATARI_KBD_SCANCODE_CLRHOME      71
#define ATARI_KBD_SCANCODE_UP           72
#define ATARI_KBD_SCANCODE_PAGEUP       73                // not on Atari keyboard
#define ATARI_KBD_SCANCODE_KP_MINUS     74
#define ATARI_KBD_SCANCODE_LEFT         75
#define ATARI_KBD_SCANCODE_ALTGR        76                // not on Atari keyboard
#define ATARI_KBD_SCANCODE_RIGHT        77
#define ATARI_KBD_SCANCODE_KP_PLUS      78
#define ATARI_KBD_SCANCODE_END          79                // not on Atari keyboard
#define ATARI_KBD_SCANCODE_DOWN         80
#define ATARI_KBD_SCANCODE_PAGEDOWN     81                // not on Atari keyboard
#define ATARI_KBD_SCANCODE_INSERT       82
#define ATARI_KBD_SCANCODE_DELETE       83
#define ATARI_KBD_SCANCODE_SHIFT_F1     84
#define ATARI_KBD_SCANCODE_SHIFT_F2     85
#define ATARI_KBD_SCANCODE_SHIFT_F3     86
#define ATARI_KBD_SCANCODE_SHIFT_F4     87
#define ATARI_KBD_SCANCODE_SHIFT_F5     88
#define ATARI_KBD_SCANCODE_SHIFT_F6     89
#define ATARI_KBD_SCANCODE_SHIFT_F7     90
#define ATARI_KBD_SCANCODE_SHIFT_F8     91
#define ATARI_KBD_SCANCODE_SHIFT_F9     92
#define ATARI_KBD_SCANCODE_SHIFT_F10    93
// 94
// 95
#define ATARI_KBD_SCANCODE_LTGT         96                // de: '<>'
#define ATARI_KBD_SCANCODE_UNDO         97
#define ATARI_KBD_SCANCODE_HELP         98
#define ATARI_KBD_SCANCODE_KP_LPAREN    99                // '('
#define ATARI_KBD_SCANCODE_KP_RPAREN    100                // ')'
#define ATARI_KBD_SCANCODE_KP_DIVIDE    101                // '/'
#define ATARI_KBD_SCANCODE_KP_MULTIPLY  102                // '*'
#define ATARI_KBD_SCANCODE_KP_7         103
#define ATARI_KBD_SCANCODE_KP_8         104
#define ATARI_KBD_SCANCODE_KP_9         105
#define ATARI_KBD_SCANCODE_KP_4         106
#define ATARI_KBD_SCANCODE_KP_5         107
#define ATARI_KBD_SCANCODE_KP_6         108
#define ATARI_KBD_SCANCODE_KP_1         109
#define ATARI_KBD_SCANCODE_KP_2         110
#define ATARI_KBD_SCANCODE_KP_3         111
#define ATARI_KBD_SCANCODE_KP_0         112
#define ATARI_KBD_SCANCODE_KP_PERIOD    113
#define ATARI_KBD_SCANCODE_KP_ENTER     114
// 115
// 116
// 117
// 118
// 119
#define ATARI_KBD_SCANCODE_ALT_1        120
#define ATARI_KBD_SCANCODE_ALT_2        121
#define ATARI_KBD_SCANCODE_ALT_3        122
#define ATARI_KBD_SCANCODE_ALT_4        123
#define ATARI_KBD_SCANCODE_ALT_5        124
#define ATARI_KBD_SCANCODE_ALT_6        125
#define ATARI_KBD_SCANCODE_ALT_7        126
#define ATARI_KBD_SCANCODE_ALT_8        127
#define ATARI_KBD_SCANCODE_ALT_9        128
#define ATARI_KBD_SCANCODE_ALT_0        129
#define ATARI_KBD_SCANCODE_ALT_MINUS    130
#define ATARI_KBD_SCANCODE_ALT_EQUAL    131
// 132


/// XCmd Commands
enum eXCMD
{
    eXCMDVersion = 0,
    eXCMDMaxCmd = 1,
    eXCMDLoadByPath = 10,
    eXCMDLoadByLibName = 11,
    eXCMDGetSymbolByName = 12,
    eXCMDGetSymbolByIndex = 13,
    eUnLoad = 14
};


/// Format for XCmd commands
struct strXCMD
{
    UINT32_BE m_cmd;            // ->    command
    UINT32_BE m_LibHandle;      // <->    Connection-ID (IN oder OUT, depending on command)
    UINT32_BE m_MacError;       // ->    Mac error code
    union
    {
        struct
        {
            char m_PathOrName[256];     // ->    path (command 10) or name
            INT32_BE m_nSymbols;        // <-    number of symbols during open
        } m_10_11;
        struct
        {
            UINT32_BE m_Index;          // ->    index (command 13)
            char m_Name[256];           // ->    symbol name (command 12)
                                        // <-    symbol name (command 13)
            UINT32_BE m_SymPtr;         // <-    Pointer to symbol
            UINT8 m_SymClass;           // <-    type of symbol
        } m_12_13;
    };
} __attribute__((packed));


/*

    MagicMac:

    The callbacks from Atari sandbox to host originally were defined for
    an 68k host. So the host was also a big-endian 32-bit machine.
    A simple function callback occupied one 32-bit pointer.
    A class method callback occupied three 32-bit pointers, for whatever
    reason, maybe compiler related or for virtual functions,
    and a 32-bit object pointer ("this").

    MagicMacX on PPC:

    The PPC is a big-endian 64-bit machine, but ran in 32-bit mode.
    A simple function callback occupies one 32-bit pointer.
    A class method callback occupies two 32-bit pointers (for
    backward compatibility to MagicMac a dummy word has been added),
    and a 32-bit object pointer ("this").

    AtariX on x86:

    The x86 is a little-endian 64-bit machine, but ran in 32-bit mode.
    Apart from endianess, the callback struct remains the same as for PPC.

    MagiCLinux:

    The host (usually little-endian) is a 64-bit machine.
    A simple function callback occupies one 64-bit pointer and does not
    fit into the old structure. For an elegant solution, the 68k code
    would have been adapted for the larger callback structs.
    Instead, we introduce a hack, using a jump table and access functions
    setCallback() and getCallback().

*/

/*
struct CPPCCallback
{
    UINT32 (*Callback)(void *params1, void *params2, unsigned char *AdrOffset68k);
    void *params1;
};
*/


/// @brief Static function host callback with two parameters
typedef UINT32 tfHostCallback(UINT32 params, uint8_t *AdrOffset68k);
void setHostCallback(PTR32_HOST *dest, tfHostCallback callback);


/// @brief CMagiC method host callback
class CMagiC;
typedef UINT32 (CMagiC::*tpfCMagiC_HostCallback)(UINT32 params, uint8_t *AdrOffset68k);
void setCMagiCHostCallback(PTR32x4_HOST *dest, tpfCMagiC_HostCallback callback, CMagiC *pthis);

/// @brief CXCmd method host callback
class CXCmd;
typedef INT32 (CXCmd::*tpfCXCmd_HostCallback)(UINT32 params, uint8_t *AdrOffset68k);
void setCXCmdHostCallback(PTR32x4_HOST *dest, tpfCXCmd_HostCallback callback, CXCmd *pthis);

/// @brief CHostXFS method host callback
class CHostXFS;
typedef INT32 (CHostXFS::*tpfCHostXFS_HostCallback)(UINT32 params, uint8_t *AdrOffset68k);
void setCHostXFSHostCallback(PTR32x4_HOST *dest, tpfCHostXFS_HostCallback callback, CHostXFS *pthis);



typedef UINT32 (*PPCCallback)(UINT32 params, uint8_t *AdrOffset68k);
//typedef UINT32 (CMagiC::*PPCCallback)(void *params, unsigned char *AdrOffset68k);



// occupies four 32-bit words

#if __UINTPTR_MAX__ == 0xFFFFFFFFFFFFFFFF

// variant for 64-bit hosts
struct CMagiC_CPPCCallback
{
    uint32_t data[4];
} __attribute__((packed));

#else

// variant for 32-bit hosts
class CMagiC;
struct CMagiC_CPPCCallback
{
    typedef UINT32 (CMagiC::*CMagiC_PPCCallback)(UINT32 params, uint8_t *AdrOffset68k);
    CMagiC_PPCCallback m_Callback;
    #if defined(__GNUC__)
    UINT32 dummy;
    #endif
    CMagiC *m_thisptr;
} __attribute__((packed));


class CMacXFS;
struct CMacXFS_CPPCCallback
{
    typedef INT32 (CMacXFS::*CMacXFS_PPCCallback)(UINT32 params, uint8_t *AdrOffset68k);
    CMacXFS_PPCCallback m_Callback;
    #if defined(__GNUC__)
    UINT32 dummy;
    #endif
    CMacXFS *m_thisptr;
} __attribute__((packed));

class CXCmd;
struct CXCmd_CPPCCallback
{
    typedef INT32 (CXCmd::*CXCmd_PPCCallback)(UINT32 params, uint8_t *AdrOffset68k);
    CXCmd_PPCCallback m_Callback;        // gcc: 2 words, mwc: 3 words
    #if defined(__GNUC__)
    UINT32 dummy;
    #endif
    CXCmd *m_thisptr;
} __attribute__((packed));

#endif

/// screen and video format description for MXVDI
typedef struct
{
    PTR32_BE  baseAddr;         // pointer to pixels
    UINT16_BE rowBytes;         // offset to next line
//  Rect      bounds;           // encloses bitmap
    UINT16_BE bounds_top;       // seems to be ignored, set to zero
    UINT16_BE bounds_left;      // seems to be ignored, set to zero
    UINT16_BE bounds_bottom;    // set to number of pixel rows
    UINT16_BE bounds_right;     // set to number of pixel columns
    UINT16_BE pmVersion;        // pixMap version number
    UINT16_BE packType;         // defines packing format
    UINT32_BE packSize;         // length of pixel data, seems to be ignored
    INT32_BE  hRes;             // horiz. resolution (ppi), in fact of type "Fixed"
    INT32_BE  vRes;             // vert. resolution (ppi), in fact of type "Fixed"
    UINT16_BE pixelType;        // defines pixel type
    UINT16_BE pixelSize;        // # bits in pixel
    UINT16_BE cmpCount;         // # components in pixel
    UINT16_BE cmpSize;          // # bits per component
    UINT32_BE planeBytes;       // offset to next plane
    PTR32_BE  pmTable;          // colour map for this pixMap, in fact of type CTabHandle
    UINT32_BE pmReserved;       // for future use. MUST BE 0
} __attribute__((packed)) MXVDI_PIXMAP;

/// The old MagiCMac system header which is now included in the
/// new MacXSysHdr, for compatibility purposes with Behnes' MXVDI.
struct OldMmSysHdr
{
    UINT32_BE magic;            // is 'MagC'
    UINT32_BE syshdr;           // address of the Atari Syshdr structure
    UINT32_BE keytabs;          // 5*128 Bytes for keyboard tables
    UINT32_BE ver;              // version
    UINT16_BE cpu;              // CPU (30=68030, 40=68040)
    UINT16_BE fpu;              // FPU (0=none, 4=68881, 6=68882, 8=68040)
    UINT32_BE boot_sp;          // sp for booting
    UINT32_BE biosinit;         // to be called after initialisation
    UINT32_BE pixmap;           // data for the VDI
    UINT32_BE offs_32k;         // address offset for first 32k in the Mac
    UINT32_BE a5;               // global 68k register a5 for Mac programs
    UINT32_BE tasksw;           // != NULL, if task switch is necessary
    UINT32_BE gettime;          // get date and time
    UINT32_BE bombs;            // Atari function, called by the Mac
    UINT32_BE syshalt;          // "System halted", text string in 68k register a0
    UINT32_BE coldboot;
    UINT32_BE debugout;         // for debugging
    UINT32_BE prtis;            //    for printer (PRT)
    UINT32_BE prtos;            //
    UINT32_BE prtin;            //
    UINT32_BE prtout;           //
    UINT32_BE serconf;          //    Rsconf for ser1
    UINT32_BE seris;            //    for ser1 (AUX)
    UINT32_BE seros;            //
    UINT32_BE serin;            //
    UINT32_BE serout;           //
    UINT32_BE xfs;              // functions for the XFS (Mac file system driver)
    UINT32_BE xfs_dev;          //  corresponding file driver
    UINT32_BE set_physbase;     // change video memory address in Setscreen() (a0 points to stack of Setscreen())
    UINT32_BE VsetRGB;          // set colour (a0 points to the stack of VsetRGB())
    UINT32_BE VgetRGB;          // ask colour (a0 points to the stack of VgetRGB())
    UINT32_BE error;            // pass error message in 68k register d0.l to the MacOS
                                //    error messages for MacSys_error:
                                //    -1: unsupported graphics format => no VDI driver
    UINT32_BE init;             // called on Atari warmboot
    UINT32_BE drv2devcode;      // convert drive number to device number
    UINT32_BE rawdrvr;          // Raw driver (eject) for Mac
    UINT32_BE floprd;
    UINT32_BE flopwr;
    UINT32_BE flopfmt;
    UINT32_BE flopver;
    UINT32_BE superstlen;        // size of supervisor stack per application
    UINT32_BE dos_macfn;         // DOS functions 0x60..0xfe
    UINT32_BE settime;           // xbios Settime
    UINT32_BE prn_wrts;          // text string to printer
    UINT32_BE version;           // version number of this structure
    UINT32_BE in_interrupt;      // interrupt counter for the MacOS side
    UINT32_BE drv_fsspec;        // List of FSSpec data for Mac drives
    UINT32_BE cnverr;            // LONG cnverr( WORD mac_errcode )
    UINT32_BE res1;              // reserved
    UINT32_BE res2;              // reserved
    UINT32_BE res3;              // reserved
} __attribute__((packed));

/// The Cookie structure is provided by the emulator. Its address
/// is passed to the kernel via the information transfer structure.
/// Except the first three ones, the other structure members are
/// written by the kernel.
struct MgMxCookieData
{
    UINT32_BE mgmx_magic;        // is "MgMx"
    UINT32_BE mgmx_version;      // version number
    UINT32_BE mgmx_len;          // structure length
    UINT32_BE mgmx_xcmd;         // load and manage PPC (Mac PowerPC CPU) libraries
    UINT32_BE mgmx_xcmd_exec;    // call PPC function from a PPC library
    UINT32_BE mgmx_internal;     // 68k address of the information transfer structure
    UINT32_BE mgmx_daemon;       // function call for the "mmx.prg" background (idle) process
} __attribute__((packed));

/*
PTRLEN    EQU    4        ; Zeiger auf Elementfunktion braucht 4 Zeiger

    OFFSET

;Atari -> Mac
MacSysX_magic:          DS.L 1        ; is 'MagC'
MacSysX_len:            DS.L 1        ; length of this structure
MacSysX_syshdr:         DS.L 1        ; address of Atari Syshdr
MacSysX_keytabs:        DS.L 1        ; 5*128 bytes for keyboard tables
MacSysX_mem_root:       DS.L 1        ; memory lists
MacSysX_act_pd:         DS.L 1        ; pointer to current process
MacSysX_act_appl:       DS.L 1        ; pointer to current task (appl)
MacSysX_verAtari:       DS.L 1        ; version number of MagicMacX.OS
;Mac -> Atari
MacSysX_verMac:         DS.L 1        ; version number of this structure
MacSysX_cpu:            DS.W 1        ; CPU (30=68030, 40=68040)
MacSysX_fpu:            DS.W 1        ; FPU (0=nix,4=68881,6=68882,8=68040)
MacSysX_init:           DS.L PTRLEN   ; called on Atari warm boot
MacSysX_biosinit:       DS.L PTRLEN   ; called after initialisation
MacSysX_VdiInit:        DS.L PTRLEN   ; called after initialisation of VDI
MacSysX_pixmap:         DS.L 1        ; data for VDI
MacSysX_pMMXCookie:     DS.L 1        ; 68k pointer to MgMx cookie
MacSysX_Xcmd:           DS.L PTRLEN   ; XCMD commands
MacSysX_PPCAddr:        DS.L 1        ; actual host address of 68k address 0 (on 64-bit host: 0x00000000)
MacSysX_VideoAddr:      DS.L 1        ; actual host address of video memory (on 64-bit host: 0x80000000)
MacSysX_Exec68k:        DS.L PTRLEN   ; here the host callback can run 68k code
MacSysX_gettime:        DS.L 1        ; LONG GetTime(void): get date and time
MacSysX_settime:        DS.L 1        ; void SetTime(LONG *time): set date and time
MacSysX_Setpalette:     DS.L 1        ; void Setpalette( int ptr[16] )
MacSysX_Setcolor:       DS.L 1        ; int Setcolor( int nr, int val )
MacSysX_VsetRGB:        DS.L 1        ; void VsetRGB( WORD index, WORD count, LONG *array )
MacSysX_VgetRGB:        DS.L 1        ; void VgetRGB( WORD index, WORD count, LONG *array )
MacSysX_syshalt:        DS.L 1        ; SysHalt( char *str ) "System halted"
MacSysX_syserr:         DS.L 1        ; SysErr( long val ) "a1 = 0 => bombs"
MacSysX_coldboot:       DS.L 1        ; ColdBoot(void) perform cold boot
MacSysX_exit:           DS.L 1        ; Exit(void) end program
MacSysX_debugout:       DS.L 1        ; MacPuts( char *str ) for debugging
MacSysX_error:          DS.L 1        ; d0 = -1: no graphics driver
MacSysX_prtos:          DS.L 1        ; Bcostat(void) for PRT
MacSysX_prtin:          DS.L 1        ; Cconin(void) for PRT
MacSysX_prtout:         DS.L 1        ; Cconout( void *params ) for PRT
MacSysX_prn_wrts:       DS.L 1        ; LONG PrnWrts({char *buf, LONG count}) character string to printer
MacSysX_serconf:        DS.L 1        ; Rsconf( void *params ) for ser1
MacSysX_seris:          DS.L 1        ; Bconstat(void) for ser1 (AUX)
MacSysX_seros:          DS.L 1        ; Bcostat(void) for ser1
MacSysX_serin:          DS.L 1        ; Cconin(void) for ser1
MacSysX_serout:         DS.L 1        ; Cconout( void *params ) for ser1
MacSysX_SerOpen:        DS.L 1        ; open serial port
MacSysX_SerClose:       DS.L 1        ; close serial port
MacSysX_SerRead:        DS.L 1        ; read multiple characters from serial port
MacSysX_SerWrite:       DS.L 1        ; write multiple characters to serial port
MacSysX_SerStat:        DS.L 1        ; read/write state for serial port
MacSysX_SerIoctl:       DS.L 1        ; Ioctl calls for serial port
MacSysX_GetKbOrMous:    DS.L PTRLEN   ; get keys and mouse events
MacSysX_dos_macfn:      DS.L 1        ; DosFn({int,void*} *) DOS functions 0x60..0xfe
MacSysX_xfs_version:    DS.L 1        ; version of host XFS
MacSysX_xfs_flags:      DS.L 1        ; flags for host XFS
MacSysX_xfs:            DS.L PTRLEN   ; central entry to host XFS
MacSysX_xfs_dev:        DS.L PTRLEN   ; corresponding file driver
MacSysX_drv2devcode:    DS.L PTRLEN   ; convert driver number to device number
MacSysX_rawdrvr:        DS.L PTRLEN   ; LONG RawDrvr({int, long} *) Raw driver (eject) for Mac
// previously MacSysX_Daemon:         DS.L PTRLEN   ; call for the mmx daemon
MacSysX_Daemon:	        DS.L 1		  ; MagicOnLinux: call for the mmx daemon
MacSysX_BlockDev:       DS.L 1        ; MagicOnLinux: disk image management
MacSysX_resvd1:         DS.L 1        ; MagicOnLinux: reserved for future use
MacSysX_resvd2:         DS.L 1        ; MagicOnLinux: reserved for future use
MacSysX_Yield:          DS.L 1        ; call to yield CPU time (idle)
MacSys_OldHdr:          DS.L 49       ; for compatibility with Behne's code
MacSysX_sizeof:

    TEXT

; Prozedur aufrufen. a0 auf Zeiger, a1 ist Parameter.

MACRO    MACPPC
        DC.W $00c0
        ENDM

; Elementfunktion aufrufen. a0 auf 4 Zeiger, a1 ist Parameter

MACRO    MACPPCE
        DC.W $00c1
        ENDM
*/

/// @brief System Header for MagicMacX
//
// The host callbacks are taken in MAGXBIOS.S by
// writing the 68k address of the callback pointer to register a0 and
// an 68k address of the parameters to register a1. Then an illegal
// 68k opcode is executed, either MACPPC or MACPPCE. The latter one
// additionally takes a "this" pointer that is located behind the
// host callback function pointer and passes this as first parameter
// of a class method.

#define MAGICLIN 1
struct MacXSysHdr
{
    // Atari -> Mac
    UINT32_BE    MacSys_magic;              // is 'MagC'
    UINT32_BE    MacSys_len;                // length of this structure
    PTR32_BE     MacSys_syshdr;             // address of Atari Syshdr
    PTR32_BE     MacSys_keytabs;            // 5*128 bytes for keyboard tables
    PTR32_BE     MacSys_mem_root;           // memory lists
    PTR32_BE     MacSys_act_pd;             // pointer to current process
    PTR32_BE     MacSys_act_appl;           // pointer to current task (appl)
    UINT32_BE    MacSys_verAtari;           // version number of MagicMacX.OS
    // Mac -> Atari
    UINT32_BE    MacSys_verMac;             // version number of this structure
    UINT16_BE    MacSys_cpu;                // CPU (20 = 68020, 30=68030, 40=68040)
    UINT16_BE    MacSys_fpu;                // FPU (0=nothing,4=68881,6=68882,8=68040)
    PTR32x4_HOST MacSys_init;               // called on Atari warm boot
    PTR32x4_HOST MacSys_biosinit;           // called after initialisation
    PTR32x4_HOST MacSys_VdiInit;            // called after initialisation of VDI
    PTR32_BE     MacSys_pixmap;             // 68k pointer, data for the VDI
    PTR32_BE     MacSys_pMMXCookie;         // 68k pointer to MgMx Cookie
    PTR32x4_HOST MacSys_Xcmd;               // XCMD commands
    PTR32_HOST   MacSys_PPCAddr;            // actual host address of 68k address 0 (on 64-bit host: 0x00000000)
    PTR32_HOST   MacSys_VideoAddr;          // actual host address of video memory (on 64-bit host: 0x80000000)
    PTR32x4_HOST MacSys_Exec68k;            // here the host callback can run 68k code
    PTR32_HOST   MacSys_gettime;            // LONG GetTime(void): get date and time
    PTR32_HOST   MacSys_settime;            // void SetTime(LONG *time): set date and time
    PTR32_HOST   MacSys_Setpalette;         // void Setpalette( int ptr[16] )
    PTR32_HOST   MacSys_Setcolor;           // int Setcolor( int nr, int val )
    PTR32_HOST   MacSys_VsetRGB;            // void VsetRGB( WORD index, WORD count, LONG *array )
    PTR32_HOST   MacSys_VgetRGB;            // void VgetRGB( WORD index, WORD count, LONG *array )
    PTR32_HOST   MacSys_syshalt;            // SysHalt( char *str ) "System halted"
    PTR32_HOST   MacSys_syserr;             // SysErr( long val ) "a1 = 0 => bombs"
    PTR32_HOST   MacSys_coldboot;           // ColdBoot(void) perform cold boot
    PTR32_HOST   MacSys_exit;               // Exit(void) end program
    PTR32_HOST   MacSys_debugout;           // MacPuts( char *str ) for debugging
    PTR32_HOST   MacSys_error;              // d0 = -1: no graphics driver
    PTR32_HOST   MacSys_prtos;              // Bcostat(void) for PRT
    PTR32_HOST   MacSys_prtin;              // Cconin(void) for PRT
    PTR32_HOST   MacSys_prtout;             // Cconout( void *params ) for PRT
    PTR32_HOST   MacSys_prtouts;            // LONG PrnOuts({char *buf, LONG count}) character string to printer
    PTR32_HOST   MacSys_serconf;            // Rsconf( void *params ) for ser1
    PTR32_HOST   MacSys_seris;              // Bconstat(void) for ser1 (AUX)
    PTR32_HOST   MacSys_seros;              // Bcostat(void) for ser1
    PTR32_HOST   MacSys_serin;              // Cconin(void) for ser1
    PTR32_HOST   MacSys_serout;             // Cconout( void *params ) for ser1
    PTR32_HOST   MacSys_SerOpen;            // open serial port
    PTR32_HOST   MacSys_SerClose;           // close serial port
    PTR32_HOST   MacSys_SerRead;            // read multiple characters from serial port
    PTR32_HOST   MacSys_SerWrite;           // write multiple characters to serial port
    PTR32_HOST   MacSys_SerStat;            // read/write state for serial port
    PTR32_HOST   MacSys_SerIoctl;           // Ioctl calls for serial port
    PTR32x4_HOST MacSys_GetKeybOrMouse;     // get keys and mouse events
    PTR32_HOST   MacSys_dos_macfn;          // DosFn({int,void*} *) DOS functions 0x60..0xfe
    UINT32_BE    MacSys_xfs_version;        // version of host XFS
    UINT32_BE    MacSys_xfs_flags;          // flags for host XFS
    PTR32x4_HOST MacSys_xfs;                // central entry to host XFS
    PTR32x4_HOST MacSys_xfs_dev;            // corresponding file driver
    PTR32x4_HOST MacSys_drv2devcode;        // convert drive number to device number
    PTR32x4_HOST MacSys_rawdrvr;            // LONG RawDrvr({int, long} *) Raw driver (eject) for Mac
#if defined(MAGICLIN)
    PTR32_HOST   MacSys_Daemon;             // call for the mmx daemon
    PTR32_HOST   MacSys_BlockDevice;        // new for MagicOnLinux
    PTR32_HOST   MacSys_resvd1;
    PTR32_HOST   MacSys_resvd2;
#else
    PTR32x4_HOST MacSys_Daemon;             // call for the mmx daemon
#endif
    PTR32_HOST   MacSys_Yield;              // call to yield CPU time (idle)
    OldMmSysHdr  MacSys_OldHdr;             // for compatibility with Behne's code
} __attribute__((packed));

/// signal handler, part of the MagiC process information structure
struct MagiC_SA
{
    UINT32    _ATARI_sa_handler;            // 0x00: signal handler
    UINT32    _ATARI_sa_sigextra;           // 0x04: OR mask during signal processing
    UINT16    _ATARI_sa_flags;
} __attribute__((packed));

/// device handler, part of the MagiC process information structure
struct MagiC_FH
{
    UINT32    fh_fd;
    UINT16    fh_flag;
} __attribute__((packed));

/// @brief MagiC Process Information
struct MagiC_ProcInfo
{
    UINT32_BE   pr_magic;           /* magic value, similarly to MiNT */
    UINT16_BE   pr_ruid;            /* "real user ID" */
    UINT16_BE   pr_rgid;            /* "real group ID" */
    UINT16_BE   pr_euid;            /* "effective user ID" */
    UINT16_BE   pr_egid;            /* "effective group ID" */
    UINT16_BE   pr_suid;            /* "saved user ID" */
    UINT16_BE   pr_sgid;            /* "saved group ID" */
    UINT16_BE   pr_auid;            /* "audit user ID" */
    UINT16_BE   pr_pri;             /* "base process priority" (only dummy) */
    UINT32_BE   pr_sigpending;      /* waiting signals */
    UINT32_BE   pr_sigmask;         /* signal mask */
    MagiC_SA    pr_sigdata[32];
    UINT32_BE   pr_usrva;           /* "user" value (introduced 9/1996) */
    PTR32_BE    pr_memlist;         /* table of "shared memory blocks" */
    char        pr_fname[128];      /* path of the corresponding .PRG file */
    char        pr_cmdlin[128];     /* oritinal command line */
    UINT16_BE   pr_flags;           /* bit 0: no entry in u:\proc */
                                    /* bit 1: created by Pfork() */
    UINT8       pr_procname[10];    /* process name for u:\proc\, without file name extension */
    UINT16_BE   pr_bconmap;         /* currently unused */
    MagiC_FH    pr_hndm6;           /* handle -6: unused */
    MagiC_FH    pr_hndm5;           /* handle -5: unused */
    MagiC_FH    pr_hndm4;           /* handle -4: by default this is NUL: */
    MagiC_FH    pr_hndm3;           /* handle -3: by default this is PRN: */
    MagiC_FH    pr_hndm2;           /* handle -2: by default this is AUX: */
    MagiC_FH    pr_hndm1;           /* handle -1: by default this is CON: */
    MagiC_FH    pr_handle[32];      /* handles 0..31 */
} __attribute__((packed));

/// @brief MagiC Process Descriptor
struct MagiC_PD
{
    PTR32_BE    p_lowtpa;       /* 0x00: begin of TPA, of the basepage (BP) itself */
    PTR32_BE    p_hitpa;        /* 0x04: points to first byte after TPA */
    PTR32_BE    p_tbase;        /* 0x08: begin of TEXT segment */
    UINT32_BE   p_tlen;         /* 0x0c: length of TEXT segment */
    PTR32_BE    p_dbase;        /* 0x10: begin of DATA segment */
    UINT32_BE   p_dlen;         /* 0x14: length of DATA segment */
    PTR32_BE    p_bbase;        /* 0x18: begin of BSS segment */
    UINT32_BE   p_blen;         /* 0x1c: length of BSS segment */
    PTR32_BE    p_dta;          /* 0x20: current DTA buffer */
    PTR32_BE    p_parent;       /* 0x24: pointer to BP of parent */
    UINT16_BE   p_procid;       /* 0x28: process ID */
    UINT16_BE   p_status;       /* 0x2a: used with MagiC 5.04 and newer */
    PTR32_BE    p_env;          /* 0x2c: pointer to environment */
    UINT8       p_devx[6];      /* 0x30: std handle <=> phs. handle */
    UINT8       p_flags;        /* 0x36: bit 0: Pdomain (MiNT:1/TOS:0) */
    UINT8       p_defdrv;       /* 0x37: default drive */
    UINT8       p_res3[8];      /* 0x38: termination context for ACC */
    UINT8       p_drvx[32];     /* 0x40: table: default path handles */
    PTR32_BE    p_procdata;     /* 0x60: pointer to PROCDATA */
    UINT16_BE   p_umask;        /* 0x64: umask for Unix file systems */
    UINT16_BE   p_procgroup;    /* 0x66: process group (introduced 1996-10-06) */
    UINT32_BE   p_mem;          /* 0x68: as much memory the process may allocate */
    PTR32_BE    p_context;      /* 0x6c: in MAGIX used instead of p_reg */
    UINT32_BE   p_mflags;       /* 0x70: bit 2: malloc allowed from AltRAM */
    PTR32_BE    p_app;          /* 0x74: APPL that started the process (main thread) */
    UINT32_BE   p_ssp;          /* 0x78: ssp at process start */
    UINT32_BE   p_reg;          /* 0x7c: for compatibility with TOS */
    UINT8       p_cmdlin[128];  /* 0x80: command line */
} __attribute__((packed));

/// Values for ap_status
enum MagiC_APP_STATUS
{
    eAPSTAT_READY = 0,
    eAPSTAT_WAITING = 1,
    eAPSTAT_SUSPENDED = 2,
    eAPSTAT_ZOMBIE = 3,
    eAPSTAT_STOPPED = 4
};

// gcc does not allow to specifically suppress the "array size 0 forbidden" warning,
// so we must switch off all pedantic warnings.

#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wzero-length-array"
#pragma GCC diagnostic ignored "-Wpedantic"

struct MagiC_APP
{
    PTR32_BE  ap_next;          // 0x00: concatenation pointer
    UINT16_BE  ap_id;           // 0x04: application ID
    UINT16_BE  ap_parent;       // 0x06: actual parent ID
    UINT16_BE  ap_parent2;      // 0x08: if applicable, the ap_id of VT52, to there ->CH_EXIT
    UINT16_BE  ap_type;         // 0x0a: 0 = Main Thread / 1 = Thread / 2 = Signal Handler
    UINT32_BE  ap_oldsigmask;   // 0x0c: saved signal mask (for Signal Handler)
    PTR32_BE   ap_sigthr;       // 0x10: if Main Tread: Pointer to active signal handler
                                //       if signal handler: Pointer to previous one or NULL
    UINT16_BE  ap_srchflg;      // 0x14: for appl_search
    PTR32_BE   ap_menutree;     // 0x16: menu tree
    PTR32_BE   ap_attached;     // 0x1a: NULL or list for menu_attach()
    PTR32_BE  ap_desktree;      // 0x1e: desktop background
    UINT16_BE  ap_1stob;        // 0x20: for this background: first object
    UINT8   ap_dummy1[2];       // 0x22: two space characters to be placed in front of ap_name
    UINT8   ap_name[8];         // 0x24: name (8 characters, with trailing blanks)
    UINT8   ap_dummy2[2];       // space character and, if applicable, hiding symbol
    UINT8   ap_dummy3;          // zero byte for end-of-string
    UINT8   ap_status;          // APSTAT_...
    UINT16_BE  ap_hbits;        // received events
    UINT16_BE  ap_rbits;        // expected events
    UINT32_BE  ap_evparm;       // event data, e.g. <pid> or message buffer
    PTR32_BE  ap_nxttim;        // next application waiting for timer
    UINT32_BE  ap_ms;           // timer
    PTR32_BE  ap_nxtalrm;       // next application waiting for alarm
    UINT32_BE  ap_alrmms;       // alarm
    UINT16_BE  ap_isalarm;      // flag
    PTR32_BE  ap_nxtsem;        // next application waiting for semaphore
    PTR32_BE  ap_semaph;        // the semaphore we are waiting for
    UINT16_BE  ap_unselcnt;     // length of table ap_unselx
    PTR32_BE  ap_unselx;        // table for evnt_(m)IO
    UINT32_BE  ap_evbut;        // for evnt_button
    UINT32_BE  ap_mgrect1;      // for evnt_mouse
    UINT32_BE  ap_mgrect2;      // for evnt_mouse
    UINT16_BE  ap_kbbuf[8];     // buffer for 8 keypresses
    UINT16_BE  ap_kbhead;       // next keypress to read
    UINT16_BE  ap_kbtail;       // next keypress to write
    UINT16_BE  ap_kbcnt;        // number of keypresses in buffer
    UINT16_BE  ap_len;          // message buffer length
    UINT8   ap_buf[0x300];      // message buffer (768 bytes = 48 messages)
    UINT16_BE  ap_critic;       // counter for "critical phase"
    UINT8   ap_crit_act;        // bit 0: killed
                                // bit 1: stopped
                                // bit 2: Signale testen
    UINT8   ap_stpsig;          // flag "stopped by signal"
    PTR32_BE  ap_sigfreeze;     // signal handler for SIGFREEZE
    UINT16_BE  ap_recogn;       // bit 0: understands AP_TERM
    UINT32_BE  ap_flags;        // Bit 0: does not accept proportional AES font
    UINT16_BE  ap_doex;
    UINT16_BE  ap_isgr;
    UINT16_BE  ap_wasgr;
    UINT16_BE  ap_isover;
    PTR32_BE  ap_ldpd;          // PD of loader process
    PTR32_BE  ap_env;           // environment or NULL
    PTR32_BE  ap_xtail;         // extended command line (> 128 bytes) or NULL
    UINT32_BE  ap_thr_usp;      // usp (user stack pointer) for Threads
    UINT32_BE  ap_memlimit;
    UINT32_BE  ap_nice;         // currently unused
    UINT8   ap_cmd[128];        // program path
    UINT8   ap_tai[128];        // program parameters
    UINT16_BE  ap_mhidecnt;     // local mouse hide counter
    UINT16_BE  ap_svd_mouse[37];    // x/y/planes/bg/fg/msk[32]/moff_cnt
    UINT16_BE  ap_prv_mouse[37];
    UINT16_BE  ap_act_mouse[37];
    UINT32_BE  ap_ssp;          // 0x58c:
    PTR32_BE  ap_pd;            // 0x590:
    PTR32_BE  ap_etvterm;
    UINT32_BE  ap_stkchk;       // magic value for stack overflow check
    UINT8   ap_stack[0];        // stack
} __attribute__((packed));


/// @brief MagiC Drive Media Descriptor, stored in an IMB (internal memory block)
///        Describes one volume.
struct XFS_DMD
{
    UINT32 d_xfs;       // 0x00: 68k pointer to file system driver
    UINT16 d_drive;     // 0x04: drive number 0..31
    UINT32 d_root;      // 0x06: 68k pointer to DD of root directory
    uint8_t data[94 - 10];
} __attribute__((packed));

/// @brief  MagiC Directory Descriptor, stored in an IMB (internal memory block)
struct XFS_DD
{
    UINT32 dd_dmd;      // 68k pointer to XFS_DMD
    UINT16 dd_refcnt;
    uint8_t data[94 - 6];   // private part, i.e. MXFSDD (6 bytes)
} __attribute__((packed));

/// @brief  MagiC File Descriptor, stored in an IMB (internal memory block)
struct XFS_FD
{
    UINT32 fd_dmd;      // 0x00: 68k pointer to XFS_DMD
    UINT16 fd_refcnt;   // 0x04: reference counter for closing, or -1
    UINT16 fd_mode;     // 0x06: open modus (0,1,2) and flags
    UINT32 fd_dev;      // 0x08: 68k pointer to MAGX_DEVDRVR
    uint8_t data[94 - 12];
} __attribute__((packed));


#if 1
/// DMDs, FDs and DDs are stored in MagiC in "internal memory blocks",
/// each of size 100 bytes, including header. Thus we have 94 bytes of payload.
/// Unfortunately, we currently cannot make use of the full size of these
/// structures in the hostXFS, because the Atari side of the XFS (MACXFS.S)
/// does not provide pointers to the host side and instead only deals with
/// the MacOS specific structure members.
/// Note that the MagiC internal DOS file system stores 8+3 filenames
/// in its FD or DD and uses an additional IMB in case of a long filename.
/// The DOS XFS also links DDs and FDs to parents, children, siblings, clones etc.
/// See STRUCTS.INC of MagiC kernel, structure "fd".
struct IMB
{
    UINT32      pLink;      // 68k pointer to next IMB
    uint8_t     bUsed;      // flag
    uint8_t     bSwitch;    // unused?
    union HostXFS
    {
        // FD = File Descriptor
        struct
        {
            UINT32 fd_dmd;      // 0x00: 68k pointer to DMD
            UINT16 fd_refcnt;   // 0x04: reference counter for closing, or -1
            UINT16 fd_mode;     // 0x06: open modus (0,1,2) and flags
            UINT32 fd_dev;      // 0x08: 68k pointer to MAGX_DEVDRVR
            uint8_t data[94 - 12];
        } fd;

        // DD = Directory Descriptor
        struct
        {
            UINT32 dd_dmd;      // 68k pointer
            UINT16 dd_refcnt;
            uint8_t data[94 - 6];   // private part, i.e. MXFSDD (6 bytes)
        } dd;

        // DMD = Drive Media Descriptor
        struct
        {
            UINT32 d_xfs;       // 0x00: 68k pointer to file system driver
            UINT16 d_drive;     // 0x04: drive number 0..31
            UINT32 d_root;      // 0x06: 68k pointer to DD of root directory
            uint8_t data[94 - 10];
        } dmd;

        uint8_t     data[94];   // depending
    };
};
#endif


#pragma GCC diagnostic pop

#endif
