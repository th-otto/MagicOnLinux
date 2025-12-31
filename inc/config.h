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

#define PROGRAM_NAME "MagicOnLinux v0.9"
#define VERSION_STRING "0.9"
#define PROGRAM_VERSION_MAJOR 0
#define PROGRAM_VERSION_MINOR 9
#define MAGIC_KERNEL_API_VERSION  1     // must match the API of the loaded kernel file

#define STE_COLOUR_PALETTE      // STe has additional low bit at positions 3,7 and 11

#define DISKIMAGE_FILENAME_EXT "st,raw-disk-image,img,fd,qd"

// debug output for debug configuration
#if !defined(NDEBUG)
//#define _DEBUG_MAGIC
//#define _DEBUG_BASEPAGE
//#define _DEBUG_KBD_AND_MOUSE
//#define _DEBUG_KB_CRITICAL_REGION
//#define _DEBUG_EVENTS
//#define _DEBUG_XFS
#define _DEBUG_VOLUME_IMAGES
//#define _DEBUG_HOST_HANDLES 1
#define _DEBUG_WRITEPROTECT_ATARI_OS
#define _DEBUG_WATCH_68K_VECTOR_CHANGE  // watch write access to addresses 0..0x140
#define EMULATE_NULLPTR_BUSERR          // block 68k access to addresses 0..8 in user mode
#endif


#endif
