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


#ifndef _CONFIG_H_
#define _CONFIG_H_

#define PROGRAM_NAME "MagicOnLinux v0.3"
#define PROGRAM_VERSION_MAJOR 0
#define PROGRAM_VERSION_MINOR 3

#define EVENT_MOUSE 1

#if defined(USE_MUSASHI_68K_EMU)
// #define DEBUG_68K_EMU 1
#endif

// debug output for debug configuration
#if !defined(NDEBUG)
//#define _DEBUG_BASEPAGE
//#define _DEBUG_KBD_AND_MOUSE
//#define _DEBUG_KB_CRITICAL_REGION
//#define _DEBUG_EVENTS
//#define _DEBUG_XFS
#define _DEBUG_VOLUME_IMAGES
//#define _DEBUG_HOST_HANDLES 1
#define EMULATE_68K_TRACE 1
#define WATCH_68K_PC 1
#endif

// emulator kernel
#if __ppc__
// PPC can use either Asgard or Musashi
// default is Asgard (faster)
#if !defined(USE_MUSASHI_68K_EMU)
#define USE_ASGARD_PPC_68K_EMU 1
#define PATCH_VDI_PPC 1
#endif
#else
// i386 never uses Asgard
// i386 always uses Musashi
#undef USE_ASGARD_PPC_68K_EMU
#define USE_MUSASHI_68K_EMU 1
#endif

#endif
