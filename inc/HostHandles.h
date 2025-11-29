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
* Manages internal memory blocks (handles)
*
*/

#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>

#define HOST_FH_SIZE MAX_HANDLE_SZ   // 128 bytes, while in fact 8 bytes are enough

// Platform compatibility for device and inode types
typedef dev_t host_dev_t;
typedef ino_t host_ino_t;

#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wzero-length-array"
#pragma GCC diagnostic ignored "-Wpedantic"
/// Host File Descriptor
/// Describes a file or a directory, without opening it.
struct HostFD
{
    int ref_cnt;     // 0 = unused
    host_dev_t dev;  // Device, retrieved from struct stat
    host_ino_t ino;  // File serial number (inode), retrieved from struct stat
    int fd;          // open file handle
    // Maybe better store the host path here?
    // Maybe also store Atari drive here?
};
#pragma GCC diagnostic pop

HostFD *getFreeHostFD();
uint16_t allocHostFD(HostFD **pfd);
void freeHostFD(HostFD *fd);
HostFD *getHostFD(uint16_t hhdl);
HostFD *findHostFD(host_dev_t dev, host_ino_t ino, uint16_t *hhdl);


#define HOST_HANDLE_NUM     1024            // number of memory blocks
#define HOST_HANDLE_INVALID 0xffffffff
#define HOST_HANDLE_SIZE    64                // size of one memory block

typedef uint32_t HostHandle_t;

#if (HOST_HANDLE_NUM > 32768)
#error "For historical reasons, open file handles must fit to 16 bits"
#endif

class HostHandles
{
  public:
    static void init();
    #if 0
    static uint32_t alloc(unsigned size);
    static uint32_t allocInt(int v);
    static void *getData(HostHandle_t hhdl);
    static int getInt(HostHandle_t hhdl);
    static void putInt(HostHandle_t hhdl, int v);
    static void free(HostHandle_t hhdl);
    #endif
    static uint16_t snextSet(DIR *dir, int dup_fd, uint32_t act_pd);
    static int snextGet(uint16_t snextHdl, DIR **dir, int *dup_fd);
    static void snextClose(uint16_t snextHdl);
    static void snextPterm(uint32_t term_pd);

  private:
    static uint8_t *memblock;
    static uint8_t *memblock_free;
};
