/*
 * Copyright (C) 1990-2018 Andreas Kromke, andreas.kromke@gmail.com
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
#include <time.h>
#include <endian.h>
#include <assert.h>
// program headers
#include "Debug.h"
#include "HostHandles.h"


uint8_t *HostHandles::memblock = nullptr;
uint8_t *HostHandles::memblock_free = nullptr;


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
