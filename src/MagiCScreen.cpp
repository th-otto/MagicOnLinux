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
 * @brief initialisation, create SDL surfaces
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

    // SDL stuff
    Uint32 rmask = 0;
    Uint32 gmask = 0;
    Uint32 bmask = 0;
    Uint32 amask = 0;
    // Pixmap stuff
    uint16_t pixelType = 0;
    uint32_t planeBytes;
    uint16_t cmpCount;
    uint16_t cmpSize;
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
            cmpCount = 3;
            cmpSize = 8;
            break;

        default:
            screenbitsperpixel = 32;    // 16M colours, direct
            rmask = 0x00ff0000;         // ARGB
            gmask = 0x0000ff00;
            bmask = 0x000000ff;
            amask = 0xff000000;
            pixelType = 16;             // RGBDirect, 0 would be indexed
            planeBytes = 0;
            cmpCount = 3;
            cmpSize = 8;
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
    m_PixMap.planeBytes    = planeBytes;                   // 2: interleaved, otherwise 0
    m_PixMap.pmTable       = 0;
    m_PixMap.pmReserved    = 0;
}


/** **********************************************************************************************
 *
 * @brief Derive Atari screen mode
 *
 * @return Atari screen mode 0/1/2/4/6/7 or 3 for "incompatible"
 *
 * @note We do not support TT modes 4, 6 and 7. Note that mode 7 is interleaved, while we use packed.
 *
 ************************************************************************************************/
uint8_t CMagiCScreen::getAtariScreenMode()
{
    if ((m_PixMap.cmpSize == 4) && (m_PixMap.planeBytes != 0))
    {
        return 0;   // ST-low (16 colours interleaved)
    }
    if (m_PixMap.cmpSize == 2)
    {
        return 1;   // ST-mid (four colours interleaved)
    }
    if (m_PixMap.cmpSize == 1)
    {
        return 2;   // ST-high (monochrome)
    }

    return 3;   // incompatible format
}


/** **********************************************************************************************
 *
 * @brief  Convert bitmap format from Atari native to host native
 *
 * @param[in]  pSrc         source surface in Atari format, e.g. monochrome
 * @param[in]  pDst         destination surface, RGB
 * @param[in]  palette      colour palette, 4 or 16 or 256 entries for indirect colour mode
 * @param[in]  bStretchX    horizontal stretching factor ?!?
 * @param[in]  bStretchY    vertical stretching factor ?!?
 *
 * @note This function is NOT called in true colour mode.
 *
 ************************************************************************************************/
void CMagiCScreen::convAtari2HostSurface()
{
    const SDL_Surface *pSrc = m_sdl_atari_surface;
    SDL_Surface *pDst = m_sdl_host_surface;
    const uint32_t *palette = m_pColourTable;

    int x,y;
    const uint8_t *ps8 = (const uint8_t *) pSrc->pixels;
    if (CMagiCScreen::m_physAddr != 0)
    {
        ps8 = mem68k + CMagiCScreen::m_physAddr;
    }
    uint8_t *pd8 = (uint8_t *) pDst->pixels;
    const uint8_t *ps8x;
    uint32_t *pd32x;
    uint8_t c;
    // Atari colour representation:
    // 0xff000000        black
    // 0xffff0000        red
    // 0xff00ff00        green
    // 0xff0000ff        blue
    // convert from RGB555 to RGB888 using a conversion table
    static const uint32_t rgbConvTable5to8[32] =
    {
        0,
        ( 1 * 255)/31,
        ( 2 * 255)/31,
        ( 3 * 255)/31,
        ( 4 * 255)/31,
        ( 5 * 255)/31,
        ( 6 * 255)/31,
        ( 7 * 255)/31,
        ( 8 * 255)/31,
        ( 9 * 255)/31,
        (10 * 255)/31,
        (11 * 255)/31,
        (12 * 255)/31,
        (13 * 255)/31,
        (14 * 255)/31,
        (15 * 255)/31,
        (16 * 255)/31,
        (17 * 255)/31,
        (18 * 255)/31,
        (19 * 255)/31,
        (20 * 255)/31,
        (21 * 255)/31,
        (22 * 255)/31,
        (23 * 255)/31,
        (24 * 255)/31,
        (25 * 255)/31,
        (26 * 255)/31,
        (27 * 255)/31,
        (28 * 255)/31,
        (29 * 255)/31,
        (30 * 255)/31,
        255
    };

    // hack to detect interleaved plane format
    int bitsperpixel = pSrc->format->BitsPerPixel;
    if (pSrc->userdata == (void *) 1)
    {
        bitsperpixel *= 10;
    }

    switch(bitsperpixel)
    {
        //
        // one bit, two colours
        //

        case 1:
        {
            uint32_t col0 = palette[0];
            uint32_t col1 = palette[1];

            // monochrome, driver MFM2.SYS.
            for (y = 0; y < pSrc->h; y++)
            {
                ps8x = ps8;                    // pointer to source line
                pd32x = (uint32_t *) pd8;    // pointer to dest line

                for (x = 0; x < pSrc->w; x += 8)
                {
                    c = *ps8x++;        // get one byte, 8 pixels
                    *pd32x++ = (c & 0x80) ? col1 : col0;
                    *pd32x++ = (c & 0x40) ? col1 : col0;
                    *pd32x++ = (c & 0x20) ? col1 : col0;
                    *pd32x++ = (c & 0x10) ? col1 : col0;
                    *pd32x++ = (c & 0x08) ? col1 : col0;
                    *pd32x++ = (c & 0x04) ? col1 : col0;
                    *pd32x++ = (c & 0x02) ? col1 : col0;
                    *pd32x++ = (c & 0x01) ? col1 : col0;
                }

/* Let SDL do that in EmulatorWindowUpdate()
                if (bStretchY)
                {
                    //copy line
                    memcpy(pd8 + pDst->pitch, pd8, pDst->w * sizeof(uint32_t));
                    pd8 += pDst->pitch;
                }
*/
                // advance to next line
                ps8 += pSrc->pitch;
                pd8 += pDst->pitch;
            }
            break;
        }

        //
        // two bits interleaved, four colours, driver MFM4IP.SYS
        //

        case 20:
            // organized as interleaved plane (16-bit-big-endian, lowest bit first)
            for (y = 0; y < pSrc->h; y++)
            {
                ps8x = ps8;                    // pointer to source line
                pd32x = (uint32_t *) pd8;    // pointer to dest line
                uint16_t index3, index2, index1, index0;

                for (x = 0; x < pSrc->w; x += 16)
                {
                    int i;
                    uint8_t ca[16];

                    index1 = *ps8x++;        // get one byte, bit 0 of first 8 pixels
                    index1 <<= 0;
                    index0 = *ps8x++;        // get one byte, bit 0 of  next 8 pixels
                    index0 <<= 0;
                    index3 = *ps8x++;        // get one byte, bit 1 of first 8 pixels
                    index3 <<= 1;
                    index2 = *ps8x++;        // get one byte, bit 1 of  next 8 pixels
                    index2 <<= 1;
                    for (i = 7; i >= 0; i--)
                    {
                        ca[i] = (index3 & 2) | (index1 & 1);
                        index3 >>= 1;
                        index1 >>= 1;
                    }

                    for (i = 15; i >= 8; i--)
                    {
                        ca[i] = (index2 & 2) | (index0 & 1);
                        index2 >>= 1;
                        index0 >>= 1;
                    }

                    for (i = 0; i < 16; i++)
                    {
                        // indexed colour, we must access the palette table here
                        *pd32x++ = palette[ca[i]];
                    }
                }

                /* Let SDL do that in EmulatorWindowUpdate()
                if (bStretchY)
                {
                    //copy line
                    memcpy(pd8 + pDst->pitch, pd8, pDst->w * sizeof(uint32_t));
                    pd8 += pDst->pitch;
                }
                */
                // advance to next line
                ps8 += pSrc->pitch;
                pd8 += pDst->pitch;
            }
            break;

        //
        // four bits packed, 16 colours, driver MFM16.SYS
        //

        case 4:
            for (y = 0; y < pSrc->h; y++)
            {
                ps8x = ps8;                    // pointer to source line
                pd32x = (uint32_t *) pd8;    // pointer to dest line
                uint8_t index1, index0;

                for (x = 0; x < pSrc->w; x += 2)
                {
                    index1 = *ps8x++;        // get one byte, 2 pixels (aaaabbbb)
                    index0 = index1 >> 4;
                    index1 &= 0x0f;

                    // indexed colour, we must access the palette table here
        //            *pd32x++ = (index0 << 4) | (index0 << 12L) | (index0 << 20L) | (0xff000000);
                    *pd32x++ = palette[index0];
        //            *pd32x++ = (index1 << 4) | (index1 << 12L) | (index1 << 20L) | (0xff000000);
                    *pd32x++ = palette[index1];
                }

                // advance to next line
                ps8 += pSrc->pitch;
                pd8 += pDst->pitch;
            }
            break;

            /* The Atari ST however has interleaved bitplanes: The first word
             of graphics memory describes the first 16 pixels on screen in
             the first bitplane, the second word describes the same 16 pixels
             in the second bitplane and so forth. If the Atari ST displays 16
             colours on screen, meaning 4 bitplanes, you have 8 bytes (4 words)
             of data which describe 16 pixels in all 4 bitplanes. */

        //
        // four bits interleaved, 16 colours, driver MFM16IP.SYS
        //

        case 40:
            // organized as interleaved plane (16-bit-big-endian)
            for (y = 0; y < pSrc->h; y++)
            {
                ps8x = ps8;                    // pointer to source line
                pd32x = (uint32_t *) pd8;    // pointer to dest line
                uint16_t index31, index21, index11, index01;
                uint16_t index32, index22, index12, index02;

                for (x = 0; x < pSrc->w; x += 16)
                {
                    int i;
                    uint8_t ca[16];

                    index01 = *ps8x++;        // get one byte, bit 0 of first 8 pixels
                    index01 <<= 0;
                    index02 = *ps8x++;        // get one byte, bit 0 of next 8 pixels
                    index02 <<= 0;
                    index11 = *ps8x++;        // get one byte, bit 1 of first 8 pixels
                    index11 <<= 1;
                    index12 = *ps8x++;        // get one byte, bit 1 of next 8 pixels
                    index12 <<= 1;
                    index21 = *ps8x++;        // get one byte, bit 2 of first 8 pixels
                    index21 <<= 2;
                    index22 = *ps8x++;        // get one byte, bit 2 of next 8 pixels
                    index22 <<= 2;
                    index31 = *ps8x++;        // get one byte, bit 3 of first 8 pixels
                    index31 <<= 3;
                    index32 = *ps8x++;        // get one byte, bit 3 of next 8 pixels
                    index32 <<= 3;

                    for (i = 7; i >= 0; i--)
                    {
                        ca[i] = (index31 & 8) | (index21 & 4) | (index11 & 2) | (index01 & 1);
                        index31 >>= 1;
                        index21 >>= 1;
                        index11 >>= 1;
                        index01 >>= 1;
                    }


                    for (i = 15; i >= 8; i--)
                    {
                        ca[i] = (index32 & 8) | (index22 & 4) | (index12 & 2) | (index02 & 1);
                        index32 >>= 1;
                        index22 >>= 1;
                        index12 >>= 1;
                        index02 >>= 1;
                    }

                    for (i = 0; i < 16; i++)
                    {
                        // indexed colour, we must access the palette table here
                        *pd32x++ = palette[ca[i]];
                    }
                }

                // advance to next line
                ps8 += pSrc->pitch;
                pd8 += pDst->pitch;
            }
            break;

        //
        // 8 bits packed, 256 colours, driver MFM256.SYS
        //

        case 8:
            for (y = 0; y < pSrc->h; y++)
            {
                ps8x = ps8;                    // pointer to source line
                pd32x = (uint32_t *) pd8;    // pointer to dest line
                uint8_t index0;

                for (x = 0; x < pSrc->w; x++)
                {
                    index0 = *ps8x++;        // get one byte, 1 pixel
                    *pd32x++ = palette[index0];
                }

                // advance to next line
                ps8 += pSrc->pitch;
                pd8 += pDst->pitch;
            }
            break;

        //
        // 16 resp. 15 bits packed, 32768 colours, driver MFM32K.SYS
        //

        case 16:
            for (y = 0; y < pSrc->h; y++)
            {
                ps8x = ps8;                    // pointer to source line
                pd32x = (uint32_t *) pd8;    // pointer to dest line
                uint32_t r,g,b,w;

                for (x = 0; x < pSrc->w; x++)
                {
                    w = *ps8x++;        // get upper byte of pixel
                    w <<= 8;
                    w |= *ps8x++;        // get lower byte of pixel

                    // extract colours
                    r = (w >> 10) & 0x1f;
                    g = (w >>  5) & 0x1f;
                    b = (w >>  0) & 0x1f;
                    // expand from 5 to 8 bit
                    r = rgbConvTable5to8[r];
                    g = rgbConvTable5to8[g];
                    b = rgbConvTable5to8[b];

                    // SDL has ARGB
                    w = (0xff000000) | (r << 16) | (g << 8) | (b);
                    *pd32x++ = w;
                }

                // advance to next line
                ps8 += pSrc->pitch;
                pd8 += pDst->pitch;
            }
            break;
    }
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
    #if defined(STE_COLOUR_PALETTE)
    // shrink to 4 bits, no rounding
    red   >>= 4;
    green >>= 4;
    blue  >>= 4;
    // rearrange for STe format
    red   = ((red   & 1) << 3) | (red   >> 1);
    green = ((green & 1) << 3) | (green >> 1);
    blue  = ((blue  & 1) << 3) | (blue  >> 1);
    #else
    // shrink to 3 bits, no rounding
    red   >>= 5;
    green >>= 5;
    blue  >>= 5;
    #endif

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
    #if defined(STE_COLOUR_PALETTE)
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
    #else
    // get 3-bit colour components
    uint32_t red   = (val & 0x0700) >> 8;
    uint32_t green = (val & 0x0070) >> 4;
    uint32_t blue  = (val & 0x0007) >> 0;
    // expand to 8 bits
    red   <<= 5;
    green <<= 5;
    blue  <<= 5;
    // rounding
    if (red & 0x20)
    {
        red |= 0x1f;
    }
    if (green & 0x20)
    {
        green |= 0x1f;
    }
    if (blue & 0x20)
    {
        blue |= 0x1f;
    }
    #endif

    uint32_t c = (red << 16) | (green << 8) | (blue << 0);
    CMagiCScreen::m_pColourTable[index] = c | (0xff000000);
}
