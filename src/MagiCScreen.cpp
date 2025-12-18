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

#include "config.h"
#include <string.h>
#include "MagiCScreen.h"


MXVDI_PIXMAP CMagiCScreen::m_PixMap;
uint32_t CMagiCScreen::m_pColourTable[MAGIC_COLOR_TABLE_LEN];
uint32_t CMagiCScreen::m_logAddr;
uint32_t CMagiCScreen::m_physAddr;
uint16_t CMagiCScreen::m_res;


/** **********************************************************************************************
 *
 * @brief initialisation
 *
 * @return zero for "no error"
 *
 ************************************************************************************************/
int CMagiCScreen::init(void)
{
    memset(&m_PixMap, 0, sizeof(m_PixMap));
    memset(&m_pColourTable, 0, sizeof(m_pColourTable));
    m_logAddr = 0;
    m_physAddr = 0;
    m_res = 0xffff;
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Get colour palette entry as a 16-bit value, for Setcolor()
 *
 * @param[in] index     0..15
 *
 * @return bit pattern 0000rRRRgGGGbBBB, with r/g/b as STe compatible LSB
 *
 * @note The ST palette register entry bit pattern is 00000RRR0GGG0BBB, and each colour
 *       component has three bits.
 *       The STe adds a least significant bit to positions 0000r000g000b000.
 *
 ************************************************************************************************/
uint16_t CMagiCScreen::getColourPaletteEntry(unsigned index)
{
    uint32_t c = CMagiCScreen::m_pColourTable[index];
    // get 8-bit colour components
    uint32_t red   = (c & 0x00ff0000) >> 16;
    uint32_t green = (c & 0x0000ff00) >> 8;
    uint32_t blue  = (c & 0x000000ff) >> 0;
    // shrink to 4 bits, no rounding
    red   >>= 4;
    green >>= 4;
    blue  >>= 4;
    // rearrange for STe format
    red   = ((red   & 1) << 3) | (red   >> 1);
    green = ((green & 1) << 3) | (green >> 1);
    blue  = ((blue  & 1) << 3) | (blue  >> 1);

    // combine
    uint16_t val = (red << 8) | (green << 4) | (blue << 0);
    return val;
}


/** **********************************************************************************************
 *
 * @brief Set colour palette entry from a 16-bit value, for Setpalette() and Setcolor()
 *
 * @param[in] index     0..15
 * @param[in] val       bit pattern 0000rRRRgGGGbBBB, with r/g/b as STe compatible LSB
 *
 * @note The ST palette register entry bit pattern is 00000RRR0GGG0BBB, and each colour
 *       component has three bits.
 *       The STe adds a least significant bit to positions 0000r000g000b000.
 *
 ************************************************************************************************/
void CMagiCScreen::setColourPaletteEntry(unsigned index, uint16_t val)
{
    // get 4-bit colour components
    uint32_t red   = (val & 0x0f00) >> 8;
    uint32_t green = (val & 0x00f0) >> 4;
    uint32_t blue  = (val & 0x000f) >> 0;
    // rearrange from STe format
    red   = ((red   >> 3) | (red   << 1));
    green = ((green >> 3) | (green << 1));
    blue  = ((blue  >> 3) | (blue  << 1));
    // expand to 8 bits
    red   <<= 4;
    green <<= 4;
    blue  <<= 4;
    // rounding
    if (red & 0x10)
    {
        red |= 0xf;
    }
    if (green & 0x10)
    {
        green |= 0xf;
    }
    if (blue & 0x10)
    {
        blue |= 0xf;
    }

    uint32_t c = (red << 16) | (green << 8) | (blue << 0);
    CMagiCScreen::m_pColourTable[index] = c | (0xff000000);
}
