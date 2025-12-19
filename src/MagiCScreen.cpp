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
#include "Debug.h"
#include "preferences.h"
#include "emulation_globals.h"
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

    m_sdl_atari_surface = nullptr;
    m_sdl_host_surface = nullptr;

    m_logAddr = 0;
    m_physAddr = 0;
    m_res = 0xffff;
    return 0;
}


/** **********************************************************************************************
 *
 * @brief de-initialisation
 *
 * @return zero for "no error"
 *
 ************************************************************************************************/
void CMagiCScreen::exit(void)
{
    if (pixels != nullptr)
    {
        free(pixels);
        pixels = nullptr;
        pixels_size = 0;
    }
}


/** **********************************************************************************************
 *
 * @brief Create Atari SDL surfaces
 *
 ************************************************************************************************/
void CMagiCScreen::create_surfaces()
{
    // SDL stuff
    Uint32 rmask = 0;
    Uint32 gmask = 0;
    Uint32 bmask = 0;
    Uint32 amask = 0;
    // Pixmap stuff
    short pixelType = 0;
    uint32_t planeBytes;
    short cmpCount = 3;
    short cmpSize = 8;
    unsigned screenbitsperpixel;

    switch(Preferences::atariScreenColourMode)
    {
        case atariScreenMode2:
            screenbitsperpixel = 1;     // monochrome
            planeBytes = 0;             // do not force Atari compatibiliy (interleaved plane) mode
            cmpCount = 1;
            cmpSize = 1;
            break;

        case atariScreenMode4ip:
            screenbitsperpixel = 2;     // 4 colours, indirect
            planeBytes = 2;             // change to 0 to force packed pixel instead of interleaved plane
            cmpCount = 1;
            cmpSize = 2;
            break;

        case atariScreenMode16:
            screenbitsperpixel = 4;     // 16 colours, indirect
            planeBytes = 0;             // packed pixel
            cmpCount = 1;
            cmpSize = 4;
            break;

        case atariScreenMode16ip:
            screenbitsperpixel = 4;     // 16 colours, indirect
            planeBytes = 2;             // force interleaved plane
            cmpCount = 1;
            cmpSize = 4;
            break;

        case atariScreenMode256:
            screenbitsperpixel = 8;     // 256 colours, indirect
            planeBytes = 0;             // do not force Atari compatibiliy (interleaved plane) mode
            cmpCount = 1;
            cmpSize = 8;
            break;

        case atariScreenModeHC:
            screenbitsperpixel = 16;    // 32768 colours, direct
            rmask = 0x7C00;
            gmask = 0x03E0;
            bmask = 0x001F;
            amask = 0x8000;
            pixelType = 16;             // RGBDirect, 0 would be indexed
            planeBytes = 0;
            break;

        default:
            screenbitsperpixel = 32;    // 16M colours, direct
            rmask = 0x00ff0000;         // ARGB
            gmask = 0x0000ff00;
            bmask = 0x000000ff;
            amask = 0xff000000;
            pixelType = 16;             // RGBDirect, 0 would be indexed
            planeBytes = 0;
            break;
    }

    // Note that the SDL surface cannot distinguish between packed pixel and interleaved.
    // This is the screen buffer for the emulated Atari

    DebugWarning2("() : Create SDL surface with %u bits per pixel, r=0x%08x, g=0x%08x, b=0x%08x",
                     screenbitsperpixel, rmask, gmask, bmask);
    #if 1
    // screen size is number bits, divided by 8
    pixels_size = (Preferences::AtariScreenWidth * Preferences::AtariScreenHeight * screenbitsperpixel) >> 3;
    // The Atari has 32768 bytes video memory from which 32000 are visible on screen
    if (pixels_size == 32000)
    {
        DebugWarning2("() : Allocate 32768 bytes instead of 32000 for compatibility");
        pixels_size += 768;
    }
    pixels = malloc(pixels_size);
    int pitch = (Preferences::AtariScreenWidth * screenbitsperpixel) >> 3;
    m_sdl_atari_surface = SDL_CreateRGBSurfaceFrom(
            pixels,   // pixel data
            Preferences::AtariScreenWidth,
            Preferences::AtariScreenHeight,
            screenbitsperpixel,     // depending on Atari screen mode
            pitch,
            rmask,
            gmask,
            bmask,
            amask);
    #else
    m_sdl_atari_surface = SDL_CreateRGBSurface(
            0,    // no flags
            Preferences::AtariScreenWidth,
            Preferences::AtariScreenHeight,
            screenbitsperpixel,     // depending on Atari screen mode
            rmask,
            gmask,
            bmask,
            amask);
    #endif
    assert(m_sdl_atari_surface);
    // hack to mark the surface as "interleaved plane"
    if (planeBytes == 2)
    {
        m_sdl_atari_surface->userdata = (void *) 1;
    }
    // we do not deal with the alpha channel, otherwise we always must make sure that each pixel is 0xff******
    SDL_SetSurfaceBlendMode(m_sdl_atari_surface, SDL_BLENDMODE_NONE);

    // In case the Atari does not run in native host graphics mode, we need a conversion surface,
    // and instead of directly updating the texture from the Atari surface, we first convert it to 32 bits per pixel.

    if (screenbitsperpixel != 32)
    {
        rmask = 0x00ff0000;         // ARGB
        gmask = 0x0000ff00;
        bmask = 0x000000ff;
        amask = 0xff000000;
        DebugWarning2("() : Create SDL surface with 32 bits per pixel, r=0x%08x, g=0x%08x, b=0x%08x",
                        screenbitsperpixel, rmask, gmask, bmask);
        m_sdl_host_surface = SDL_CreateRGBSurface(
                                             0,    // no flags
                                             Preferences::AtariScreenWidth,     // was m_hostScreenW, why?
                                             Preferences::AtariScreenHeight,    // dito
                                             32,    // bits per pixel
                                             rmask,
                                             gmask,
                                             bmask,
                                             amask);
        assert(m_sdl_host_surface);
        // we do not deal with the alpha channel, otherwise we always must make sure that each pixel is 0xff******
        SDL_SetSurfaceBlendMode(m_sdl_host_surface, SDL_BLENDMODE_NONE);
    }
    else
    {
        m_sdl_host_surface = m_sdl_atari_surface;
    }
    init_pixmap(pixelType, cmpCount, cmpSize, planeBytes);
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
