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
* Manages the MagiC screen
*
*/

#ifndef _MAGIC_SCREEN_H
#define _MAGIC_SCREEN_H

#include "Atari.h"

#define MAGIC_COLOR_TABLE_LEN 256

class CMagiCScreen
{
  public:
    static int init();
    static void setColourPaletteEntry(unsigned index, uint16_t val);
    static uint16_t getColourPaletteEntry(unsigned index);

	static MXVDI_PIXMAP m_PixMap;
	static uint32_t m_pColourTable[MAGIC_COLOR_TABLE_LEN];
    static uint32_t m_logAddr;      // logical 68k address of video memory
    static uint32_t m_physAddr;     // physical 68k address of video memory
    static uint16_t m_res;          // desired resolution, usually 0xffff
};

#endif
