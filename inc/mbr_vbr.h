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
* Does everything that deals with volume images
*
*/

#ifndef _MBR_VBR_H_INCLUDED_
#define _MBR_VBR_H_INCLUDED_

#include <stdint.h>


// number of MBR primary partitions
#define MBR_NO_PRIMARY_PARTITIONS 4

// MBR Partition entry (16 bytes)
struct MBR_PART
{
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t part_type;
    uint8_t chs_last[3];
    uint8_t lba_first[4];       // first absolute sector (little-endian)
    uint8_t lba_num[4];         // number of sectors (little-endian)
} __attribute__((packed));

// Master Boot Record, old Partition table format, before GPT came up
struct MBR
{
    uint8_t boot_code[446];     // 0x000
    MBR_PART partitions[4];     // 0x1be / 0x1ce / 0x1de / 0x1ee
    uint8_t sig0;               // 0x55
    uint8_t sig1;               // 0xaa
} __attribute__((packed));


// alles little-endian!
// AUCH VBR genannt (Volume Boot Record)
struct FAT_BOOT_SEC_32
{
    uint8_t bs_jump[3];         /* 0x00: x86 langer oder kurzer Sprung auf Bootcode */
                                /*       0xeb,0x??,0x90 oder 0xe9,0x??,0x?? */
    uint8_t bs_system_id[8];    /* 0x03: Systemname, sollte "MSWIN4.1" sein */
    uint8_t bs_sec_size[2];     /* 0x0b: Bytes pro Sektor. */
                                /*       M$ erlaubt hier 512,1024,2048 oder 4096, empfiehlt aber 512 */
    uint8_t bs_clu_size;        /* 0x0d: Sektoren pro Cluster. M$ erlaubt jede */
                                /*       2er-Potenz von 1 bis 128 und empfiehlt dringend Bytes/Cluster <= 32kB */
    uint16_t bs_sec_resvd;      /* 0x0e: Anzahl reservierter Sektoren ab Partitionsanfang, */
                                /*       darf nicht Null sein (Bootsektor!). Bei FAT12 und FAT16 */
                                /*       sollte der Wert 1 sein, für FAT32 32 */
    uint8_t bs_nfats;           /* 0x10: Anzahl FATs. M$ empfiehlt 2 */
    uint8_t bs_dir_entr[2];     /* 0x11: Anzahl Einträge (à 32 Bytes) für root, 0 bei FAT32 */
    uint8_t bs_nsectors[2];     /* 0x13: Anzahl Sektoren (reserviert+FAT+root+Data) bzw. 0, wenn > 65535 */
    uint8_t bs_media;           /* 0x15: "media code": 0xf8 für Harddisk, 0xf0 für Wechselmedium */
                                /*       der Wert muß im Lowbyte von FAT[0] stehen */
    uint16_t bs_fatlen;         /* 0x16: Sektoren für eine FAT. 0 für FAT32 (daran wird FAT32 erkannt) */
    uint16_t bs_secs_track;     /* 0x18: Sektoren pro Spur (nur historisch oder Floppy) */
    uint16_t bs_heads;          /* 0x1a: Anzahl Köpfe (historisch oder Floppy) */
    uint32_t bs_hidden;         /* 0x1c: Anzahl versteckter Sektoren VOR der Partition (normalerweise 0) */
    uint32_t bs_total_sect;     /* 0x20: Anzahl Sektoren (reserviert+FAT+root+Data), wenn bs_nsectors == 0 oder bei FAT32 */

    // Ab hier unterscheiden sich FAT12/16 und FAT32
    // Hier die Felder für FAT32:

    uint32_t bs_fatlen32;       /* Anzahl Sektoren für eine FAT, wenn bs_fatlen == 0 */
    uint16_t bs_flags;          /* Bit 0..3: aktive FAT, wenn Bit 7 == 1 */
                                /* Bit 4..6: reserviert */
                                /* Bit 7: 0 für "FAT-Spiegelung", 1 für "nur eine aktive FAT" */
                                /* Bit 8..15: reserviert */
    uint8_t bs_version[2];      /* Hi: "major filesystem version, Lo: "lower" */
                                /* zur Zeit 0.0. Ein Treiber sollte neuere Versionen verweigern */
    uint32_t bs_rootclust;      /* Erster Cluster des Wurzelverzeichnisses, sollte */
                                /* normalerweise 2 sein */
    uint16_t bs_info_sect;      /* Sektornummer des Info-Sektors (im reservierten Bereich) */
                                /* normalerweise 1 (direkt hinter dem Bootsektor) */
    uint16_t bs_bckup_boot;     /* Sektornummer des Backup-Bootsektors (im reservierten Bereich) */
                                /* sollte 0 sein (unbenutzt) oder 6 */
                                /* Hinter dem Backup-Bootsektor liegt das Backup-FSInfo */
    uint8_t bs_RESERVED2[12];   /* reserviert, sollte 0 sein */
    uint8_t bs_DrvNum;          /* "drive number". 0x00 == floppy, 0x80 == HD */
    uint8_t bs_Reserved1;       /* für Windows NT reserviert, sollte 0 sein */
    uint8_t bs_BootSig;         /* 0x29 legt fest, daß die folgenden drei Felder gültig sind */
    uint8_t bs_VolID[4];        /* Seriennummer, die mit bs_VolLab zusammen zur Medienwechselerkennung verwendet wird */
                                /* ist i.a. Datum+Uhrzeit der Formatierung kombiniert */
    uint8_t bs_VolLab[11];      /* muß mit dem Disknamen im Wurzelverzeichnis identisch sein. */
                                /* Ist "NO NAME    ", wenn das Medium unbenannt ist */
    uint8_t bs_FilSysType[8];   /* "FAT32   ". Darf aber nicht zur Bestimmung des Typs verwendet werden */

} __attribute__((packed));


struct FAT_BOOT_SEC_16_12
{
    uint8_t bs_jump[3];         /* 0x00: x86 langer oder kurzer Sprung auf Bootcode */
                                /*       0xeb,0x??,0x90 oder 0xe9,0x??,0x?? */
    uint8_t bs_system_id[8];    /* 0x03: Systemname, sollte "MSWIN4.1" sein */
    uint8_t bs_sec_size[2];     /* 0x0b: Bytes pro Sektor. */
                                /*       M$ erlaubt hier 512,1024,2048 oder 4096, empfiehlt aber 512 */
    uint8_t bs_clu_size;        /* 0x0d: Sektoren pro Cluster. M$ erlaubt jede */
                                /*       2er-Potenz von 1 bis 128 und empfiehlt dringend Bytes/Cluster <= 32kB */
    uint16_t bs_sec_resvd;      /* 0x0e: Anzahl reservierter Sektoren ab Partitionsanfang, */
                                /*       darf nicht Null sein (Bootsektor!). Bei FAT12 und FAT16 */
                                /*       sollte der Wert 1 sein, für FAT32 32 */
    uint8_t bs_nfats;           /* 0x10: Anzahl FATs. M$ empfiehlt 2 */
    uint8_t bs_dir_entr[2];     /* 0x11: Anzahl Einträge (à 32 Bytes) für root, 0 bei FAT32 */
    uint8_t bs_nsectors[2];     /* 0x13: Anzahl Sektoren (reserviert+FAT+root+Data) bzw. 0, wenn > 65535 */
    uint8_t bs_media;           /* 0x15: "media code": 0xf8 für Harddisk, 0xf0 für Wechselmedium */
                                /*       der Wert muß im Lowbyte von FAT[0] stehen */
    uint16_t bs_fatlen;         /* 0x16: Sektoren für eine FAT. 0 für FAT32 (daran wird FAT32 erkannt) */
    uint16_t bs_secs_track;     /* 0x18: Sektoren pro Spur (nur historisch oder Floppy) */
    uint16_t bs_heads;          /* 0x1a: Anzahl Köpfe (historisch oder Floppy) */
    uint32_t bs_hidden;         /* 0x1c: Anzahl versteckter Sektoren VOR der Partition (normalerweise 0) */
    uint32_t bs_total_sect;     /* 0x20: Anzahl Sektoren (reserviert+FAT+root+Data), wenn bs_nsectors == 0 oder bei FAT32 */

    // Ab hier unterscheiden sich FAT12/16 und FAT32
    // Hier die Felder für FAT16:

    uint8_t bs_drvnum;          /* 0x24: physische Laufwerknummer */
    uint8_t bs_res;             /* 0x25: reserviert, von NT verwendet */
    uint8_t bs_xbootsig;        /* 0x26: erweiterte Bootsignatur: Wenn == 0x29 dann sind die folgenden 3 Felder präsent. */
    uint8_t bs_vol_id[4];       /* 0x27: Volume ID (i. d. R. Kombination aus Datum und Zeit) */
    uint8_t bs_vol_name[11];    /* 0x2b: Volume Name (mit Leerzeichen aufgefüllt, z. B. "NO NAME "). */
    uint8_t bs_fs_id[8];        /* 0x36: Dateisystem ID (mit Leerzeichen aufgefüllt: 'FAT ', 'FAT12 ' oder 'FAT16 ').
                                         Hat nur informellen Charakter, d. h. sollte nicht zur Bestimmung des FAT-Typs genutzt werden! */
} __attribute__((packed));

#endif
