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

#include <stdint.h>

#define HOST_HANDLE_NUM		1024			// number of memory blocks
#define HOST_HANDLE_INVALID	0xffffffff
#define HOST_HANDLE_SIZE 	64				// size of one memory block

typedef uint32_t HostHandle_t;

class HostHandles
{
  public:
    static void init();
    static uint32_t alloc(unsigned size);
    static void *getData(HostHandle_t hhdl);
    static void free(HostHandle_t hhdl);

  private:
    static uint8_t *memblock;
	  static uint8_t *memblock_free;
};
