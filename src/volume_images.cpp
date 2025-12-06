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
#include <assert.h>
#include "Debug.h"
#include "conversion.h"
#include "gui.h"
#include "volume_images.h"
#include "mbr_vbr.h"

#if !defined(_DEBUG_VOLUME_IMAGES)
 #undef DebugInfo
 #define DebugInfo(...)
 #undef DebugInfo2
 #define DebugInfo2(...)
#endif

CVolumeImages::drv_image *CVolumeImages::drv_images[NDRVIMAGES];      // nullptr, if not valid
CVolumeImages::drv_volume *CVolumeImages::drv_volumes[NDRIVES];      // nullptr, if not valid
uint32_t CVolumeImages::m_drvbits;


/** **********************************************************************************************
 *
 * @brief get disk images from preferences
 *
 ************************************************************************************************/
void CVolumeImages::init()
{
    struct stat statbuf;

    m_drvbits = 0;
    for (int drv = 0; drv < NDRIVES; drv++)
    {
        const char *path = Preferences::drvPath[drv];
        if (path == nullptr)
        {
            continue;
        }

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
            DebugInfo2("() -- Atari drive %c: is not a regular file: \"%s\"", 'A' + drv, path);
            path = nullptr;
            continue;
        }

        uint64_t size = statbuf.st_size;
        unsigned flags = Preferences::drvFlags[drv];
        bool longnames = (flags & DRV_FLAG_8p3) == 0;       // currently unused
        bool readonly = (flags & DRV_FLAG_RDONLY);
        const char *allocated_path = CConversion::copyString(path);
        setNewDrv(drv, allocated_path, longnames, readonly, size);
    }
}


/** **********************************************************************************************
 *
 * @brief close all disk images
 *
 ************************************************************************************************/
void CVolumeImages::exit(void)
{
    for (unsigned drv = 0; drv < NDRIVES; drv++)
    {
        eject(drv);
    }

    assert(m_drvbits == 0);

    for (unsigned i = 0; i < NDRVIMAGES; i++)
    {
        assert(drv_images[i] == nullptr);
    }
}


/** **********************************************************************************************
 *
 * @brief Check if image has already been registered
 *
 * @param[in]  path         disk image path
 * @param[out] imgno        index in drv_image[]
 *
 * @return true: found, false return first free index or -1
 *
 ************************************************************************************************/
bool CVolumeImages::findImagePath(const char *path, int *imgno)
{
    *imgno = -1;
    for (int i = 0; i < NDRVIMAGES; i++)
    {
        if (drv_images[i] != nullptr)
        {
            if (!strcmp(drv_images[i]->host_path, path))
            {
                *imgno = i;     // index of found image
                return true;
            }
        }
        else
        {
            if (*imgno < 0)
            {
                *imgno = i;
            }
        }
    }

    return false;
}


/** **********************************************************************************************
 *
 * @brief Check for MBR
 *
 * @param[in]  sector       boot sector
 * @param[out] partitions   partition table
 * @param[in]  maxparts     length of that table
 *
 * @return number of found partitions, less or equal to maxparts, or -1 for "no MBR"
 * @retval -1  no MBR
 * @retval -2  corrupt MBR
 * @retval 0   no usable partitions found
 *
 ************************************************************************************************/
int CVolumeImages::getMbr(const uint8_t *sector, partition *partitions, unsigned maxparts)
{
    const MBR *mbr = (const MBR *) sector;

    if ((mbr->sig0 != 0x55) || (mbr->sig1 != 0xaa))
    {
        DebugInfo2("() -- No MBR signature");
        return -1;
    }

    DebugInfo2("() -- MBR signature found");

    //
    // sanity check: partition overlap
    //

    uint32_t lba_first[MBR_NO_PRIMARY_PARTITIONS];
    uint32_t lba_num[MBR_NO_PRIMARY_PARTITIONS];

    for (unsigned i = 0; i < MBR_NO_PRIMARY_PARTITIONS; i++)
    {
        const MBR_PART *part = mbr->partitions + i;
        lba_first[i] = (part->lba_first[0]) | (part->lba_first[1] << 8) | (part->lba_first[2] << 16) | (part->lba_first[3] << 24);
        lba_num[i]   = (part->lba_num[0]) | (part->lba_num[1] << 8) | (part->lba_num[2] << 16) | (part->lba_num[3] << 24);

        uint32_t first = lba_first[i];
        uint32_t last = lba_first[i] + lba_num[i] - 1;
        for (unsigned j = 0; j < i; j++)
        {
            if (((first >= lba_first[j]) && (first < lba_first[j] + lba_num[j])) ||
                ((last >= lba_first[j]) && (last < lba_first[j] + lba_num[j])) ||
                ((first < lba_first[j]) && (last >= lba_first[j] + lba_num[j])))
            {
                DebugError2("() -- MBR partitions %u and %u overlap. Not mounted.", j, i);
                return 0;
            }
        }
    }

    //
    // walk and gather
    //

    unsigned numparts = 0;
    uint64_t secsiz = 512;      // force 64-bit multiplication

    for (unsigned i = 0; i < MBR_NO_PRIMARY_PARTITIONS; i++)
    {
        const MBR_PART *part = mbr->partitions + i;

        int type = 0;
        const char *description;
        switch(part->part_type)
        {
            case 0:
                description = "empty (ignored)";
                break;

            case 1:
                description = "FAT12";
                type = 12;
                break;

            case 4:
                description = "FAT16 < 65,536 sectors";
                type = 16;
                break;

            case 5:
                description = "Extended partition CHS (ignored)";
                break;

            case 6:
                description = "FAT16B >= 65,536 sectors";
                type = 16;
                break;

            case 0x0b:
                description = "FAT32 CHS (ignored)";
                break;

            case 0x0c:
                description = "FAT32 LBA";
                type = 32;
                break;

            case 0x0e:
                description = "FAT16B LBA";
                type = 16;
                break;

            case 0x0f:
                description = "Extended partition LBA (ignored)";
                break;

            default:
                description = "unhandled (ignored)";
                break;
        }

        DebugInfo2("() -- Partition #%u type %u (%s) LBA %u .. %u, size %u sectors",
                         i, part->part_type, description, lba_first[i], lba_first[i] + lba_num[i], lba_num[i]);
        (void) description;

        if ((type > 0) && (numparts < maxparts))
        {
            partitions->start_offs = secsiz * lba_first[i];
            partitions->size = secsiz * lba_num[i];
            partitions->type = type;
            partitions++;
            numparts++;
        }
    }

    return numparts;
}


/** **********************************************************************************************
 *
 * @brief Converts boot sector an image to BPB (FAT12 or FAT16), if any
 *
 * @param[in]  sector   boot sector
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
    if (reserved_sectors == 0)
    {
        DebugError2("() -- No reserved sectors, should be 1. Continue with 1.");
        reserved_sectors = 1;
    }

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
 * @brief Check if the sector is a FAT32 VBR
 *
 * @param[out] sector      boot sector
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
    INT32 aerr = AtariRwabs(drv, 0, 1, 0, dskbuf);
    if (aerr == E_OK)
    {
        aerr = vbr2Bpb(dskbuf, bpb);
    }
    return aerr;
}


/** **********************************************************************************************
 *
 * @brief Check if the file contains a mountable drive or volume image, for Drag&Drop
 *
 * @param[in]  path     path to image file
 *
 * @return 12 for FAT12, 16 for FAT16, 32 for FAT32, 0 for MBR, -2 for format error and -1 for file error
 *
 ************************************************************************************************/
int CVolumeImages::checkFatVolume(const char *path)
{
    //return 12;
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
    if (aerr == E_OK)
    {
        free(dskbuf);
        return 32;
    }

    // 3. Try MBR

    partition part_tab[8];
    int num_parts = getMbr(dskbuf, part_tab, 8);
    free(dskbuf);
    return (num_parts > 0) ? 0 : -2;
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

    if ((drv >= NDRIVES) || (drv_volumes[drv] == nullptr))
    {
        return EUNDEV;
    }

    if (flags & 2)
    {
        DebugError2("() - Physical sector numbers are not (yet) supported");
        return EUNCMD;
    }

    // open disk image, if not already done, and get partitions, if any

    drv_volume *volume = drv_volumes[drv];
    if (!openDrvImage(volume->image_no))
    {
        return EUNDEV;
    }

    drv_image *image = drv_images[volume->image_no];
    assert(image != nullptr);
    unsigned partno = volume->partition_no;

    if ((int) partno >= image->nparts)
    {
        DebugError2("() - Partition %u on disk image %u is invalid", partno, volume->image_no);
        return EUNDEV;
    }

    int fd = image->fd;
    uint64_t part_offs = image->partitions[partno].start_offs;
    uint64_t part_size = image->partitions[partno].size;
    uint64_t secsize = 512;     // force 64-bit multiply

    uint64_t new_offset = lrecno * secsize;
    uint64_t new_count = count * secsize;

    if ((new_offset < part_size) && (new_offset + new_count <= part_size))
    {
        off_t offs = lseek(fd, part_offs + new_offset, SEEK_SET);
        if (offs == (long) (part_offs + new_offset))
        {
            ssize_t transferred;

            if (flags & 1)
            {
                if (volume->read_only)
                {
                    return EWRPRO;
                }
                transferred = write(fd, buf, new_count);
                volume->sec_written = true;
            }
            else
            {
                if (lrecno == 0)
                {
                    // We can use this as sign that there was an attempt to
                    // mount the volume.
                    volume->bootsec_read = true;
                }
                else
                {
                    // We can use this as sign that the MagiC BIOS accepted
                    // the boot sector and mounted the volume.
                    volume->non_bootsec_read = true;
                }
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
        DebugError2("() - pos %u out of size %u", (unsigned) new_offset, (unsigned) image->size);
        return ATARIERR_ERANGE;
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
            if ((drv < NDRIVES) && (drv_volumes[drv] != nullptr))
            {
                drv_volume *volume = drv_volumes[drv];
                drv_image *image = drv_images[volume->image_no];
                assert(image != nullptr);
                aerr = (image->fd >= 0) ? 0 : EUNDEV;
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
 * @brief Open a drive image and get partitions, if any and if not already done
 *
 * @param[in] image_no          image index
 *
 * @return true for success, false for failure
 *
 ************************************************************************************************/
bool CVolumeImages::openDrvImage(unsigned image_no)
{
    if ((image_no >= NDRVIMAGES) || (drv_images[image_no] == nullptr))
    {
        DebugError2("() -- wrong parameters");
        return false;    // wrong parameters
    }

    drv_image *image = drv_images[image_no];
    if (image->fd >= 0)
    {
        return true;    // already open
    }

    if (image->fd == -2)
    {
        DebugError2("() -- disk image not usable");
        return false;    // error state
    }

    // not yet open
    // check if all volumes shall be mounted as read-only

    bool disk_image_read_only = true;
    for (unsigned pi = 0; pi < NPARTITIONS; pi++)
    {
        char cdrv = image->partitions[pi].drv;
        if (cdrv >= 'A')
        {
            unsigned drv = (unsigned) (cdrv - 'A');
            assert(drv < NDRIVES);
            drv_volume *volume = drv_volumes[drv];
            assert(volume != nullptr);
            if (!volume->read_only)
            {
                DebugInfo("() -- disk image must be opened in read/write mode");
                disk_image_read_only = true;    // TODO: remember?
                break;
            }
        }
    }

    // open disk image file as either read-write or read-only

    image->fd = open(image->host_path, disk_image_read_only ? O_RDONLY : O_RDWR);
    if (image->fd < 0)
    {
        (void) showAlert("Cannot open disk image:", image->host_path);
        image->fd = -2;     // mark as errorneous
        return false;
    }

    // read first sector and check if it contains an MBR partion table

    uint8_t *dskbuf = (uint8_t *) malloc(512);
    ssize_t bytes = read(image->fd, dskbuf, 512);
    if (bytes != 512)
    {
        free(dskbuf);
        (void) showAlert("Cannot read disk image first sector:", image->host_path);
        close(image->fd);
        image->fd = -2;     // mark as errorneous
        return false;
    }

    // get partition table if any

    image->nparts = getMbr(dskbuf, image->partitions, NPARTITIONS);
    if (image->nparts == -2)
    {
        (void) showAlert("Corrupted MBR on disk image:", image->host_path);
        close(image->fd);
        image->fd = -2;     // mark as errorneous
        return false;
    }

    if (image->nparts == 0)
    {
        (void) showAlert("No readable partitions on disk image:", image->host_path);
        close(image->fd);
        image->fd = -2;     // mark as errorneous
        return false;
    }

    if (image->nparts == -1)
    {
        // disk image seems to be un-partitioned. Treat as one partition.
        DebugInfo2("() -- Disk image seems be un-partitioned: %s", image->host_path);
        image->partitions[0].start_offs = 0;
        image->partitions[0].size = image->size;
        image->partitions[0].type = 0;
        image->nparts = 1;
    }

    return true;
}


/** **********************************************************************************************
 *
 * @brief Register a drive image and a volume image
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
    if ((drv >= NDRIVES) || (allocated_path == nullptr))
    {
        DebugError2("() -- invalid parameters");
        return;
    }

    int imgno;
    if (!findImagePath(allocated_path, &imgno))
    {
        if (imgno >= 0)
        {
            // new image
            drv_image *image = (drv_image *) calloc(1, sizeof(*image));
            image->host_path = allocated_path;
            image->fd = -1;
            image->size = size;
            drv_images[imgno] = image;
        }
        else
        {
            DebugError2("() -- Too many disk images, ignore drive %c:", drv + 'A');
            free((void *) allocated_path);
            return;
        }
    }

    if (imgno >= 0)
    {
        // image registered in drv_images[imgno]
        // look for unregistered partition

        drv_image *image = drv_images[imgno];
        assert(image != nullptr);
        int partno;
        for (partno = 0; partno < NPARTITIONS; partno++)
        {
            if (image->partitions[partno].drv == 0)
            {
                break;
            }
        }
        if (partno < NPARTITIONS)
        {
            //
            // The logical Atari drive will get the first free partition of an already
            // registered disk image.
            //

            DebugInfo2("() -- Atari drive %c: is partition %u in disk image %u", drv + 'A', partno, imgno);
            drv_volume *volume = (drv_volume *) calloc(1, sizeof(*volume));
            volume->image_no = imgno;
            volume->partition_no = partno;
            volume->read_only = readonly;
            volume->long_names = longnames;       // currently unused
            drv_volumes[drv] = volume;
            image->partitions[partno].drv = 'A' + drv;
            m_drvbits |= (1 << drv);
        }
        else
        {
            DebugError2("() -- No unused partition for Atari drive %c: in disk image %u", drv + 'A', imgno);
            free((void *) allocated_path);
            return;
        }
    }

    m_drvbits |= (1 << drv);
}


/** **********************************************************************************************
 *
 * @brief Hack to remove volumes that were tried to be mounted, but failed
 *
 ************************************************************************************************/
void CVolumeImages::remove_failed_volumes()
{
    for (unsigned drv = 0; drv < NDRIVES; drv++)
    {
        drv_volume *volume = drv_volumes[drv];
        if (volume != nullptr)
        {
            if (volume->bootsec_read && !volume->non_bootsec_read && !volume->sec_written)
            {
                DebugInfo2("() - logical drive %u: presumably failed to mount, remove it", drv);
                eject(drv);
            }
        }
    }
}


/** **********************************************************************************************
 *
 * @brief Eject a logical volume, also eject image, if no longer needed
 *
 * @param[in] drv       drive number 0..25
 *
 ************************************************************************************************/
void CVolumeImages::eject(uint16_t drv)
{
    if ((drv < NDRIVES) && (drv_volumes[drv] != nullptr))
    {
        drv_volume *volume = drv_volumes[drv];
        drv_volumes[drv] = nullptr;             // remove logical drive
        m_drvbits &= ~(1 << drv);

        unsigned image_no = volume->image_no;
        drv_image *image = drv_images[image_no];
        assert(image != nullptr);
        assert(volume->partition_no < NPARTITIONS);
        assert(image->partitions[volume->partition_no].drv == 'A' + drv);
        image->partitions[volume->partition_no].drv = 0;    // set partition to "unused"
        free(volume);

        // also free disk image, if now unused

        for (unsigned pi = 0; pi < NPARTITIONS; pi++)
        {
            if (image->partitions[pi].drv != 0)
            {
                DebugInfo2("() - disk image %u still in use", image_no);
                return;
            }
        }

        DebugInfo2("() - disk image %u no longer in use, close it", image_no);

        if (image->fd >= 0)
        {
            close(image->fd);
            image->fd = -1;
        }

        if (image->host_path != nullptr)
        {
            free((void *) image->host_path);
            image->host_path = nullptr;
        }

        drv_images[image_no] = nullptr;
        free(image);
    }
}
