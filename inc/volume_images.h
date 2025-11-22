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

#include <stdint.h>
#include "Atari.h"
#include "preferences.h"


#define NDRVIMAGES  16
#define NPARTITIONS 8

// Atari volume images (static class)
class CVolumeImages
{
   public:
    static void init(void);
    static void exit(void);

    static uint32_t AtariBlockDevice(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t vbr2Bpb(const uint8_t *sector, BPB *bpb);
    static uint32_t vbr2Fat32(const uint8_t *sector);
    static uint32_t AtariGetBpb(uint16_t drv, uint8_t *dskbuf, BPB *bpb);
    static int checkFatVolume(const char *path);
    static uint32_t AtariRwabs(uint16_t drv, uint16_t flags, uint16_t count, uint32_t lrecno, uint8_t *buf);
    static bool isDrvValid(uint16_t drv) { return ((drv < NDRIVES) && (drv_volumes[drv] != nullptr)); }
    static void eject(uint16_t drv);
    static void setNewDrv(uint16_t drv, const char *allocated_path, bool longnames, bool readonly, uint64_t size);
    static uint32_t getDrvBits() { return m_drvbits; }

   private:
    struct partition
    {
        uint64_t start_offs;    // in bytes
        uint64_t size;
        int type;               // partition type or 0 for "whole disk, no partitions"
        char drv;               // 'A' .. 'Z' or 0 if unused
    };
    struct drv_image
    {
        const char *host_path;
        int fd;                 // initialised with -1, set on open, -2 on error
        partition partitions[NPARTITIONS];
        int nparts;             // number of readable partitions
        uint64_t size;          // set on creation of object
    };

    struct drv_volume
    {
        unsigned image_no;
        unsigned partition_no;
        bool long_names;            // currently ignored
        bool read_only;
    };

    static drv_image *drv_images[NDRVIMAGES];      // nullptr, if not valid
    static drv_volume *drv_volumes[NDRIVES];      // nullptr, if not valid

    static int getMbr(const uint8_t *sector, partition *partitions, unsigned maxparts);
    static bool openDrvImage(unsigned image_no);
    static bool findImagePath(const char *path, int *imgno);
    static uint32_t m_drvbits;
};
