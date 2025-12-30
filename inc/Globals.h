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
*
* Manages all global variables
*
*/

#ifndef _INCLUDED_GLOBALS_H
#define _INCLUDED_GLOBALS_H

#include <stdio.h>
#include <stdint.h>
#ifdef __APPLE__
#include "macos_endian.h"
#else
#include <endian.h>
#endif


extern "C" {
// Musashi 68k emulator ('C')
#include "m68k.h"
} // end extern "C"


// compile time switches

#ifdef _DEBUG
//#define _DEBUG_NO_ATARI_KB_INTERRUPTS
//#define _DEBUG_NO_ATARI_MOUSE_INTERRUPTS
//#define _DEBUG_NO_ATARI_HZ200_INTERRUPTS
//#define _DEBUG_NO_ATARI_VBL_INTERRUPTS
#endif

// endian conversion helpers

#define getAtariBE16(addr) \
    be16toh(*((uint16_t *) (addr)))
#define getAtariBE32(addr) \
    be32toh(*((uint32_t *) (addr)))
#define setAtariBE16(addr, val) \
    *((uint16_t *) (addr)) = htobe16(val);
#define setAtariBE32(addr, val) \
    *((uint32_t *) (addr)) = htobe32(val);
#define setAtari32(addr, val) \
    *((uint32_t *) (addr)) = val;

//
// global variables and functions
//

// -> MagiC.cpp
void sendBusError(uint32_t addr, const char *AccessMode);
void getActAtariPrg(const char **pName, uint32_t *pact_pd);
void sendVBL(void);

#endif
