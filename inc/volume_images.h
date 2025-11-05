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


// static class
class CVolumeImages
{
   public:
    static void init(uint32_t *new_drvbits);
    static void exit(void);

    static uint32_t AtariBlockDevice(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariGetBpb(uint16_t drv, uint8_t *dskbuf, BPB *bpb);
    static uint32_t AtariRwabs(uint16_t drv, uint16_t flags, uint16_t count, uint32_t lrecno, uint8_t *buf);
    static bool isDrvValid(uint16_t drv) { return ((drv < NDRIVES) && (drv_image_host_path[drv] != nullptr)); }
    static void eject(uint16_t drv);

    // Atari volume images
    static const char *drv_image_host_path[NDRIVES];       // nullptr, if not valid
    static int drv_image_fd[NDRIVES];
    static uint64_t drv_image_size[NDRIVES];
    static bool drv_longNames[NDRIVES];              // initialised with zeros
    static bool drv_readOnly[NDRIVES];
    static uint32_t m_diskimages_drvbits;
};
