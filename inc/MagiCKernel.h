/*
 * Copyright (C) 1990-2018/2025 Andreas Kromke, andreas.kromke@gmail.com
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


/*
*
* Constants and structures for the MagiC kernel
* (currently unused, only for documentation)
*
*/

#ifndef MAGICKERNEL_H_INCLUDED
#define MAGICKERNEL_H_INCLUDED

#include "Atari.h"


#define MagiCKernel_MAX_OPEN    32    // valid handles 0..31

struct MagiCKernel_PD
{
    PTR32_BE p_lowtpa;
    PTR32_BE p_hitpa;
    PTR32_BE p_tbase;
    UINT32_BE p_tlen;
    PTR32_BE p_dbase;
    UINT32_BE p_dlen;
    PTR32_BE p_bbase;
    UINT32_BE p_blen;
    PTR32_BE p_dta;
    PTR32_BE p_parent;      // 0x24: Parent process
    UINT16_BE p_procid;     // 0x28: Process id
    UINT16_BE p_status;     // 0x2a: >= MagiC 5.04
    PTR32_BE p_env;
    char p_devx[6];
    char p_flags;           // 0x36: Bit 0: Pdomain (MiNT:1/TOS:0)
    char p_defdrv;
    char p_res3[8];         // 0x38: Termination context for ACC
    char p_drvx[32];        // 0x40: Table: default path handles for all drives
    UINT32_BE p_procdata;   // 0x60: Pointer to PROCDATA
    UINT16_BE p_umask;      // 0x64: umask for Unix file systems
    UINT16_BE p_procgroup;  // 0x66: process group (>= 6.10.96)
    UINT32_BE p_mem;        // 0x68: amount of memory that may be allocated
    UINT32_BE p_context;    // 0x6c: used by MAGIX instead of p_reg benutzt
    UINT32_BE p_mflags;     // 0x70: Bit 2: Malloc() allowed from AltRAM
    UINT32_BE p_app;        // 0x74: APPL that started the process (main thread)
    UINT32_BE p_ssp;        // 0x78: ssp at process start
    UINT32_BE p_reg;        // 0x7c: for compatibility with TOS
    char p_cmdline[128];
} __attribute__((packed));

struct MagiCKernel_SIGHNDL
{
    UINT32_BE    sa_handler;    // 0x00: signal handler
    UINT32_BE    sa_sigextra;   // 0x04: OR mask during signal processing
    UINT16_BE    sa_flags;
} __attribute__((packed));

struct MagiCKernel_FH
{
    UINT32_BE    fh_fd;
    UINT16_BE    fh_flag;
} __attribute__((packed));

struct MagiCKernel_PROCDATA
{
    UINT32_BE    pr_magic;          // magic value, simlar as in MiNT
    UINT16_BE    pr_ruid;           // "real user ID"
    UINT16_BE    pr_rgid;           // "real group ID"
    UINT16_BE    pr_euid;           // "effective user ID"
    UINT16_BE    pr_egid;           // "effective group ID"
    UINT16_BE    pr_suid;           // "saved user ID"
    UINT16_BE    pr_sgid;           // "saved group ID"
    UINT16_BE    pr_auid;           // "audit user ID"
    UINT16_BE    pr_pri;            // "base process priority" (only dummy)
    UINT32_BE    pr_sigpending;     // waiting signals
    UINT32_BE    pr_sigmask;        // signal mask
    struct MagiCKernel_SIGHNDL pr_sigdata[32];
    UINT32_BE    pr_usrval;         // "User" value (>= 9/96)
    UINT32_BE    pr_memlist;        // table of "shared memory blocks"
    char         pr_fname[128];     // path of corresponding .prg file
    char         pr_cmdlin[128];    // initial command line
    UINT16_BE    pr_flags;          // Bit 0: no entry in u:\proc
                                    // Bit 1: created by Pfork()
    char         pr_procname[10];   // process name for u:\proc\, without filename extension
    UINT16_BE    pr_bconmap;        // (currently unused)
    struct MagiCKernel_FH pr_hndm6; // Handle -6: unused
    struct MagiCKernel_FH pr_hndm5; // Handle -5: unused
    struct MagiCKernel_FH pr_hndm4; // Handle -4: by default this is NUL:
    struct MagiCKernel_FH pr_hndm3; // Handle -3: by default this is PRN:
    struct MagiCKernel_FH pr_hndm2; // Handle -2: by default this is AUX:
    struct MagiCKernel_FH pr_hndm1; // Handle -1: by default this is CON:
    struct MagiCKernel_FH pr_handle[MagiCKernel_MAX_OPEN];    // handles 0..31
} __attribute__((packed));

 struct MagiCKernel_APP
{
    UINT32_BE    ap_next;           // concatenation pointer
    UINT16_BE    ap_id;             // Application ID
    UINT16_BE    ap_parent;         // actual (== tatsächliche!) parent ID
    UINT16_BE    ap_parent2;        // if used, the ap_id of VT52, send CH_EXIT there
    UINT16_BE    ap_type;           // 0 = main thread / 1 = thread / 2 = signal handler
    UINT32_BE    ap_oldsigmask;     // old signal mask (for signal handler)
    UINT32_BE    ap_sigthr;         // main thread: pointer to signal handler
                                    // Signalhandler: pointer to previous or NULL
    UINT16_BE    ap_srchflg;        // for appl_search()
    UINT32_BE    ap_menutree;       // menu tree
    UINT32_BE    ap_attached;       // NULL or list for menu_attach()
    UINT32_BE    ap_desktree;       // desktop background ..
    UINT16_BE    ap_1stob;          // .. and its first object
    UINT8        ap_dummy1[2];      // two spaces before ap_name
    UINT8        ap_name[8];        // name (8 characters with trailing blanks)
    UINT8        ap_dummy2[2];      // space and, if used, hide mark
    UINT8        ap_dummy3;         // zero byte for end-of-string
    UINT8        ap_status;         // APSTAT_...
    UINT16_BE    ap_hbits;          // received events
    UINT16_BE    ap_rbits;          // exptected events
    UINT32_BE    ap_evparm;         // event data, e.g. <pid> or msg buffer
    UINT32_BE    ap_nxttim;         // next APP that's waiting for timer
    UINT32_BE    ap_ms;             // timer
    UINT32_BE    ap_nxtalrm;        // next APP waiting for alarm
    UINT32_BE    ap_alrmms;         // alarm
    UINT16_BE    ap_isalarm;        // flag
    UINT32_BE    ap_nxtsem;         // next APP waiting for semaphore
    UINT32_BE    ap_semaph;         // the Semaphore we are waiting for
    UINT16_BE    ap_unselcnt;       // length of table ap_unselx
    UINT32_BE    ap_unselx;         // table fur evnt_(m)IO
    UINT32_BE    ap_evbut;          // for evnt_button
    UINT32_BE    ap_mgrect1;        // for evnt_mouse
    UINT32_BE    ap_mgrect2;        // for evnt_mouse
    UINT16_BE    ap_kbbuf[8];       // buffer for 8 keys
    UINT16_BE    ap_kbhead;         // next key to read
    UINT16_BE    ap_kbtail;         // next key to write
    UINT16_BE    ap_kbcnt;          // number of keys in buffer
    UINT16_BE    ap_len;            // message buffer length
    UINT8        ap_buf[0x300];     // message bufferr (768 bytes = 48 messages)
    UINT16_BE    ap_critic;         // counter for "critical phase"
    UINT8        ap_crit_act;       // bit 0: killed
                                    // bit 1: stopped
                                    // bit 2: test signals
    UINT8        ap_stpsig;         // Flag "stopped by signal"
    UINT32_BE    ap_sigfreeze;      // aignal handler for SIGFREEZE
    UINT16_BE    ap_recogn;         // Bit 0: handles AP_TERM
    UINT32_BE    ap_flags;          // Bit 0: deny proportional AES font
    UINT16_BE    ap_doex;
    UINT16_BE    ap_isgr;
    UINT16_BE    ap_wasgr;
    UINT16_BE    ap_isover;
    UINT32_BE    ap_ldpd;           // PD of loader process
    PTR32_BE     ap_env;            // environment or NULL
    PTR32_BE     ap_xtail;          // extended command line (> 128 bytes) or NULL
    UINT32_BE    ap_thr_usp;        // usp for threads
    UINT32_BE    ap_memlimit;
    UINT32_BE    ap_nice;           // (currently unused)
    UINT8        ap_cmd[128];       // program path
    UINT8        ap_tai[128];       // program parameters
    UINT16_BE    ap_mhidecnt;       // local mouse hide counter
    UINT16_BE    ap_svd_mouse[37];  // x/y/planes/bg/fg/msk[32]/moff_cnt
    UINT16_BE    ap_prv_mouse[37];
    UINT16_BE    ap_act_mouse[37];
    UINT32_BE    ap_ssp;
    UINT32_BE    ap_pd;
    UINT32_BE    ap_etvterm;
    UINT32_BE    ap_stkchk;         // magic guard word for checking stack overflow
    UINT8        ap_stack[0];       // stack
} __attribute__((packed));

#endif
