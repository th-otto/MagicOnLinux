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
#include <assert.h>
#include <fcntl.h>

// program headers
#include "Globals.h"
#include "Debug.h"
#include "HostHandles.h"


#if !defined(_DEBUG_HOST_HANDLES)
 #undef DebugInfo
 #define DebugInfo(...)
 #undef DebugInfo2
 #define DebugInfo2(...)
#endif

/*
 * DD / FD handling
 */


/// table of host FDs
HostFD HostHandles::fdTab[HOST_HANDLE_NUM];


/** **********************************************************************************************
 *
 * @brief Get a free host FD, but do not allocate it, yet
 *
 * @return free host FD
 * @retval nullptr      none is available
 *
 ************************************************************************************************/
HostFD *HostHandles::getFreeHostFD()
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


/** **********************************************************************************************
 *
 * @brief Allocate a free host FD or co-use an already opened one
 *
 * @param[in,out] pfd       in: host FD, whose refcnt is still zero, out: allocated host FD
 *
 * @return host FD handle, 16 bits are suitable for old MAC_XFS API
 *
 * @note When using an already opened host FD, its reference counter is incremented,
 *       and the pointer is changed to that one.
 *
 ************************************************************************************************/
uint16_t HostHandles::allocHostFD(HostFD **pfd)
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


/** **********************************************************************************************
 *
 * @brief Find an open host FD by providing host's device and inode
 *
 * @param[in]  dev      host device descriptor
 * @param[in]  ino      host inode
 * @param[out] hhdl     handle of found host FD
 *
 * @return host FD
 * @retval nullptr      none has been found
 *
 ************************************************************************************************/
HostFD *HostHandles::findHostFD(host_dev_t dev, host_ino_t ino, uint16_t *hhdl)
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


/** **********************************************************************************************
 *
 * @brief Get (opened or free) host FD from 16-bit handle
 *
 * @param[in] hhdl      16-bit handle, suitable for old MAC_XFS
 *
 * @return host FD
 * @retval nullptr      invalid handle
 *
 ************************************************************************************************/
HostFD *HostHandles::getHostFD(uint16_t hhdl)
{
    return (hhdl < HOST_HANDLE_NUM) ? &fdTab[hhdl] : nullptr;
}


/** **********************************************************************************************
 *
 * @brief Free an open host FD
 *
 * @param[in] HostFD    host FD
 *
 ************************************************************************************************/
void HostHandles::freeHostFD(HostFD *fd)
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
 * Fsfirst/Fsnext and Dopendir/Dreaddir/Drewinddir/Dclosedir handling with LRU management
 *
 * LRU is necessary, because the Fsfirst/Fsnext mechanism, copied from MS-DOS
 * does not have any kind of close() mechanism.
 */


/// descriptor for open Dopendir/Dreaddir and Fsfirst/Fsnext searches
struct opendirDescriptor
{
    time_t lru;         // filled with time()
    uint32_t atari_pd;  // Atari process, used for tidy-up
    int dup_fd;         // fd that is generated from dir_fd and used for dopendir()
    DIR *dir;           // host directory descriptor for dreaddir()
};

/// maximum allowed number of parallel Dopendir/Dreaddir and Ffirst/next runs
#define OPENDIR_N     64

/// table of all Snext descriptors
static opendirDescriptor snextTab[OPENDIR_N];


/** **********************************************************************************************
 *
 * @brief Allocate a descriptor for successful Atari Dopendir() or Fsfirst()
 *
 * @param[in] dir       host directory descriptor for dreaddir()
 * @param[in] dup_fd    directory file descriptor, dup-ed from directory descriptor
 * @param[in] act_pd    owning Atari process, used for tidy-up after GEMDOS Pterm()
 *
 * @return 16-bit handle for Snext descriptor, suitable for Atari DTA storage
 *
 * @note Due to last-recently-used (LRU) strategy, this function cannot fail. If all
 *       descriptors are in use, the oldest one will be occupied.
 *
 ************************************************************************************************/
uint16_t HostHandles::allocOpendir(DIR *dir, int dup_fd, uint32_t act_pd)
{
    DebugInfo2("(dir = %p, dup_fd = %d)", dir, dup_fd);
    uint16_t snextHdl = 0xffff;
    time_t oldest_time;

    for (uint16_t i = 0; i < OPENDIR_N; i++)
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

    opendirDescriptor *entry = &snextTab[snextHdl];
    if (entry->lru != 0)
    {
        // overwrite oldest entry -> close old file directory handle
        closeOpendir(snextHdl);
    }

    entry->dir = dir;
    entry->dup_fd = dup_fd;
    entry->lru = time(NULL);
    entry->atari_pd = act_pd;
    DebugInfo2("() => %u", snextHdl);
    return snextHdl;
}


/** **********************************************************************************************
 *
 * @brief Get a descriptor for Atari Dreaddir(), Drewinddir() or Fsnext()
 *
 * @param[in]  snextHdl     16-bit handle for Snext descriptor, suitable for Atari DTA storage
 * @param[in]  act_pd       active Atari process, used for ownership check
 * @param[out] dir          host directory descriptor for dreaddir()
 * @param[out] dup_fd       directory file descriptor, dup-ed from directory descriptor
 *
 * @return 0 for OK or -1 for error (invalid Snext handle)
 *
 * @note For the last-recently-used (LRU) strategy, the descriptor usage time is refreshed.
 *
 ************************************************************************************************/
int HostHandles::getOpendir(uint16_t snextHdl, uint32_t act_pd, DIR **dir, int *dup_fd)
{
    if (snextHdl < OPENDIR_N)
    {
        // handle is valid -> update
        opendirDescriptor *entry = &snextTab[snextHdl];
        if (entry->lru != 0)
        {
            if (entry->atari_pd == act_pd)
            {
                *dir = entry->dir;
                *dup_fd = entry->dup_fd;
                entry->lru = time(NULL);
                DebugInfo2("() => dup_fd = %d", *dup_fd);
                return 0;
            }
        }
    }

    DebugError2("() -- Invalid snext handle %d or owner mismatch", snextHdl);
    return -1;  // error
}


/** **********************************************************************************************
 *
 * @brief Close a descriptor, used by Dclosedir or in case of Atari Fsnext() failure
 *
 * @param[in]  snextHdl     16-bit handle for Snext descriptor, suitable for Atari DTA storage
 *
 * @note This is only called in Dclosedir() or in case of a complete Fsfirst/Fsnext loop, i.e.
 *       until the last Fsnext() fails with "no more files" error.
 *       Otherwise the descriptor remains open.
 *
 ************************************************************************************************/
void HostHandles::closeOpendir(uint16_t snextHdl)
{
    DebugInfo2("(snextHdl = %u)", snextHdl);
    if (snextHdl < OPENDIR_N)
    {
        opendirDescriptor *entry = &snextTab[snextHdl];
        closedir(entry->dir);   // also closes dup_fd
        entry->lru = 0;
    }
}


/** **********************************************************************************************
 *
 * @brief Close all descriptors belonging to the terminated Atari process
 *
 * @param[in]  term_pd      Atari process descriptor
 *
 ************************************************************************************************/
void HostHandles::ptermOpendir(uint32_t term_pd)
{
    uint16_t oldestHdl = 0xffff;
    time_t oldest_time;

    for (uint16_t i = 0; i < OPENDIR_N; i++)
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
            closeOpendir(i);
        }
    }

    // TODO: consider adding an auto-close mechanism, maybe also for fsfirst()
    if (oldestHdl != 0xffff)
    {
        oldest_time = time(NULL) - oldest_time;
        uint32_t oldest_time_min = (uint32_t) (oldest_time / 60);
        if (oldest_time > 10)
        {
            DebugWarning2("() -- Oldest snext handle is %u:%02u minutes. Consider auto close?", oldest_time_min, oldest_time % 60);
            (void) oldest_time_min;
        }
    }
}


/** **********************************************************************************************
*
* @brief Initialisation
*
 ************************************************************************************************/
void HostHandles::init(void)
{
}
