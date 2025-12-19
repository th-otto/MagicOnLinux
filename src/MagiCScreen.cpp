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
#include <assert.h>
#include "MagiCScreen.h"


MXVDI_PIXMAP CMagiCScreen::m_PixMap;
uint32_t CMagiCScreen::m_pColourTable[MAGIC_COLOR_TABLE_LEN];
void *CMagiCScreen::pixels;
unsigned CMagiCScreen::pixels_size;
SDL_Surface *CMagiCScreen::m_sdl_atari_surface;        // surface in Atari native pixel format or NULL
SDL_Surface *CMagiCScreen::m_sdl_host_surface;         // surface in host native pixel format
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

    pixels = nullptr;
    pixels_size = 0;
    m_sdl_atari_surface = nullptr;
    m_sdl_host_surface = nullptr;

    m_logAddr = 0;
    m_physAddr = 0;
    m_res = 0xffff;
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Initialise the PIXMAP needed for MVDI
 *
 * @param[in] pixelType     0:indexed, 16: RGB direct
 * @param[in] cmpCount      1:indexed, 3: RGB direct
 * @param[in] cmpSize       bit depth for indexed modes, 8 for RGB direct
 * @param[in] planeBytes    2:interleaved plane, 0: packed or RGB direct
 *
 ************************************************************************************************/
void CMagiCScreen::init_pixmap(uint16_t pixelType, uint16_t cmpCount, uint16_t cmpSize, uint32_t planeBytes)
{
    // create ancient style Pixmap structure to be passed to the Atari kernel, from m_sdl_surface
    assert(m_sdl_atari_surface->pitch < 0x4000);            // Pixmap limit and thus limit for Atari
    assert((m_sdl_atari_surface->pitch & 3) == 0);          // pitch (alias rowBytes) must be dividable by 4

    // The baseAddr is supposed to be m_sdl_atari_surface->pixels, but this does no longer work
    // with 64-bit host computer. However, the baseAddr will be changed later anyway
    m_PixMap.baseAddr      = 0x8000000;                    // dummy target address, later filled in by emulator
    m_PixMap.rowBytes      = m_sdl_atari_surface->pitch | 0x8000;    // 0x4000 and 0x8000 are flags
    m_PixMap.bounds_top    = 0;
    m_PixMap.bounds_left   = 0;
    m_PixMap.bounds_bottom = m_sdl_atari_surface->h;
    m_PixMap.bounds_right  = m_sdl_atari_surface->w;
    m_PixMap.pmVersion     = 4;                            // should mean: pixmap base address is 32-bit address
    m_PixMap.packType      = 0;                            // unpacked?
    m_PixMap.packSize      = 0;                            // unimportant?
    m_PixMap.pixelType     = pixelType;                    // 16 is RGBDirect, 0 would be indexed
    m_PixMap.pixelSize     = m_sdl_atari_surface->format->BitsPerPixel;
    m_PixMap.cmpCount      = cmpCount;                     // components: 3 = red, green, blue, 1 = monochrome
    m_PixMap.cmpSize       = cmpSize;                      // True colour: 8 bits per component
    m_PixMap.planeBytes    = planeBytes;                   // offset to next plane
    m_PixMap.pmTable       = 0;
    m_PixMap.pmReserved    = 0;
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
