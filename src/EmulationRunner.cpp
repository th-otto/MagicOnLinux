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
 *  EmulationRunner.cpp
 *
 */

#include "config.h"

#include <unistd.h>
#include <assert.h>
#include "Debug.h"
#include "Clipboard.h"        // MagiC clipboad handling
#include "gui.h"
#include "EmulationRunner.h"
#include "emulation_globals.h"

#if !defined(_DEBUG_EVENTS)
 #undef DebugInfo
 #define DebugInfo(...)
 #undef DebugInfo2
 #define DebugInfo2(...)
#endif


// global variables from "emulation_globals.h"
uint8_t *mem68k;                    // host pointer to memory block used by emulator
uint32_t mem68kSize;                // complete address range for 68k emulator, but without video memory
uint32_t memVideo68kSize;        // size of emulated video memory
uint8_t *addrOpcodeROM;				// pointer to 68k memory (host address)
uint32_t addr68kVideo;				// start of 68k video memory (68k address)
uint32_t addr68kVideoEnd;			// end of 68k video memory (68k address)
uint32_t addrOsRomStart;			// beginning of write-protected memory area (68k address)
uint32_t addrOsRomEnd;				// end of write-protected memory area (68k address)
bool gbAtariVideoRamHostEndian;		// true: video RAM is stored in host endian-mode
uint8_t *hostVideoAddr;				// start of host video memory (host address)
std::atomic_bool gbAtariVideoBufChanged;



unsigned EmulationRunner::m_hostScreenW;
unsigned EmulationRunner::m_hostScreenH;
double EmulationRunner::m_hostScreenStretchX;
double EmulationRunner::m_hostScreenStretchY;

unsigned EmulationRunner::screenbitsperpixel = 32;
char EmulationRunner::m_window_title[256];
uint32_t EmulationRunner::m_counter;
bool EmulationRunner::m_visible;

SDL_Surface *EmulationRunner::m_sdl_atari_surface;        // surface in Atari native pixel format or NULL
SDL_Surface *EmulationRunner::m_sdl_surface;                // surface in host native pixel format
SDL_Window  *EmulationRunner::m_sdl_window;
SDL_Renderer *EmulationRunner::m_sdl_renderer;
SDL_Texture *EmulationRunner::m_sdl_texture;
CMagiCScreen EmulationRunner::m_EmulatorScreen;
CXCmd EmulationRunner::m_EmulatorXcmd;
CMagiC EmulationRunner::m_Emulator;
SDL_Thread *EmulationRunner::m_EmulatorThread = nullptr;
bool EmulationRunner::m_EmulatorRunning = false;
CNetwork EmulationRunner::m_Network;

SDL_TimerID EmulationRunner::m_timer;
bool EmulationRunner::m_bQuitLoop = false;
unsigned EmulationRunner::m_200HzCnt = 0;


/** **********************************************************************************************
 *
 * @brief Initialisation
 *
 * Does some default initialisations that are independent from the current setting/configuration.
 *
 ************************************************************************************************/
int EmulationRunner::init(void)
{
    DebugInfo("%s()", __func__);
    m_counter = 0;

    // we do not want SDL to catch events like SIGSEGV
    int ret = SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
    if (ret != 0)
    {
        const char *errmsg = SDL_GetError();
        fprintf(stderr, "SDL error \"%s\"\n", errmsg);
    }
    else
    {
        assert(!ret);
        // For whatever reason we need this to make non-US-keys working in X11.
        SDL_StopTextInput();
        if (Preferences::bRelativeMouse)
        {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
    }

    return ret;
}


/** **********************************************************************************************
 *
 * @brief Reconfigure virtual Atari drive during runtime
 *
 * @param[in]  drvnr    Atari drive number 0..25
 * @param[in]  path     host path
 *
 ************************************************************************************************/
void EmulationRunner::ChangeAtariDrive(unsigned drvnr, const char *path)
{
    DebugInfo2("()\n");
    if (drvnr < NDRIVES)
    {
        (void) path;
        #if 0
        Preferences::setDrvPath(drvnr, path);
        if (m_EmulatorRunning)
        {
            m_Emulator.ChangeXFSDrive(drvnr);
        }
        #endif
    }
}


/** **********************************************************************************************
 *
 * @brief Start the 68k emulation thread (asynchronous function)
 *
 * @return status code
 * @retval 0  thread started
 * @retval 1  thread was already started
 *
 * @note Send a RUN_EMULATOR SDL user event, and this event will cause a short-life thread to
 *       be created, and this thread will start the CMagiC emulator thread.
 *
 ************************************************************************************************/
int EmulationRunner::StartEmulatorThread(void)
{
    DebugInfo2("()");
    if (!m_EmulatorThread)
    {
        // Send user event to event loop
        SDL_Event event;

        event.type = SDL_USEREVENT;
        event.user.code = RUN_EMULATOR;
        event.user.data1 = 0;
        event.user.data2 = 0;

        SDL_PushEvent(&event);

        return 0;
    }
    else
    {
        return 1;
    }
}


/** **********************************************************************************************
 *
 * @brief Open the Emulation window (asynchronous function)
 *
 * @return status code
 * @retval 0  window will be opened
 * @retval 1  window is already open
 *
 * @note Send a OPEN_EMULATOR_WINDOW SDL user event, and this event will cause
 *       the emulator window to be opened.
 *
 ************************************************************************************************/
int EmulationRunner::OpenWindow(void)
{
    DebugInfo2("()\n");
    if (!m_sdl_window)
    {
        // Send user event to event loop
        SDL_Event event;

        event.type = SDL_USEREVENT;
        event.user.code = OPEN_EMULATOR_WINDOW;
        event.user.data1 = 0;
        event.user.data2 = 0;

        SDL_PushEvent(&event);

        return 0;
    }
    else
        return 1;
}


/** **********************************************************************************************
 *
 * @brief Helper function to redraw an SDL texture from a rectangle of an SDL surface
 *
 * @param[in]  txtu         SDL texture
 * @param[in]  srf          SDL surface
 * @param[in]  rect         rectangle, might be nullptr to update whole texture
 *
 * @note The drawing process to the screen is done later, by SDL_RenderCopy().
 *
 ************************************************************************************************/
static void UpdateTextureFromRect(SDL_Texture *txtu, const SDL_Surface *srf, const SDL_Rect *rect)
{
    const uint8_t *pixels = (const uint8_t *) srf->pixels;
    if (rect)
    {
        pixels += rect->y * srf->pitch;
        pixels += rect->x * srf->format->BytesPerPixel;
    }
    int r = SDL_UpdateTexture(txtu, rect, pixels, srf->pitch);
    if (r == -1)
    {
        DebugError2("() - SDL error %s", SDL_GetError());
    }
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
static void ConvertAtari2HostSurface
(
    const SDL_Surface *pSrc,
    SDL_Surface *pDst,
    const uint32_t palette[256],
    bool bStretchX, bool bStretchY
)
{
    (void) bStretchX;
    (void) bStretchY;

    int x,y;
    const uint8_t *ps8 = (const uint8_t *) pSrc->pixels;
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
 * @brief Create 200 Hz SDL timer and start the 68k emulation thread
 *
 * @note The 68k emulator thread will be started indirectly, via a short-life helper thread.
 *
 ************************************************************************************************/
void EmulationRunner::_StartEmulatorThread(void)
{
    m_timer = SDL_AddTimer(5 /* 5 milliseconds, 200 Hz */, LoopTimer, nullptr);
    // create a short-life helper thread that will later start the CMagiC
    // thread. TODO: Why?
    m_EmulatorThread = SDL_CreateThread(EmulatorThread, "EmulatorThread", nullptr);
}


/** **********************************************************************************************
 *
 * @brief Create all necessary surfaces and textures and open the emulation window
 *
 * @note See VDI source file "SETUP.C" for actual usage of the structure members, e.g.
 *       planeBytes == 2 forces Atari compatibility format, i.e. interleaved plane.
 *       The corresponding drivers are called "MFMxxIP.SYS". Otherwise planeBytes is not
 *       used by VDI and may have any value.
 * @note The guest drivers in particular (halfword = 16 bits):
 *
 *     MFM16M.SYS   24 bit true colour, 32 bits per pixel, direct colour
 *     MFM256.SYS   256 colours, indexed, 8 bits per pixel
 *     MFM16.SYS    16 colours, indexed, 4 bits per pixel (packed, i.e. 2 pixel per byte)
 *     MFM16IP.SYS  16 colours, indexed, 4 bits per pixel (interleaved plane, i.e. 16 pixels per 4 halfwords)
 *     MFM4.SYS     This driver is addressed, but does not exist, unfortunately.
 *     MFM4IP.SYS   4 colours, indexed, 2 bits per pixel (interleaved plane, i.e. 16 pixels per 2 halfwords)
 *     MFM2.SYS     2 colours, indexed, 1 bit per pixel (interleaved plane and packed pixel is here the same)
 *
 ************************************************************************************************/
void EmulationRunner::_OpenWindow(void)
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
            rmask = 0x00ff0000;         // ARGB
            gmask = 0x0000ff00;
            bmask = 0x000000ff;
            amask = 0xff000000;
            pixelType = 16;             // RGBDirect, 0 would be indexed
            planeBytes = 0;
            break;
    }

    sprintf(m_window_title, PROGRAM_NAME " (%ux%ux%u%s)",
                            Preferences::AtariScreenWidth, Preferences::AtariScreenHeight,
                            screenbitsperpixel, (planeBytes == 2) ? "ip" : "");
    m_visible = false;


    // get initial screen size
    m_hostScreenW = Preferences::AtariScreenWidth  * Preferences::AtariScreenStretchX;
    m_hostScreenH = Preferences::AtariScreenHeight * Preferences::AtariScreenStretchY;
    m_hostScreenStretchX = Preferences::AtariScreenStretchX;
    m_hostScreenStretchY = Preferences::AtariScreenStretchY;

    // Note that the SDL surface cannot distinguish between packed pixel and interleaved.
    // This is the screen buffer for the emulated Atari

    DebugWarning2("() : Create SDL surface with %u bits per pixel, r=0x%08x, g=0x%08x, b=0x%08x",
                     screenbitsperpixel, rmask, gmask, bmask);
    m_sdl_atari_surface = SDL_CreateRGBSurface(
            0,    // no flags
            Preferences::AtariScreenWidth,
            Preferences::AtariScreenHeight,
            screenbitsperpixel,     // depending on Atari screen mode
            rmask,
            gmask,
            bmask,
            amask);
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
        m_sdl_surface = SDL_CreateRGBSurface(
                                             0,    // no flags
                                             Preferences::AtariScreenWidth,     // was m_hostScreenW, why?
                                             Preferences::AtariScreenHeight,    // dito
                                             32,    // bits per pixel
                                             rmask,
                                             gmask,
                                             bmask,
                                             amask);
        assert(m_sdl_surface);
        // we do not deal with the alpha channel, otherwise we always must make sure that each pixel is 0xff******
        SDL_SetSurfaceBlendMode(m_sdl_surface, SDL_BLENDMODE_NONE);
    }
    else
    {
        m_sdl_surface = m_sdl_atari_surface;
    }

    int pos_x = Preferences::AtariScreenX;
    int pos_y = Preferences::AtariScreenY;
    if ((pos_x < 0) || (pos_y < 0))
    {
        pos_x = SDL_WINDOWPOS_UNDEFINED;
        pos_y = SDL_WINDOWPOS_UNDEFINED;
    }

    m_sdl_window = SDL_CreateWindow(
                                    m_window_title,
                                    pos_x,
                                    pos_y,
                                    m_hostScreenW,
                                    m_hostScreenH,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    assert(m_sdl_window);

    m_sdl_renderer = SDL_CreateRenderer(m_sdl_window, -1, SDL_RENDERER_ACCELERATED);
    assert(m_sdl_renderer);
    if (m_sdl_renderer == nullptr)
    {
        DebugError2("() : SDL error %s", SDL_GetError());
        return;     // fatal
    }

    // initially fill whole window with white colour
    (void) SDL_FillRect(m_sdl_surface, NULL, 0x00ffffff);
    m_sdl_texture = SDL_CreateTextureFromSurface(m_sdl_renderer, m_sdl_surface);    // seems not to work with non-native format?!?
    assert(m_sdl_texture);
    if (m_sdl_texture == nullptr)
    {
        DebugError2("() : SDL error %s", SDL_GetError());
        return;     // fatal
    }

#if 0
    /*
    * Draw test:  draws a grey square, only visible in native mode (true colour)
    */

    SDL_Rect r = { 1, 1, 256, 256 };
    //SDL_Rect r = { 0, 0, screenw, screenh };
    // 0xff000000        black
    // 0xffff0000        red
    // 0xff00ff00        green
    // 0xff0000ff        blue
    ret = SDL_FillRect(m_sdl_surface, &r, 0x88888888);
    if (ret == -1)
    {
        DebugError("%s() : SDL %s", __func__, SDL_GetError());
        //exit(-1);
    }
    UpdateTextureFromRect(m_sdl_texture, m_sdl_surface, &r);
#endif

    /*
     * Stuff needed for the MagiC graphics kernel
     */

    // create ancient style Pixmap structure to be passed to the Atari kernel, from m_sdl_surface
    assert(m_sdl_atari_surface->pitch < 0x4000);            // Pixmap limit and thus limit for Atari
    assert((m_sdl_atari_surface->pitch & 3) == 0);          // pitch (alias rowBytes) must be dividable by 4

    MXVDI_PIXMAP *pixmap = &m_EmulatorScreen.m_PixMap;

    // We use a global pointer directly used by the heart of the
    // 68k emulator ("mem_access_68k.cpp"). Whenever the emulator detects an access
    // to the guest's video memory, the real access, read or write, will be done
    // via this pointer.
    hostVideoAddr = (uint8_t *) m_sdl_atari_surface->pixels;

    // The baseAddr is supposed to be m_sdl_atari_surface->pixels, but this does no longer work
    // with 64-bit host computer. However, the baseAddr will be changed later anyway
    pixmap->baseAddr      = 0x8000000;                    // dummy target address, later filled in by emulator
    pixmap->rowBytes      = m_sdl_atari_surface->pitch | 0x8000;    // 0x4000 and 0x8000 are flags
    pixmap->bounds_top    = 0;
    pixmap->bounds_left   = 0;
    pixmap->bounds_bottom = m_sdl_atari_surface->h;
    pixmap->bounds_right  = m_sdl_atari_surface->w;
    pixmap->pmVersion     = 4;                            // should mean: pixmap base address is 32-bit address
    pixmap->packType      = 0;                            // unpacked?
    pixmap->packSize      = 0;                            // unimportant?
    pixmap->pixelType     = pixelType;                    // 16 is RGBDirect, 0 would be indexed
    pixmap->pixelSize     = m_sdl_atari_surface->format->BitsPerPixel;
    pixmap->cmpCount      = cmpCount;                     // components: 3 = red, green, blue, 1 = monochrome
    pixmap->cmpSize       = cmpSize;                      // True colour: 8 bits per component
    pixmap->planeBytes    = planeBytes;                   // offset to next plane
    pixmap->pmTable       = 0;
    pixmap->pmReserved    = 0;
}


/** **********************************************************************************************
 *
 * @brief Timer function for simulated Atari and for screen update to host
 *
 * @param[in]  interval     timer interval in milliseconds, here 5 for 200 Hz
 * @param[in]  param        user data, here points to EmulationRunner object
 *
 * @return the next interval
 *
 * @note Triggers 200 Hz timer for simulated Atari
 * @note Triggers VBL timer for simulated Atari with 50 Hz
 * @note Triggers host screen update with 25 Hz, if <gbAtariVideoBufChanged>  is set
 *
 ************************************************************************************************/
uint32_t EmulationRunner::LoopTimer(Uint32 interval, void *param)
{
    // too often DebugInfo("%s()", __func__);
    EmulationRunner *p = (EmulationRunner *) param;

    if (p->m_EmulatorRunning)
    {
        p->m_Emulator.SendHz200();
        p->m_200HzCnt++;
        if ((p->m_200HzCnt % 4) == 0)
        {
            // VBL interrupt runs with 50 Hz
            p->m_Emulator.SendVBL();
        }

        if (((p->m_200HzCnt % 8) == 0) && (gbAtariVideoBufChanged))
        {
            // screen update runs with 25 Hz

            // Create a user event to call the application loop.
            SDL_Event event;

            event.type = SDL_USEREVENT;
            event.user.code = RUN_EMULATOR_WINDOW_UPDATE;
            event.user.data1 = 0;
            event.user.data2 = 0;

            SDL_PushEvent(&event);
        }
    }

    if ((p->m_Emulator.m_bEmulatorHasEnded) && !p->m_bQuitLoop)
    {
        (void) showAlert("The virtual machine has ended", "The application window will be closed");
        p->m_bQuitLoop = true;
    }

    return interval;
}


/** **********************************************************************************************
 *
 * @brief Cleanup, removes SDL timer and quits SDL
 *
 ************************************************************************************************/
void EmulationRunner::Cleanup(void)
{
    (void) SDL_RemoveTimer(m_timer);
    SDL_Quit();
}


/** **********************************************************************************************
 *
 * @brief Debug helper to convert SDL event id to text
 *
 * @param[in]  id   SDL event id
 *
 * @return SDL event as text
 *
 ************************************************************************************************/
const char *SDL_WindowEventID_to_str(SDL_WindowEventID id)
{
    switch(id)
    {
        case SDL_WINDOWEVENT_NONE:            return "NONE";
        case SDL_WINDOWEVENT_SHOWN:           return "SHOWN";
        case SDL_WINDOWEVENT_HIDDEN:          return "HIDDEN";
        case SDL_WINDOWEVENT_EXPOSED:         return "EXPOSED";
        case SDL_WINDOWEVENT_MOVED:           return "MOVED";
        case SDL_WINDOWEVENT_RESIZED:         return "RESIZED";
        case SDL_WINDOWEVENT_SIZE_CHANGED:    return "SIZE_CHANGED";
        case SDL_WINDOWEVENT_MINIMIZED:       return "MINIMIZED";
        case SDL_WINDOWEVENT_MAXIMIZED:       return "MAXIMIZED";
        case SDL_WINDOWEVENT_RESTORED:        return "RESTORED";
        case SDL_WINDOWEVENT_ENTER:           return "ENTER";
        case SDL_WINDOWEVENT_LEAVE:           return "LEAVE";
        case SDL_WINDOWEVENT_FOCUS_GAINED:    return "FOCUS_GAINED";
        case SDL_WINDOWEVENT_FOCUS_LOST:      return "FOCUS_LOST";
        case SDL_WINDOWEVENT_CLOSE:           return "CLOSE";
        case SDL_WINDOWEVENT_TAKE_FOCUS:      return "TAKE_FOCUS";
        case SDL_WINDOWEVENT_HIT_TEST:        return "HIT_TEST";
        case SDL_WINDOWEVENT_ICCPROF_CHANGED: return "ICCPROF_CHANGED";
        case SDL_WINDOWEVENT_DISPLAY_CHANGED: return "DISPLAY_CHANGED";
    }

    return "UNKNOWN";
}


/** **********************************************************************************************
 *
 * @brief SDL event loop
 *
 * @note Must most probably be run in main thread (GUI thread)?!?
 *
 ************************************************************************************************/
void EmulationRunner::EventLoop(void)
{
    uint8_t *clipboardData;

    DebugInfo2("()");
    SDL_Event event;

    while((!m_bQuitLoop) && (SDL_WaitEvent(&event)))
    {

#ifndef NDEBUG
        while(do_not_interrupt_68k)
        {
            sleep(1);   // do not disturb the 68k debugging
        }
#endif

        switch(event.type)
        {
            case SDL_WINDOWEVENT:
                {
                    const SDL_WindowEvent *ev = (SDL_WindowEvent *) &event;
                    DebugInfo2("() - SDL window event: evt=%u, wid=%u, ev=%s, data1=0x%08x, data2=0x%08x",
                            ev->type,
                            ev->windowID,
                            SDL_WindowEventID_to_str((SDL_WindowEventID) ev->event),
                            ev->data1,
                            ev->data2);
                    switch(ev->event)
                    {
                        case SDL_WINDOWEVENT_SHOWN:
                            m_visible = true;
                            break;

                        case SDL_WINDOWEVENT_FOCUS_GAINED:
                        {
                            // Synchronise modifier keys
                            // Note that Cmd key KMOD_LGUI/RGUI is not passed to the emulated system.
                            SDL_Keymod mod_state = SDL_GetModState();
                            uint32_t button_state = SDL_GetMouseState(nullptr, nullptr);
                            uint8_t atari_kbshift = 0;
                            if (mod_state & KMOD_LSHIFT) atari_kbshift |= KBSHIFT_SHIFT_LEFT;
                            if (mod_state & KMOD_RSHIFT) atari_kbshift |= KBSHIFT_SHIFT_RIGHT;
                            if (mod_state & KMOD_LCTRL) atari_kbshift |= KBSHIFT_CTRL;
                            if (mod_state & KMOD_RCTRL) atari_kbshift |= KBSHIFT_CTRL;
                            if (mod_state & KMOD_LALT) atari_kbshift |= KBSHIFT_ALT;
                            if (mod_state & KMOD_RALT) atari_kbshift |= KBSHIFT_ALT;
                            if (mod_state & KMOD_CAPS) atari_kbshift |= KBSHIFT_CAPS_LOCK;
                            if (mod_state & KMOD_MODE) atari_kbshift |= KBSHIFT_ALTGR;
                            if (button_state & SDL_BUTTON_LMASK)  atari_kbshift |= KBSHIFT_MBUTTON_LEFT;
                            if (button_state & SDL_BUTTON_RMASK)  atari_kbshift |= KBSHIFT_MBUTTON_RIGHT;
                            m_Emulator.sendKbshift(atari_kbshift);

                            // Copy host Clipboard to Atari Clipboard
                            if (SDL_HasClipboardText())
                            {
                                // get clipboard text from SDL, hopefully in UTF-8
                                clipboardData = (uint8_t *) SDL_GetClipboardText();
                                if (clipboardData)
                                {
                                    CClipboard::host2Atari(clipboardData);
                                    SDL_free(clipboardData);
                                }
                            }

                            // hide mouse cursor in emulation window, if configured
                            if (Preferences::bHideHostMouse)
                            {
                                SDL_ShowCursor(SDL_DISABLE);
                            }
                            break;
                        }

                        case SDL_WINDOWEVENT_FOCUS_LOST:
                            // Copy Atari Clipboard to host Clipboard
                            clipboardData = nullptr;
                            CClipboard::Atari2host(&clipboardData);
                            if (clipboardData)
                            {
                                SDL_SetClipboardText((const char *) clipboardData);
                                free(clipboardData);
                            }

                            // un-hide mouse cursor outside of emulation window, if configured
                            if (Preferences::bHideHostMouse)
                            {
                                SDL_ShowCursor(SDL_ENABLE);
                            }
                            break;

                        case SDL_WINDOWEVENT_RESIZED:
                            m_hostScreenW = ev->data1;
                            m_hostScreenH = ev->data2;
                            m_hostScreenStretchX = (double) m_hostScreenW / Preferences::AtariScreenWidth;
                            m_hostScreenStretchY = (double) m_hostScreenH / Preferences::AtariScreenHeight;

                            //SDL_SetWindowSize(m_sdl_window, 1000, 800);
                            gbAtariVideoBufChanged = true;
                            break;
                    }
                }
                break;

            case SDL_USEREVENT:
                HandleUserEvents(&event);
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                {
                    const SDL_KeyboardEvent *ev = &event.key;
                    DebugInfo2("() - type %s", ev->type == SDL_KEYUP ? "up" : "down");
                    DebugInfo2("() - state %s", ev->state == SDL_PRESSED ? "pressed" : "released");
                    DebugInfo2("() - scancode = %08x (%d), keycode = %08x, mod = %04x", ev->keysym.scancode, ev->keysym.scancode, ev->keysym.sym, ev->keysym.mod);

                    (void) m_Emulator.SendSdlKeyboard(ev->keysym.scancode, ev->type == SDL_KEYUP);
                }
                // Quit when user presses a key.
                //m_bQuitLoop = true;
                //showPreferencesWindow();
                break;

            case SDL_MOUSEMOTION:
                {
                    const SDL_MouseMotionEvent *ev = &event.motion;
                    DebugWarning2("() - mouse motion x = %d, y = %d, xrel = %d, yrel = %d", ev->x, ev->y, ev->xrel, ev->yrel);

                    if (Preferences::bRelativeMouse)
                    {
                        double xrel = (double) (ev->xrel) / m_hostScreenStretchX;
                        double yrel = (double) (ev->yrel) / m_hostScreenStretchY;
                        m_Emulator.SendMouseMovement(xrel, yrel);
                    }
                    else
                    {
                        double xd = (double) (ev->x) / m_hostScreenStretchX;
                        double yd = (double) (ev->y) / m_hostScreenStretchY;
                        int x = (int) (xd + 0.5);   // rounding
                        int y = (int) (yd + 0.5);
                        m_Emulator.SendMousePosition(x, y);
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                {
                    const SDL_MouseButtonEvent *ev = &event.button;
                    DebugInfo2("() - mouse button %s: x = %d, y = %d, button = %d",
                            ev->type == SDL_MOUSEBUTTONUP ? "up" : "down", ev->x, ev->y, ev->button);
                    int atariMouseButton = -1;
                    if (ev->button == 1)
                        atariMouseButton = 0;
                    else
                    if (ev->button == 3)
                        atariMouseButton = 1;
                    if (atariMouseButton >= 0)
                    {
                        // left button is 0, right button is 1
                        m_Emulator.SendMouseButton(atariMouseButton, ev->type == SDL_MOUSEBUTTONDOWN);
                    }
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    const SDL_MouseWheelEvent *ev = &event.wheel;
                    (void) ev;
                    DebugInfo2("() - mouse wheel: x = %d, y = %d", ev->x, ev->y);
                }
                break;

            case SDL_TEXTEDITING:
                break;

            case SDL_TEXTINPUT:
                break;

            case SDL_AUDIODEVICEADDED:
                break;

            case SDL_QUIT:
                m_Emulator.TerminateThread();
                m_bQuitLoop = true;
                break;

            case SDL_CLIPBOARDUPDATE:
                #if !defined(NDEBUG)
                static bool SDL_CLIPBOARDUPDATE_warned = false;
                if (!SDL_CLIPBOARDUPDATE_warned)
                {
                    DebugWarning2("() - SDL_CLIPBOARDUPDATE currently unhandled");
                    SDL_CLIPBOARDUPDATE_warned = true;      // warn only ONCE!
                }
                #endif
                // The clipboard or primary selection changed
                // TODO: support
                break;

            case SDL_DROPFILE:
            {
                SDL_DropEvent *ev = &event.drop;
                if ((ev->file != nullptr) && !m_Emulator.sendDragAndDropFile(ev->file))
                {
                    DebugWarning2("() - SDL_DROPFILE \"%s\" unhandled (warn ONCE)", ev->file);
                    free(ev->file);
                    ev->file = nullptr;
                }
                break;
            }

            case SDL_DROPTEXT:
            {
                SDL_DropEvent *ev = &event.drop;
                DebugWarning2("() - SDL_DROPTEXT \"%s\" currently unhandled", (ev->file != nullptr) ? ev->file : "null");
                (void) ev;
                break;
            }

            case SDL_DROPBEGIN:
            {
                SDL_DropEvent *ev = &event.drop;
                DebugWarning2("() - SDL_DROPBEGIN \"%s\" currently unhandled", (ev->file != nullptr) ? ev->file : "null");
                (void) ev;
                break;
            }

            case SDL_DROPCOMPLETE:
            {
                SDL_DropEvent *ev = &event.drop;
                DebugWarning2("() - SDL_DROPCOMPLETE \"%s\" currently unhandled", (ev->file != nullptr) ? ev->file : "null");
                (void) ev;
                break;
            }

            default:
                DebugWarning2("() - unhandled SDL event %u", event.type);
                break;
        }   // end switch

    }   // end while

    DebugInfo2("() =>");
}


/** **********************************************************************************************
 *
 * @brief SDL event loop: User events
 *
 * @param[in] event     SDL event

 * @note Only called in EventLoop()
 *
 ************************************************************************************************/
void EmulationRunner::HandleUserEvents(SDL_Event* event)
{
    switch (event->user.code)
    {
        case OPEN_EMULATOR_WINDOW:
            if (m_sdl_window == nullptr)
            {
                _OpenWindow();
            }
            break;

        case RUN_EMULATOR:
            if (m_EmulatorThread == nullptr)
            {
                _StartEmulatorThread();
            }
            break;

        case RUN_EMULATOR_WINDOW_UPDATE:
            if (m_sdl_window != nullptr)
            {
                EmulatorWindowUpdate();
            }
            break;

        default:
            break;
    }
}


/** **********************************************************************************************
 *
 * @brief SDL event loop: Special user event to update the emulation window
 *
 * @note Only called from HandleUserEvents()
 * @note If the Atari graphics format differs from host format, i.e. is not 32-bit RGB,
 *       we have m_sdl_atari_surface. The emulated Atari will then change this surface
 *       directly, via writing data to surface->pixels,
 *       and we convert m_sdl_atari_surface to m_sdl_surface. The m_sdl_surface is then
 *       drawn to texture and this one will be drawn to screen.
 *       If the Atari graphics format is the same as host, the emulated Atari directly
 *       writes to m_sdl_surface.
 *
 ************************************************************************************************/
void EmulationRunner::EmulatorWindowUpdate(void)
{
    // too often DebugInfo2("()");

    // also does stretching, if necessary:
    SDL_Rect rc = { 0, 0, (int) m_hostScreenW, (int) m_hostScreenH };        // dst
    SDL_Rect rc2 = { 0, 0, (int) Preferences::AtariScreenWidth, (int) Preferences::AtariScreenHeight };    // src

    if (atomic_exchange(&gbAtariVideoBufChanged, false))
    {
        // too often DebugInfo2("() - Atari Screen dirty");
        if (m_sdl_atari_surface != m_sdl_surface)
        {
            // convert Atari graphics format to host graphics format RGB
            ConvertAtari2HostSurface(m_sdl_atari_surface, m_sdl_surface,
                                    m_EmulatorScreen.m_pColourTable,
                                    /* TODO:ignored */ Preferences::AtariScreenStretchX,
                                    /* TODO: igored */ Preferences::AtariScreenStretchY);
        }

        UpdateTextureFromRect(m_sdl_texture, m_sdl_surface, nullptr);

        if (m_visible)
        {
            (void) SDL_RenderCopy(m_sdl_renderer, m_sdl_texture, &rc2, &rc);
            SDL_RenderPresent(m_sdl_renderer);
        }
    }
}


/** **********************************************************************************************
 *
 * @brief Short-life thread to create and start emulator thread in CMagiC object
 *
 * @return The return value is always zero
 *
 * @note The SDL user event RUN_EMULATOR causes a short-life thread to be started. This
 *       EmulatorThread() initialises the CMagiC object and starts the CMagiC thread.
 *       Afterwards the EmulatorThread() automatically ends.
 *
 ************************************************************************************************/
int EmulationRunner::EmulatorThread(void *param)
{
    (void) param;
    DebugInfo2("()");
    int err;

    err = m_Emulator.Init(&m_EmulatorScreen, &m_EmulatorXcmd);
    if (err)
    {
        DebugError2("() => %d", err);
        m_bQuitLoop = true;     // leave main loop
        return 0;
    }

    err = m_Emulator.CreateThread();
    if (err)
    {
        DebugError2("() - m_Emulator.CreateThread() => %d", err);
        m_bQuitLoop = true;     // leave main loop
        return 0;
    }
    m_Network.init();

    m_EmulatorRunning = true;

    m_Emulator.StartExec();

    DebugInfo2("() =>");
    return 0;
}
