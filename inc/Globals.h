/*
 * Copyright (C) 1990-225 Andreas Kromke, andreas.kromke@gmail.com
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
#ifndef be32toh /* sometimes <endian.h> already included by c++ headers */
#if defined(__linux__)
#include <endian.h>
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define be32toh(x) __builtin_bswap32(x)
#define htobe32(x) __builtin_bswap32(x)
#define be16toh(x) __builtin_bswap16(x)
#define htobe16(x) __builtin_bswap16(x)
#define le16toh(x) (x)
#define le32toh(x) (x)
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define be32toh(x) (x)
#define htobe32(x) (x)
#define be16toh(x) (x)
#define htobe16(x) (x)
#define le16toh(x) __builtin_bswap16(x)
#define le32toh(x) __builtin_bswap32(x)
#else
#error "unsupported byte order"
#endif
#endif


#if defined(USE_ASGARD_PPC_68K_EMU)
// Asgard 68k emulator (PPC Assembler)
#include "Asgard68000.h"
#define COUNT_CYCLES 0
#else
extern "C" {
// Musashi 68k emulator ('C')
#include "m68k.h"
} // end extern "C"
#endif


// compile time switches

#ifdef _DEBUG
#define _DEBUG_WRITEPROTECT_ATARI_OS

//#define _DEBUG_NO_ATARI_KB_INTERRUPTS
//#define _DEBUG_NO_ATARI_MOUSE_INTERRUPTS
//#define _DEBUG_NO_ATARI_HZ200_INTERRUPTS
//#define _DEBUG_NO_ATARI_VBL_INTERRUPTS
#endif

// endian conversion helpers

#define getAtariBE16(addr) \
    be16toh(*((uint16_t *) (addr)));
#define getAtariBE32(addr) \
    be32toh(*((uint32_t *) (addr)));
#define setAtariBE16(addr, val) \
    *((uint16_t *) (addr)) = htobe16(val);
#define setAtariBE32(addr, val) \
    *((uint32_t *) (addr)) = htobe32(val);
#define setAtari32(addr, val) \
    *((uint32_t *) (addr)) = val;


typedef struct
{
	uint16_t bottom;
	uint16_t left;
	uint16_t right;
	uint16_t top;
} Rect;

typedef struct
{
	int16_t x;
	int16_t y;
} Point;


//
// global variables and functions
//


// -> MagiC.cpp
void sendBusError(uint32_t addr, const char *AccessMode);
void getActAtariPrg(const char **pName, uint32_t *pact_pd);
void sendVBL(void);

#endif
