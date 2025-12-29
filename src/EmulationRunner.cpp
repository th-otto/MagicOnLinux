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

char EmulationRunner::m_window_title[256];
uint32_t EmulationRunner::m_counter;
bool EmulationRunner::m_visible;

SDL_Window  *EmulationRunner::m_sdl_window;
SDL_Renderer *EmulationRunner::m_sdl_renderer;
SDL_Texture *EmulationRunner::m_sdl_texture;
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
    sprintf(m_window_title, PROGRAM_NAME " (%ux%ux%s)",
                            Preferences::AtariScreenWidth, Preferences::AtariScreenHeight,
                            Preferences::videoModeToShortString(Preferences::atariScreenColourMode));
    m_visible = false;

    // get initial screen size
    m_hostScreenW = Preferences::AtariScreenWidth  * Preferences::AtariScreenStretchX;
    m_hostScreenH = Preferences::AtariScreenHeight * Preferences::AtariScreenStretchY;
    m_hostScreenStretchX = Preferences::AtariScreenStretchX;
    m_hostScreenStretchY = Preferences::AtariScreenStretchY;

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
    (void) SDL_FillRect(CMagiCScreen::m_sdl_host_surface, NULL, 0x00ffffff);
    m_sdl_texture = SDL_CreateTextureFromSurface(m_sdl_renderer, CMagiCScreen::m_sdl_host_surface);    // seems not to work with non-native format?!?
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

    // We use a global pointer directly used by the heart of the
    // 68k emulator ("mem_access_68k.cpp"). Whenever the emulator detects an access
    // to the guest's video memory, the real access, read or write, will be done
    // via this pointer.
    hostVideoAddr = (uint8_t *) CMagiCScreen::m_sdl_atari_surface->pixels;
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
        p->m_Emulator.sendHz200();
        p->m_200HzCnt++;
        if ((p->m_200HzCnt % 4) == 0)
        {
            // VBL interrupt runs with 50 Hz
            p->m_Emulator.sendVBL();
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
 * @brief Special handling for some keys
 *
 * @param[in,out] event         The event can be changed or left unchanged
 *
 * @return false, if event has been handled, otherwise true
 *
 ************************************************************************************************/
bool EmulationRunner::convertKeyEvent(SDL_KeyboardEvent *ev)
{
    static const uint8_t kbshift_sh_ctrl_alt_mask = (KBSHIFT_SHIFT_RIGHT + KBSHIFT_SHIFT_LEFT + KBSHIFT_CTRL + KBSHIFT_ALT + KBSHIFT_ALTGR);
    uint8_t kbshift_masked = m_Emulator.getKbshift() & kbshift_sh_ctrl_alt_mask;
    if ((ev->keysym.scancode == SDL_SCANCODE_GRAVE) &&  (kbshift_masked == KBSHIFT_CTRL + KBSHIFT_ALT))
    {
        ev->keysym.scancode = SDL_SCANCODE_ESCAPE;
        return true;   // continue processing with the changed scancode
    }

    //
    // Alt-Cursor for mouse move emulation in absolute mode
    // Note that the key-release event is ignored here.
    // Also note that there will be no Atari key click sound, because
    // the event is handled in the emulator only.
    //

    if ((kbshift_masked & KBSHIFT_ALT) && !Preferences::bRelativeMouse &&
        ((ev->keysym.scancode == SDL_SCANCODE_LEFT) || (ev->keysym.scancode == SDL_SCANCODE_RIGHT) ||
         (ev->keysym.scancode == SDL_SCANCODE_UP) || (ev->keysym.scancode == SDL_SCANCODE_DOWN)))
    {
        if (ev->type == SDL_KEYDOWN)
        {
            int x, y;
            bool isShift = kbshift_masked & (KBSHIFT_SHIFT_RIGHT + KBSHIFT_SHIFT_LEFT);
            int factor = (isShift) ? 1 : 8;

            if ((ev->keysym.scancode == SDL_SCANCODE_LEFT) || (ev->keysym.scancode == SDL_SCANCODE_UP))
            {
                factor = -factor;
            }

            (void) SDL_GetGlobalMouseState(&x, &y);
            if ((ev->keysym.scancode == SDL_SCANCODE_LEFT) || (ev->keysym.scancode == SDL_SCANCODE_RIGHT))
            {
                x += factor * Preferences::AtariScreenStretchX;
            }
            else
            {
                y += factor * Preferences::AtariScreenStretchY;
            }

            SDL_WarpMouseGlobal(x, y);
        }
        return false;   // no further handling
    }

    return true;   // continue processing with the unchanged scancode
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
                    SDL_KeyboardEvent *ev = &event.key;
                    DebugInfo2("() - type %s", ev->type == SDL_KEYUP ? "up" : "down");
                    DebugInfo2("() - state %s", ev->state == SDL_PRESSED ? "pressed" : "released");
                    DebugInfo2("() - scancode = %08x (%d), keycode = %08x, mod = %04x", ev->keysym.scancode, ev->keysym.scancode, ev->keysym.sym, ev->keysym.mod);

                    if (convertKeyEvent(ev))
                    {
                        (void) m_Emulator.sendSdlKeyboard(ev->keysym.scancode, ev->type == SDL_KEYUP);
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                {
                    const SDL_MouseMotionEvent *ev = &event.motion;
                    //DebugWarning2("() - mouse motion x = %d, y = %d, xrel = %d, yrel = %d", ev->x, ev->y, ev->xrel, ev->yrel);

                    if (Preferences::bRelativeMouse)
                    {
                        double xrel = (double) (ev->xrel) / m_hostScreenStretchX;
                        double yrel = (double) (ev->yrel) / m_hostScreenStretchY;
                        m_Emulator.sendMouseMovement(xrel, yrel);
                    }
                    else
                    {
                        double xd = (double) (ev->x) / m_hostScreenStretchX;
                        double yd = (double) (ev->y) / m_hostScreenStretchY;
                        int x = (int) (xd + 0.5);   // rounding
                        int y = (int) (yd + 0.5);
                        m_Emulator.sendMousePosition(x, y);
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
                        m_Emulator.sendMouseButton(atariMouseButton, ev->type == SDL_MOUSEBUTTONDOWN);
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
                m_Emulator.sendShutdown();      // -> MMXDAEMON
                m_Emulator.terminateThread();
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
        if (CMagiCScreen::m_sdl_atari_surface != CMagiCScreen::m_sdl_host_surface)
        {
            // convert Atari graphics format to host graphics format RGB
            CMagiCScreen::convAtari2HostSurface();
        }

        UpdateTextureFromRect(m_sdl_texture, CMagiCScreen::m_sdl_host_surface, nullptr);

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

    err = m_Emulator.init(&m_EmulatorXcmd);
    if (err)
    {
        DebugError2("() => %d", err);

        switch(err)
        {
            case -1:
                (void) showAlert("The emulator cannot find the kernel file MAGICLIN.OS", "Review configuration file!");
                break;
            case -2:
                (void) showAlert("The kernel file MAGICLIN.OS is invalid or corrupted", "Review configuration file!");
                break;
            case -3:
                (void) showAlert("The kernel file MAGICLIN.OS has a wrong kernel API version", "Review configuration file!");
                break;
            case -4:
                (void) showAlert("The emulator cannot reserve enough memory", "Reduce Atari memory size in configuration file");
                break;
        }

        m_bQuitLoop = true;     // leave main loop
        return 0;
    }

    err = m_Emulator.createThread();
    if (err)
    {
        DebugError2("() - m_Emulator.CreateThread() => %d", err);
        m_bQuitLoop = true;     // leave main loop
        return 0;
    }
    m_Network.init();

    m_EmulatorRunning = true;

    m_Emulator.startExec();

    DebugInfo2("() =>");
    return 0;
}
