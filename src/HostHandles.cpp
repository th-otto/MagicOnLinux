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
 */

/*
*
* Manages internal memory blocks (handles)
*
*/

// system headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <endian.h>
#include <assert.h>
#include <fcntl.h>
// program headers
#include "Debug.h"
#include "HostHandles.h"



/*
 * DD / FD handling
 */


static HostFD fdTab[HOST_HANDLE_NUM];
HostFD *getFreeHostFD()
{
    HostFD *p = fdTab;
    for (unsigned n = 0; n < HOST_HANDLE_NUM; n++, p++)
    {
        if (p->ref_cnt == 0)
        {
            return p;
        }
    }
    return nullptr;
}
uint16_t allocHostFD(HostFD **pfd)
{
    HostFD *fd = *pfd;
    assert(fd->ref_cnt == 0);
    uint16_t handle = 0xffff;
    HostFD *p = fdTab;
    for (unsigned n = 0; n < HOST_HANDLE_NUM; n++, p++)
    {
        if (p == fd)
        {
            handle = n;
        }
        else
        if (p->ref_cnt != 0)
        {
            // check if this descriptor references the same file or directory
            if ((p->dev == fd->dev) &&
                (p->ino == fd->ino))
               {
                    // FWFR it seems to happen, that we get the same file descriptor
                    // for the same directory. Thus we may not close it here,
                    // we just use the existing HostFD.
                    if (p->fd == fd->fd)
                    {
                        // new and old host fd are the same?!?
                        DebugInfo("%s() - FWFR got host fd %d twice", __func__, fd->fd);
                    }
                    else
                    {
                        // close new host fd, as we can use the old one
                        DebugInfo("%s() - close new host fd %d", __func__, fd->fd);
                        close(fd->fd);  // do not use it
                    }
                    handle = n;     // this one has already been opened, reuse it
                    fd = p;
                    *pfd = fd;
                    break;
               }
        }
    }

    fd->ref_cnt++;
    return handle;
}
HostFD *findHostFD(__dev_t dev, __ino_t ino, uint16_t *hhdl)
{
    HostFD *p = fdTab;
    for (unsigned n = 0; n < HOST_HANDLE_NUM; n++, p++)
    {
        if (p->ref_cnt != 0)
        {
            // check if this descriptor references the same file or directory
            if ((p->dev == dev) &&
                (p->ino == ino))
               {
                    p->ref_cnt++;
                    *hhdl = n;
                    return p;
               }
        }
    }

    return nullptr;
}

HostFD *getHostFD(uint16_t hhdl)
{
    return (hhdl < HOST_HANDLE_NUM) ? &fdTab[hhdl] : nullptr;
}
void freeHostFD(HostFD *fd)
{
    assert(fd->ref_cnt > 0);
    fd->ref_cnt--;
    if (fd->ref_cnt == 0)
    {
        close(fd->fd);
        fd->fd = -1;    // to be sure..
    }
}


/*
 * Fsfirst / Fsnext handling
 */

// maximum allowed number of parallel Ffirst/next runs
#define SNEXT_N     64


uint8_t *HostHandles::memblock = nullptr;
uint8_t *HostHandles::memblock_free = nullptr;

// Fsfirst/Fsnext LRU management


struct SnextEntry
{
    time_t lru;         // filled with time()
    uint32_t atari_pd;  // Atari process, used for tidy-up
    int dup_fd;         // fd that is generated from dir_fd and used for dopendir()
    DIR *dir;
};

static SnextEntry snextTab[SNEXT_N];


uint16_t HostHandles::snextSet(DIR *dir, int dup_fd, uint32_t act_pd)
{
    uint16_t snextHdl = 0xffff;
    time_t oldest_time;

    for (uint16_t i = 0; i < SNEXT_N; i++)
    {
        if (snextTab[i].lru == 0)
        {
            snextHdl = i;
            break;  // free entry found
        }
        if ((snextHdl == 0xffff) || (snextTab[i].lru < oldest_time))
        {
            snextHdl = i;
            oldest_time = snextTab[i].lru;
        }
    }

    SnextEntry *entry = &snextTab[snextHdl];
    if (entry->lru != 0)
    {
        // overwrite oldest entry -> close old file directory handle
        snextClose(snextHdl);
    }
    entry->dir = dir;
    entry->dup_fd = dup_fd;
    entry->lru = time(NULL);
    entry->atari_pd = act_pd;
    return snextHdl;
}
int HostHandles::snextGet(uint16_t snextHdl, DIR **dir, int *dup_fd)
{
    if (snextHdl < SNEXT_N)
    {
        // handle is valid -> update
        SnextEntry *entry = &snextTab[snextHdl];
        if (entry->lru != 0)
        {
            *dir = entry->dir;
            *dup_fd = entry->dup_fd;
            entry->lru = time(NULL);
            return 0;
        }
    }

    DebugError2("() -- Invalid snext handle %d", snextHdl);
    return -1;  // error
}
void HostHandles::snextClose(uint16_t snextHdl)
{
    if (snextHdl < SNEXT_N)
    {
        SnextEntry *entry = &snextTab[snextHdl];
        closedir(entry->dir);   // also closed dup_fd
        // entry->dup
        /*
        HostHandle_t hhdl = entry->hhdl;
        HostFD *hostFD = getHostFD(hhdl);
        if (hostFD == nullptr)
        {
            return;
        }
        int dir_fd = hostFD->fd;
        if (dir_fd == -1)
        {
            DebugError("Invalid directory file descriptor");
        }
        else
        {
            DebugInfo("Closing directory file descriptor %d", dir_fd);
            close(dir_fd);
        }
        freeHostFD(hostFD);
        */
        entry->lru = 0;
    }
}
void HostHandles::snextPterm(uint32_t term_pd)
{
    uint16_t oldestHdl = 0xffff;
    time_t oldest_time;

    for (uint16_t i = 0; i < SNEXT_N; i++)
    {
        if (snextTab[i].lru == 0)
        {
            continue;  // skip free entries
        }
        if ((oldestHdl == 0xffff) || (snextTab[i].lru < oldest_time))
        {
            oldestHdl = i;
            oldest_time = snextTab[i].lru;
        }

        if (snextTab[i].atari_pd == term_pd)
        {
            DebugInfo2("() Closing orphaned snext handle %u", i);
            snextClose(i);
        }
    }

    // TODO: consider adding an auto-close mechanism, maybe also for fsfirst()
    if (oldestHdl != 0xffff)
    {
        oldest_time = time(NULL) - oldest_time;
        uint32_t oldest_time_min = (uint32_t) (oldest_time / 60);
        DebugWarning2("() Oldest snext handle is %u minutes. Consider auto close?", oldest_time_min);
    }
}


/** **********************************************************************************************
*
* @brief Initialisation
*
 ************************************************************************************************/
void HostHandles::init(void)
{
    assert(memblock == nullptr);        // not yet initialised

    // allocate blocks
    memblock = (uint8_t *) malloc(HOST_HANDLE_NUM * HOST_HANDLE_SIZE);
    // allocate free-flags
    memblock_free = (uint8_t *) calloc(HOST_HANDLE_NUM, 1);
}


#if 0
/** **********************************************************************************************
*
* @brief Allocate
*
 ************************************************************************************************/
uint32_t HostHandles::alloc(unsigned size)
{
    assert(size <= HOST_HANDLE_SIZE);

    // search for free block
    uint8_t *pfree = memblock_free;
    for (unsigned i = 0; i < HOST_HANDLE_NUM; i++, pfree++)
    {
        if (*pfree == 0)
        {
            *pfree = 1;
            return i;
        }
    }

    DebugError("No host handles left");
    return HOST_HANDLE_INVALID;
}


/** **********************************************************************************************
*
* @brief Allocate
*
 ************************************************************************************************/
uint32_t HostHandles::allocInt(int v)
{
    HostHandle_t hhdl = alloc(sizeof(v));
    if (hhdl != HOST_HANDLE_INVALID)
    {
        HostHandles::putInt(hhdl, v);
    }
    return hhdl;
}


/** **********************************************************************************************
 *
 * @brief Get raw data from handle
 *
 * @param[in]  hhdl    32-bit value passed from MagiC kernel
 *
 * @return nullptr in case of an error
 *
 ************************************************************************************************/
void *HostHandles::getData(HostHandle_t hhdl)
{
    if ((hhdl >= HOST_HANDLE_NUM) || (memblock_free[hhdl] == 0))
    {
        DebugError("Invalid host handle %d", hhdl);
        return nullptr;
    }
    return memblock + hhdl * HOST_HANDLE_SIZE;
}


/** **********************************************************************************************
 *
 * @brief Get integer value from handle
 *
 * @param[in]  hhdl    32-bit value passed from or to MagiC kernel
 *
 * @return -1 in case of an error, otherwise the read value
 *
 ************************************************************************************************/
int HostHandles::getInt(HostHandle_t hhdl)
{
    int *p = (int *) getData(hhdl);
    return (p == nullptr) ? -1 : *p;
}


/** **********************************************************************************************
 *
 * @brief Put integer value to handle
 *
 * @param[in]  hhdl    32-bit value passed from or to MagiC kernel
 * @param[in]  v       value to store
 *
 ************************************************************************************************/
void HostHandles::putInt(HostHandle_t hhdl, int v)
{
    int *p = (int *) getData(hhdl);
    if (p != nullptr)
    {
        *p = v;
    }
}


/** **********************************************************************************************
*
* @brief free handle
*
*************************************************************************************************/
void HostHandles::free(HostHandle_t hhdl)
{
    assert(hhdl < HOST_HANDLE_NUM);
    assert(memblock_free[hhdl] == 1);
    memblock_free[hhdl] = 0;
}
#endif