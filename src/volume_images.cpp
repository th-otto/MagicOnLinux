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

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "Debug.h"
#include "conversion.h"
#include "gui.h"
#include "volume_images.h"


const char *CVolumeImages::drv_image_host_path[NDRIVES];       // nullptr, if not valid
int CVolumeImages::drv_image_fd[NDRIVES];
uint64_t CVolumeImages::drv_image_size[NDRIVES];
bool CVolumeImages::drv_longNames[NDRIVES];              // initialised with zeros
bool CVolumeImages::drv_readOnly[NDRIVES];
uint32_t CVolumeImages::m_diskimages_drvbits;


/** **********************************************************************************************
 *
 * @brief get disk images from preferences
 *
 ************************************************************************************************/
void CVolumeImages::init()
{
    struct stat statbuf;

    m_diskimages_drvbits = 0;
    for (int i = 0; i < NDRIVES; i++)
    {
        const char *path = Preferences::drvPath[i];
        unsigned flags = Preferences::drvFlags[i];
        if (path != nullptr)
        {
            int res = stat(path, &statbuf);
            if (res < 0)
            {
                // alert already shown by HostXFS
                // (void) showAlert("Invalid Atari drive path:", path, 1);
                continue;
            }
            // we should not get symbolic links here, because of stat, not lstat...
            mode_t ftype = (statbuf.st_mode & S_IFMT);
            if (ftype != S_IFREG)
            {
                DebugInfo2("() -- Atari drive %c: is not a regular file: \"%s\"", 'A' + i, path);
                path = nullptr;
            }
        }

        if (path != nullptr)
        {
            drv_image_host_path[i] = CConversion::copyString(path);
            drv_image_size[i] = statbuf.st_size;
            drv_longNames[i] = (flags & DRV_FLAG_8p3) == 0;
            drv_readOnly[i] = (flags & DRV_FLAG_RDONLY);
            m_diskimages_drvbits |= (1 << i);
        }
        else
        {
            drv_image_host_path[i] = nullptr;    // invalid
            drv_longNames[i] = false;
            drv_readOnly[i] = false;
            drv_image_size[i] = 0;
        }

        drv_image_fd[i] = -1;   // not open

    }
}


/** **********************************************************************************************
 *
 * @brief close all disk images
 *
 ************************************************************************************************/
void CVolumeImages::exit(void)
{
    for (int i = 0; i < NDRIVES; i++)
    {
        if (drv_image_fd[i] >= 0)
        {
            close(drv_image_fd[i]);
            drv_image_fd[i] = -1;
        }
    }
}


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



/** **********************************************************************************************
 *
 * @brief Converts boot sector an image to BPB (FAT12 or FAT16), if any
 *
 * @param[out] vbr      boot sector
 * @param[out] bpb      buffer for BPB (inside Atari memory)
 *
 * @return E_OK or negative error code
 *
 * @note Only handles FAT12 and FAT16.
 *
 ************************************************************************************************/
uint32_t CVolumeImages::vbr2Bpb(const uint8_t *sector, BPB *bpb)
{
    const FAT_BOOT_SEC_16_12 *vbr = (const FAT_BOOT_SEC_16_12 *) sector;

    uint16_t reserved_sectors;
    uint16_t sec_size;
    uint16_t cl_sectors;
    uint16_t root_dir_size;
    uint16_t fat_sectors;
    uint16_t fat2_sectors;
    uint16_t fat2_secno;
    uint16_t data_secno;
    uint32_t num_sectors;
    uint32_t num_clusters;
    uint8_t nfats;
    uint16_t flags0 = 0;

    // sector size in bytes (little-endian)
    sec_size = vbr->bs_sec_size[0];
    sec_size += (vbr->bs_sec_size[1] << 8);
    if (sec_size < 512)
    {
        DebugWarning2("() -- Invalid sector size %u", sec_size);
        return EUNDEV;
    }
    bpb->b_recsiz = htobe16(sec_size);

    // sectors per cluster
    cl_sectors = vbr->bs_clu_size;
    if (cl_sectors < 1)
    {
        DebugWarning2("() -- Invalid cluster size %u sectors", cl_sectors);
        return EUNDEV;
    }
    bpb->b_clsiz = htobe16(cl_sectors);

    // bytes per cluster
    bpb->b_clsizb = htobe16(sec_size * cl_sectors);

    // sectors for root directory
    root_dir_size = vbr->bs_dir_entr[0];
    root_dir_size += (vbr->bs_dir_entr[1] << 8);
    if (root_dir_size == 0)
    {
        DebugWarning2("() -- bs_dir_entr = 0, seems to be FAT32, handled by MagiC kernel");
        return EUNDEV;
    }
    root_dir_size <<= 5;    // 32 bytes per entry
    if (root_dir_size % sec_size != 0)
    {
        DebugWarning2("() -- Root directory size mismatch. Ignored.");
    }
    root_dir_size /= sec_size;
    bpb->b_rdlen = htobe16(root_dir_size);

    // sectors per FAT
    fat_sectors = le16toh(vbr->bs_fatlen);
    if (fat_sectors == 0)
    {
        DebugWarning2("() -- bs_fatlen = 0, seems to be FAT32, handled by MagiC kernel");
        return EUNDEV;
    }
    bpb->b_fsiz = htobe16(fat_sectors);

    // number or reserved sectors, including boot sector, should be 1
    reserved_sectors = le16toh(vbr->bs_sec_resvd);
    if (reserved_sectors != 1)
    {
        DebugWarning2("() -- %u reserved sectors, should be 1. Continue.", reserved_sectors);
    }

    // sector number of second FAT
    fat2_secno = reserved_sectors + fat_sectors;
    bpb->b_fatrec = htobe16(fat2_secno);

    // number of FATs
    nfats = vbr->bs_nfats;
    if ((nfats < 1) || (nfats > 2))
    {
        DebugWarning2("() -- num_fats = %u is not supported", nfats);
        return EUNDEV;
    }
    if (nfats == 1)
    {
        flags0 |= 2;    // bit 1: only one FAT
        fat2_sectors = 0;
    }
    else
    {
        fat2_sectors = fat_sectors;
    }

    // sector number of first data cluster
    data_secno = fat2_secno + fat2_sectors + root_dir_size;
    bpb->b_datrec = htobe16(data_secno);

    // total number of sectors
    num_sectors = vbr->bs_nsectors[0];
    num_sectors += (vbr->bs_nsectors[1] << 8);
    if (num_sectors == 0)
    {
        num_sectors = le32toh(vbr->bs_total_sect);
    }
    if (num_sectors < data_secno)
    {
        DebugWarning2("() -- num_sectors = %u is inconsistant", num_sectors);
        return EUNDEV;
    }

    // number of data clusters: subtract reserved, FATs, root, then convert to clusters
    // 65525 from Microsoft documentation
    num_clusters = (num_sectors - data_secno) / cl_sectors;
    if (num_clusters >= 65525)
    {
        DebugWarning2("() -- num_clusters = %u is too much for FAT12/16", num_clusters);
        return EUNDEV;
    }
    bpb->b_numcl = htobe16((uint16_t) num_clusters);

    // FAT12 or FAT16
    // 65525 from Microsoft documentation
    if (num_clusters >= 4085)
    {
        flags0 |= 1;    // bit 0: FAT16
    }
    bpb->b_flags[0] = htobe16(flags0);

    // Set unused flags to zero. Note that the MagiC kernel reserves
    // four additional bytes, in total 36 bytes for internal BPB.
    bpb->b_flags[1] = 0;
    bpb->b_flags[2] = 0;
    bpb->b_flags[3] = 0;
    bpb->b_flags[4] = 0;
    bpb->b_flags[5] = 0;
    bpb->b_flags[6] = 0;
    bpb->b_flags[7] = 0;

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Check if boot sector an image of FAT32
 *
 * @param[out] vbr      boot sector
 *
 * @return E_OK or negative error code
 *
 * @note Only handles FAT32
 *
 ************************************************************************************************/
uint32_t CVolumeImages::vbr2Fat32(const uint8_t *sector)
{
    const FAT_BOOT_SEC_32 *vbr = (const FAT_BOOT_SEC_32 *) sector;

    uint16_t reserved_sectors;
    uint16_t sec_size;
    uint16_t cl_sectors;
    uint16_t root_dir_size;
    uint32_t fat_sectors;
    uint32_t fat2_sectors;
    uint32_t fat2_secno;
    uint32_t data_secno;
    uint32_t num_sectors;
    uint32_t num_clusters;
    uint8_t nfats;
    uint16_t flags0 = 0;

    // sector size in bytes (little-endian)
    sec_size = vbr->bs_sec_size[0];
    sec_size += (vbr->bs_sec_size[1] << 8);
    if (sec_size < 512)
    {
        DebugWarning2("() -- Invalid sector size %u", sec_size);
        return EUNDEV;
    }

    // sectors per cluster
    cl_sectors = vbr->bs_clu_size;
    if (cl_sectors < 1)
    {
        DebugWarning2("() -- Invalid cluster size %u sectors", cl_sectors);
        return EUNDEV;
    }

    // sectors for root directory
    root_dir_size = vbr->bs_dir_entr[0];
    root_dir_size += (vbr->bs_dir_entr[1] << 8);
    if (root_dir_size > 0)
    {
        // FAT16 or FAT32
        return EUNDEV;
    }

    // sectors per FAT
    fat_sectors = le16toh(vbr->bs_fatlen);
    if (fat_sectors > 0)
    {
        // FAT16 or FAT32
        return EUNDEV;
    }
    fat_sectors = le16toh(vbr->bs_fatlen32);

    // number or reserved sectors, including boot sector, should be 32 für FAT32
    reserved_sectors = le16toh(vbr->bs_sec_resvd);
    if (reserved_sectors != 32)
    {
        DebugWarning2("() -- %u reserved sectors, should be 32. Continue.", reserved_sectors);
    }

    // sector number of second FAT
    fat2_secno = reserved_sectors + fat_sectors;

    // number of FATs
    nfats = vbr->bs_nfats;
    if ((nfats < 1) || (nfats > 2))
    {
        DebugWarning2("() -- num_fats = %u is not supported", nfats);
        return EUNDEV;
    }
    if (nfats == 1)
    {
        flags0 |= 2;    // bit 1: only one FAT
        fat2_sectors = 0;
    }
    else
    {
        fat2_sectors = fat_sectors;
    }

    // sector number of first data cluster
    data_secno = fat2_secno + fat2_sectors + root_dir_size;

    // total number of sectors
    num_sectors = vbr->bs_nsectors[0];
    num_sectors += (vbr->bs_nsectors[1] << 8);
    if (num_sectors == 0)
    {
        num_sectors = le32toh(vbr->bs_total_sect);
    }
    if (num_sectors < data_secno)
    {
        DebugWarning2("() -- num_sectors = %u is inconsistant", num_sectors);
        return EUNDEV;
    }

    num_clusters = (num_sectors - data_secno) / cl_sectors;
    if (vbr->bs_rootclust >= num_clusters)
    {
        DebugWarning2("() -- bs_rootclust %u inconsistant with %u clusters", vbr->bs_rootclust, num_clusters);
        return EUNDEV;
    }

    return E_OK;
}


/** **********************************************************************************************
 *
 * @brief Handle hdv_getbpb
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[out] dskbuf    4096-byte Atari disk buffer that may be used here
 * @param[out] bpb       buffer for BPB (inside Atari memory)
 *
 * @return E_OK or negative error code
 *
 * @note Only handles FAT12 and FAT16. If this fails, FAT32 will be handled by MagiC kernel.
 *       The MagiC kernel itself ONLY (!) handles BPB for FAT32.
 *
 ************************************************************************************************/
uint32_t CVolumeImages::AtariGetBpb(uint16_t drv, uint8_t *dskbuf, BPB *bpb)
{
    // read boot sector and evaluate
    INT32 aerr = AtariRwabs(drv, 0,  1, 0, dskbuf);
    if (aerr == E_OK)
    {
        aerr = vbr2Bpb(dskbuf, bpb);
    }
    return aerr;
}


/** **********************************************************************************************
 *
 * @brief Handle hdv_rwabs Atari callback, read or write sectors
 *
 * @param[in]  path     path to image file
 *
 * @return 12 for FAT12, 16 for FAT16, 32 for FAT32, -2 for format error and -1 for file error
 *
 ************************************************************************************************/
int CVolumeImages::checkFatVolume(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        return -1;      // file not accessible
    }
    uint8_t *dskbuf = (uint8_t *) malloc(512);
    ssize_t bytes = read(fd, dskbuf, 512);
    close(fd);
    if (bytes != 512)
    {
        free(dskbuf);
        return -2;      // file too small
    }

    // 1. Try FAT12 and 16

    BPB bpb;
    INT32 aerr = vbr2Bpb(dskbuf, &bpb);
    if (aerr == E_OK)
    {
        free(dskbuf);
        return (bpb.b_flags[0] & 1) ? 16 : 12;
    }

    // 2. Try FAT32

    aerr = vbr2Fat32(dskbuf);
    free(dskbuf);

    return (aerr == E_OK) ? 32 : -2;
}


/** **********************************************************************************************
 *
 * @brief Handle hdv_rwabs Atari callback, read or write sectors
 *
 * @param[in]  drv       Atari drive number 0..31
 * @param[in]  flags     bit 0: write (1) or read (0), bit 1: physical, not logical sector number
 * @param[in]  count     number of sectors to read or write
 * @param[in]  lrecno    sector number where to start transfer
 * @param[out] buf       data to write or buffer to read to
 *
 * @return E_OK or negative error code
 *
 * @note Physical sector numbers are not supported, yet. They are needed for multi-volume
 *       images, i.e. with partition table. Here the disk can be addressed instead of the
 *       partition (volume).
 *
 ************************************************************************************************/
uint32_t CVolumeImages::AtariRwabs(uint16_t drv, uint16_t flags, uint16_t count, uint32_t lrecno, uint8_t *buf)
{
    DebugInfo2("() - hdv_rawbs(flags = 0x%04x, buf = 0x%08x, count = %u, dev = %u, lrecno = %u)",
                    flags, buf, count, drv, lrecno);

    if ((drv < NDRIVES) && ((drv_image_host_path[drv]) != nullptr))
    {
        if (drv_image_fd[drv] == -1)
        {
            // not open, yet, or cannot be opend
            drv_image_fd[drv] = open(drv_image_host_path[drv], drv_readOnly[drv] ? O_RDONLY : O_RDWR);
            if (drv_image_fd[drv] < 0)
            {
                (void) showAlert("Cannot open Atari volume image:", drv_image_host_path[drv]);
                drv_image_host_path[drv] = nullptr;
            }
        }

        int fd = drv_image_fd[drv];
        if (fd >= 0)
        {
            uint64_t fsize = drv_image_size[drv];
            uint64_t secsize = 512;     // force 64-bit multiply

            uint64_t new_offset = lrecno * secsize;
            uint64_t new_count = count * secsize;

            if ((new_offset < fsize) && (new_offset + new_count <= fsize))
            {
                off_t offs = lseek(fd, new_offset, SEEK_SET);
                if (offs == (long) new_offset)
                {
                    ssize_t transferred;

                    if (flags & 1)
                    {
                        if (drv_readOnly[drv])
                        {
                            return EWRPRO;
                        }
                        transferred = write(fd, buf, new_count);
                    }
                    else
                    {
                        transferred = read(fd, buf, new_count);
                    }

                    if (transferred == (long) new_count)
                    {
                        return E_OK;
                    }
                    else
                    {
                        return (flags & 1) ? EWRITF : EREADF;
                    }
                }
                else
                {
                    DebugError2("() - lseek() failure");
                    return ATARIERR_ERANGE;
                }
            }
            else
            {
                DebugError2("() - pos %u out of size %u", (unsigned) new_offset, (unsigned) drv_image_size[drv]);
                return ATARIERR_ERANGE;
            }
        }
    }

    return EUNDEV;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: hdv and boot operations
 *
 * @param[in] params            68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return zero or negative error code
 *
 ************************************************************************************************/
uint32_t CVolumeImages::AtariBlockDevice(uint32_t params, uint8_t *addrOffset68k)
{
    uint16_t cmd = getAtariBE16(addrOffset68k + params);
    INT32 aerr = EINVFN;

    struct GetBpbParm
    {
        uint16_t cmd;               // sub-command (big endian)
        uint32_t dskbuf;            // 68k address of 4096-byte buffer address (big endian)
        uint32_t bpb;               // 68k address of 36-byte BPB buffer (big endian)
        uint32_t retaddr68k;        // 68k return address (big endian)
        uint16_t dev;               // drive (big endian)
    } __attribute__((packed));

    struct RwabsParm
    {
        uint16_t cmd;               // sub-command (big endian)
        uint32_t retaddr68k;        // 68 return address (big endian)
        uint16_t flags;             // flags (bit 0: write)
        uint32_t buf;               // 68k buffer address (big endian)
        uint16_t count;             // number of sectors (big endian)
        uint16_t recno;             // sector index (big endian)
        uint16_t dev;               // device or drive (big endian)
        uint32_t lrecno;            // long sector index, if recno = -1 (big endian)
    } __attribute__((packed));

    struct MediachParm
    {
        uint16_t cmd;               // sub-command (big endian)
        uint32_t retaddr68k;        // 68 return address (big endian)
        uint16_t drive;             // drive (big endian)
    } __attribute__((packed));


    uint16_t drv;

    switch(cmd)
    {
        case 1:
            // void hdv_init(void)
            DebugWarning("() - hdv_init() ignored");
            return E_OK;

        case 2:
        {
            // long hdv_rwabs(int flags, void *buf, int count, int recno, int dev)
            // With absense of XHDI we only support 512 bytes per sector.
            RwabsParm *theParams = (RwabsParm *) (addrOffset68k + params);
            uint16_t flags = be16toh(theParams->flags);
            uint32_t buf = be32toh(theParams->buf);
            uint16_t count = be16toh(theParams->count);
            uint16_t recno = be16toh(theParams->recno);
            drv = be16toh(theParams->dev);
            uint32_t lrecno = be32toh(theParams->lrecno);
            DebugInfo2("(drv = %c:) - hdv_rwabs(flags = 0x%04x, buf = 0x%08x, count = %u, recno = %u, lrecno = %u)",
                         'A' + drv, flags, buf, count, recno, drv, lrecno);

            if (recno != 0xffff)
            {
                lrecno = recno; // old API
            }

            aerr = AtariRwabs(drv, flags, count, lrecno, addrOffset68k + buf);
        }
            break;

        case 3:
        {
            // long hdv_getbpb(int drv)
            // Called from DFS_FAT.S, functions drv_open() -> getxbpb()
            //  to check if BIOS knows this drive, e.g. A: or B:
            GetBpbParm *bpbParm = (GetBpbParm *) (addrOffset68k + params);
            drv = be16toh(bpbParm->dev);
            BPB *bpb = (BPB *) (addrOffset68k + be32toh(bpbParm->bpb));
            uint8_t *dskbuf = addrOffset68k + be32toh(bpbParm->dskbuf);
            DebugInfo2("(drv = %c:) - hdv_getbpb()", 'A' + drv);
            aerr = AtariGetBpb(drv, dskbuf, bpb);
            if (aerr == E_OK)
            {
                // return buffer address at success, result will be converted to little-endian
                aerr = be32toh(bpbParm->bpb);
            }
            else
            {
                aerr = 0;   // error
            }
            break;
        }

        case 4:
        {
            // long hdv_mediach(int drive)
            // -> 0 for "not changed"
            // -> 1 for "maybe changed"
            // -> 2 for "was changed"
            MediachParm *theParams = (MediachParm *) (addrOffset68k + params);
            drv = be16toh(theParams->drive);
            DebugInfo2("(drv = %c:) - hdv_mediach()", 'A' + drv);
            if ((drv < NDRIVES) && ((drv_image_fd[drv]) >= 0))
            {
                aerr = 0;
            }
            else
            {
                aerr = EUNDEV;
            }
            break;
        }

        case 5:
            // long hdv_boot()
            DebugWarning2("() - hdv_boot() -- currently ignored");
            aerr = 1;   // currently ignored
            break;
    }

    return aerr;
}


/** **********************************************************************************************
 *
 * @brief Register a volume image
 *
 * @param[in] drv               drive number 0..25
 * @param[in] allocated_path    allocated memory block containing the host path of the volume file
 * @param[in] longnames         true, if long filenames shall be supported
 * @param[in] readonly          true for read-only volume
 * @param[in] size              volume file size in bytes
 *
 * @note called from main thread. TODO: add semaphore?
 *
 ************************************************************************************************/
void CVolumeImages::setNewDrv(uint16_t drv, const char *allocated_path, bool longnames, bool readonly, uint64_t size)
{
    if (drv < NDRIVES)
    {
        drv_image_host_path[drv] = allocated_path;
        drv_image_size[drv] = size;
        drv_longNames[drv] = longnames;
        drv_readOnly[drv] = readonly;
        m_diskimages_drvbits |= (1 << drv);
    }
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: eject
 *
 * @param[in] drv       drive number 0..25
 *
 ************************************************************************************************/
void CVolumeImages::eject(uint16_t drv)
{
    if (drv < NDRIVES)
    {
        if (drv_image_fd[drv] >= 0)
        {
            close(drv_image_fd[drv]);
            drv_image_fd[drv] = -1;
        }

        if (drv_image_host_path[drv] != nullptr)
        {
            free((void *) drv_image_host_path[drv]);
            drv_image_host_path[drv] = nullptr;
        }

        m_diskimages_drvbits &= ~(1 << drv);
    }
}
