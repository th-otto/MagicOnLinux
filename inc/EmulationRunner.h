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
 *  EmulationRunner.h
 *
 */

#define MGMX 1

// system headers
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
//#include <OpenGL/gl.h>        // needed?
//#include <OpenGL/glu.h>        // needed?
// user headers
#include "MagiC.h"
#include "MagiCScreen.h"
#include "XCmd.h"
#include "network.h"

// static class
class EmulationRunner
{
  public:

    static int StartEmulatorThread(void);
    static void EventLoop(void);
    static int init(void);
    static int OpenWindow(void);
    static void Cleanup(void);
    static void ChangeAtariDrive(unsigned drvnr, const char *path);

  private:

    static uint32_t LoopTimer(Uint32 interval, void* param);
    static void HandleUserEvents(SDL_Event* event);
    static void EmulatorWindowUpdate(void);
    static void _OpenWindow(void);
    static void _StartEmulatorThread(void);
    static int EmulatorThread(void *param);

    static unsigned m_hostScreenW;
    static unsigned m_hostScreenH;
    static double m_hostScreenStretchX;
    static double m_hostScreenStretchY;

    static char m_window_title[256];
    static uint32_t m_counter;
    static bool m_visible;

    static SDL_Window  *m_sdl_window;
    static SDL_Renderer *m_sdl_renderer;
    static SDL_Texture *m_sdl_texture;
    static CXCmd m_EmulatorXcmd;
    static CMagiC m_Emulator;
    static CNetwork m_Network;
    static SDL_Thread *m_EmulatorThread;
    static bool m_EmulatorRunning;

    static SDL_TimerID m_timer;
    static bool m_bQuitLoop;
    static unsigned m_200HzCnt;
};

const int RUN_EMULATOR_WINDOW_UPDATE = 1;
const int OPEN_EMULATOR_WINDOW =2;
const int RUN_EMULATOR = 3;
