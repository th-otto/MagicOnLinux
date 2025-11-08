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
#include "Debug.h"
#include "Globals.h"
#include "MagiC.h"
#include "MagiCScreen.h"
#include "XCmd.h"
#include "Clipboard.h"        // MagiC clipboad handling


class EmulationRunner
{
    protected:
        SDL_TimerID m_timer;
        bool m_bQuitLoop;
        unsigned m_200HzCnt;

    public:
        // Constructor and destructor
        EmulationRunner(void);
        ~EmulationRunner(void);

        int StartEmulatorThread(void);
        void EventLoop(void);
        int Init(void);
        int OpenWindow(void);
        void Cleanup(void);
        void ChangeAtariDrive(unsigned drvnr, const char *path);

    private:
        static uint32_t LoopTimer(Uint32 interval, void* param);

        void HandleUserEvents(SDL_Event* event);

        // Game related functions
        void EmulatorWindowUpdate(void);

        void _OpenWindow(void);
        void _StartEmulatorThread(void);

    unsigned m_hostScreenW;
    unsigned m_hostScreenH;
    double m_hostScreenStretchX;
    double m_hostScreenStretchY;

    unsigned screenbitsperpixel;
    char m_window_title[256];
    uint32_t m_counter;
    bool m_visible;

    SDL_Surface *m_sdl_atari_surface;        // surface in Atari native pixel format or NULL
    SDL_Surface *m_sdl_surface;                // surface in host native pixel format
    SDL_Window  *m_sdl_window;
    SDL_Renderer *m_sdl_renderer;
    SDL_Texture *m_sdl_texture;
    CMagiCScreen m_EmulatorScreen;
    CXCmd m_EmulatorXcmd;
    CMagiC m_Emulator;
    SDL_Thread *m_EmulatorThread;
    bool m_EmulatorRunning;

private:
    int EmulatorThread();
    static int _EmulatorThread(void *ptr);
};

const int RUN_EMULATOR_WINDOW_UPDATE = 1;
const int OPEN_EMULATOR_WINDOW =2;
const int RUN_EMULATOR = 3;
