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
 *  C interface for C++ class EmulatorRunner.
 * To be called from Objective-C
 *
 */

#include "Debug.h"
#include "EmulationMain.h"
#include "EmulationRunner.h"
extern "C"
//void m68k_init(void);
{
#include "m68k.h"
}

#if 0
extern "C"
{
    // dummy memory access functions
    unsigned int  m68k_read_memory_8(unsigned int address){return 0;}
    unsigned int  m68k_read_memory_16(unsigned int address){return 0;}
    unsigned int  m68k_read_memory_32(unsigned int address){return 0;}
    void m68k_write_memory_8(unsigned int address, unsigned int value){}
    void m68k_write_memory_16(unsigned int address, unsigned int value){}
    void m68k_write_memory_32(unsigned int address, unsigned int value){}
}
#endif

static int s_EmulationIsInit = 0;
static int s_EmulationIsRunning = 0;



int EmulationIsRunning(void)
{
    return s_EmulationIsRunning;
}

int EmulationInit(void)
{
    int ret = 0;

    if (!s_EmulationIsInit)
    {
        m68k_init();
        ret = EmulationRunner::init();
        s_EmulationIsInit = 1;
    }

    return ret;
}

int EmulationOpenWindow()
{
    return EmulationRunner::OpenWindow();
}

void EmulationRun(void)
{
    DebugInfo("%s()", __func__);
    if (s_EmulationIsInit && !s_EmulationIsRunning)
    {
        EmulationRunner::StartEmulatorThread();
        s_EmulationIsRunning = 1;
    }
    DebugInfo("%s() =>", __func__);
}

void EmulationRunSdl(void)
{
    EmulationRunner::EventLoop();
}

void EmulationExit(void)
{
    if (s_EmulationIsRunning)
    {
    }

    if (s_EmulationIsInit)
    {
        EmulationRunner::Cleanup();
        s_EmulationIsRunning = 0;
        s_EmulationIsInit = 0;
    }
}

void EmulationChangeAtariDrive(unsigned drvnr, const char *path)
{
    EmulationRunner::ChangeAtariDrive(drvnr, path);
}



/*
extern "C" int my_main(void)
{
    DebugInfo("%s()", __func__);

    theEmulation.EventLoop();

    DebugInfo("%s() =>", __func__);
    return 0;
}
*/