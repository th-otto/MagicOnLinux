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

// System-Header
#include <stdint.h>
#include <endian.h>
#include <stdio.h>
// Programm-Header
#include "MyPreferences.h"

// Schalter

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

//#define min(a,b) (((a) < (b)) ? (a) : (b))
//#define max(a,b) (((a) > (b)) ? (a) : (b))
#define MAX_ATARIMEMSIZE	(2U*1024U*1024U*1024U)		// 2 Gigabytes

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

// compatibility to macOS
typedef int OSStatus;

enum OSErr
{
	noErr = 0,
	memFullErr = -1
};

// intermediate replacement for error alert
static inline void MyAlert(const char *a, const char *b)
{
 	fprintf(stderr, "%s/%s\n", a, b);
}

// helper
int64_t getFileSize(const char *path);


// global functions used by XCMD
extern void MMX_BeginDialog(void);
extern void MMX_EndDialog(void);
// global function used by CMagiCWindow to report closing
extern void SendWindowClose( void );
// global function used by CMagiCWindow to report collapsing
extern void SendWindowCollapsing( void );
// global function used by CMagiCWindow to report collapsed window has re-expanded
extern void SendWindowExpanded( void );
// global function used by CMagiCWindow to report keyboard focus acquired
extern void SendWindowFocusAcquired( void );
// global function used by CMagiCWindow to report keyboard focus relinguish
extern void SendWindowFocusRelinguish( void );
// global function used by CMagiCWindow to report mouse clicks
extern void SendWindowMoveHandler( uint32_t evkind, Rect *pNewRect );
// global function used by CMagiCWindow to report mouse clicks
extern int SendMouseButtonHandler( unsigned int NumOfButton, bool bIsDown );
// global function used by CMagiC to report 68k exceptions
extern void Send68kExceptionData(
			uint16_t exc,
			uint32_t ErrAddr,
			char *AccessMode,
			uint32_t pc,
			uint16_t sr,
			uint32_t usp,
			uint32_t *pDx,
			uint32_t *pAx,
			const char *ProcPath,
			uint32_t pd);
// global function used by AtariSysHalt
extern void SendSysHaltReason(const char *Reason);
extern void UpdateAtariDoubleBuffer(void);

// TODO: needed?
//extern DialogItemIndex MyAlert(int16_t alertID, AlertType alertType);

class CGlobals
{
     public:
	CGlobals();
	static char s_atariKernelPath[1024];
	static char s_atariRootfsPath[1024];
	static char s_atariScrapFileUnixPath[1024];
	static char s_atariTempFilesUnixPath[1024];
	static const unsigned s_ProgramVersionMajor = 0;
	static const unsigned s_ProgramVersionMinor = 1;
	static int getDosPath(
				const char *hostpPath,
				char *pBuf,
				unsigned uBufLen);
	static int Init(void);
	static bool s_bRunning;

	// leave Mac menu visible, no fullscreen
	static bool s_bShowMacMenu;
	// curent Atari screen size
	static bool s_bAtariScreenManualSize;
	static unsigned short s_AtariScreenX;
	static unsigned short s_AtariScreenY;
	static unsigned short s_AtariScreenWidth;
	static unsigned short s_AtariScreenHeight;

	static CMyPreferences s_Preferences;
	static bool s_XFSDrvWasChanged[NDRIVES];

   private:
};

extern CGlobals Globals;
#endif
