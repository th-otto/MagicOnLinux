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
* User provided memory access functions for the 68k emulator
*
*/

#include "config.h"
// system headers
#include <cassert>
#include <cerrno>
#include <sys/param.h>
#include <time.h>
// programm headers
#include "Debug.h"
#include "Globals.h"
#include "Atari.h"
#include "missing.h"

// compile time switches

#ifdef _DEBUG
#define _DEBUG_WRITEPROTECT_ATARI_OS
#endif

// Stuff for the 68k emulator
extern "C" {

typedef unsigned int m68k_data_type;
typedef unsigned int m68k_addr_type;

extern m68k_data_type m68k_read_memory_8(m68k_addr_type address);
extern m68k_data_type m68k_read_memory_16(m68k_addr_type address);
extern m68k_data_type m68k_read_memory_32(m68k_addr_type address);
extern void m68k_write_memory_8(m68k_addr_type address, m68k_data_type value);
extern void m68k_write_memory_16(m68k_addr_type address, m68k_data_type value);
extern void m68k_write_memory_32(m68k_addr_type address, m68k_data_type value);
#ifdef MAGICMACX_DEBUG68K
uint32_t DebugCurrentPC;			// für Test der Bildschirmausgabe
static uint32_t WriteCounters[100];
extern void TellDebugCurrentPC(uint32_t pc);
void TellDebugCurrentPC(uint32_t pc)
{
	DebugCurrentPC = pc;
}
#endif

//static unsigned char *HostVideo2Addr;		// Beginn Bildschirmspeicher Host (Hintergrundpuffer)

static const char *AtariAddr2Description(uint32_t addr);

#if defined(MAGICMACX_DEBUG_SCREEN_ACCESS) || defined(PATCH_VDI_PPC)
static uint32_t p68k_OffscreenDriver = 0;
static uint32_t p68k_ScreenDriver = 0;
//#define ADDR_VDIDRVR_32BIT			0x1819c	// VDI-Treiber für "true colour" liegt hier
//#define p68k_OffscreenDriver		0x19cc0	// Offscreen-VDI-Treiber für "true colour" liegt hier
#endif

#ifdef PATCH_VDI_PPC
#include "VDI_PPC.c.h"
#endif


/** **********************************************************************************************
 *
 * @brief Debug helper to get the name of the addressed Atari chip from 68k address
 *
 * @param[in] addr		68k address
 *
 * @return name or "?", if unknown
 *
 ************************************************************************************************/

static const char *AtariAddr2Description(uint32_t addr)
{
	// Convert ST address to TT address

	if	((addr >= 0xff0000) && (addr < 0xffffff))
		addr |= 0xff000000;

	if	(addr == 0xffff8201)
		return("Videocontroller: Video base register high");

	if	(addr == 0xffff8203)
		return("Videocontroller: Video base register mid");

	if	(addr == 0xffff820d)
		return("Videocontroller: Video base register low (STE)");

	if	(addr == 0xffff8260)
		return("Videocontroller: ST shift mode register (0: 4 Planes, 1: 2 Planes, 2: 1 Plane)");

	// Interrupts A: Timer B/XMIT Err/XMIT Buffer Empty/RCV Err/RCV Buffer full/Timer A/Port I6/Port I7 (Bits 0..7)
	if	(addr == 0xfffffa0f)
		return("MFP: ISRA (Interrupt-in-service A)");

	// Interrupts B: Port I0/Port I1/Port I2/Port I3/Timer D/Timer C/Port I4/Port I5 (Bits 0..7)
	if	(addr == 0xfffffa11)
		return("MFP: ISRB (Interrupt-in-service B)");

	if	((addr >= 0xfffffa40) && (addr < 0xfffffa54))
		return("MC68881");

	return("?");
}


/** **********************************************************************************************
 *
 * @brief Helper to get the Atari process name, including "unknown"
 *
 * @param[in] address		68k address
 *
 * @return byte read, extended to 32-bit unsigned
 *
 ************************************************************************************************/

static void getAtariPrg(const char **pprocName, uint32_t *pact_pd)
{
	getActAtariPrg(pprocName, pact_pd);
	if	(*pprocName == nullptr)
	{
		*pprocName = "<unknown>";
	}
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: read a single byte
 *
 * @param[in] address		68k address
 *
 * @return byte read, extended to 32-bit unsigned
 *
 ************************************************************************************************/

m68k_data_type m68k_read_memory_8(m68k_addr_type address)
{
#if !COUNT_CYCLES
	if	(address < addr68kVideo)
	{
		// read regular memory
		return(*((uint8_t *) (addrOpcodeROM + address)));
	}
	else
#endif
	if	(address < addr68kVideoEnd)
	{
		// read from host's video memory
		return(*((uint8_t *) (hostVideoAddr + (address - addr68kVideo))));
	}
	else
	{
		// handle access error
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_read_memory_8(addr = 0x%08lx) --- bus error (%s) by process %s",
					 address, AtariAddr2Description(address), procName);
		/*
		if	(address == 0xfffffa11)
		{
			DebugWarning("CMagiC::ReadByte() --- Access to \"MFP ISRB\" is ignored to make ZBENCH.PRG working!");
		}
		else
		*/
			sendBusError(address, "read byte");

		return(0xff);
	}
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: read a single 16-bit halfword in big-endian mode
 *
 * @param[in] address		68k address
 *
 * @return halfword read, extended to 32-bit unsigned
 *
 ************************************************************************************************/

m68k_data_type m68k_read_memory_16(m68k_addr_type address)
{
#if !COUNT_CYCLES
	if	(address < addr68kVideo)
	{
		// read regular memory and convert to big-endian
		return getAtariBE16(addrOpcodeROM + address);
	}
	else
#endif
	if	(address < addr68kVideoEnd)
	{
		// read from host's video memory
		if (gbAtariVideoRamHostEndian)
		{
			// x86 has bgr instead of rgb
			return *((uint16_t *) (hostVideoAddr + (address - addr68kVideo)));
		}
		else
		{
			return getAtariBE16(hostVideoAddr + (address - addr68kVideo));
		}
	}
	else
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_read_memory_16(addr = 0x%08lx) --- bus error (%s) by process %s",
					 address, AtariAddr2Description(address), procName);
		sendBusError(address, "read word");
		return(0xffff);		// in fact this was a bus error
	}
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: read a single 32-bit word in big-endian mode
 *
 * @param[in] address		68k address
 *
 * @return word read
 *
 ************************************************************************************************/

m68k_data_type m68k_read_memory_32(m68k_addr_type address)
{
#if !COUNT_CYCLES
	if	(address < addr68kVideo)
	{
		// read regular memory and convert to big-endian
		return getAtariBE32(addrOpcodeROM + address);
	}
	else
#endif
	if	(address < addr68kVideoEnd)
	{
		// read from host's video memory
		if (gbAtariVideoRamHostEndian)
		{
			// x86 has bgr instead of rgb
			return *((uint32_t *) (hostVideoAddr + (address - addr68kVideo)));
		}
		else
		{
			return getAtariBE32(hostVideoAddr + (address - addr68kVideo));
		}
	}
	else
	{
		// handle access error
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_read_memory_32(addr = 0x%08lx) --- bus error by process %s", address, procName);
		return(0xffffffff);		// in fact this was a bus error
	}
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: write a single byte
 *
 * @param[in] address		68k address
 * @param[in] value			datum to write
 *
 ************************************************************************************************/

void m68k_write_memory_8(m68k_addr_type address, m68k_data_type value)
{
#ifdef _DEBUG_WRITEPROTECT_ATARI_OS
	if	((address >= addrOsRomStart) && (address < addrOsRomEnd))
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_write_memory_8() --- MagiC ROM tried to be overwritten by process %s at address 0x%08x",
						procName, address);
		sendBusError(address, "write byte");
	}
#endif
#if !COUNT_CYCLES
	if	(address < addr68kVideo)
	{
		// write regular memory
		*((uint8_t *) (addrOpcodeROM + address)) = (uint8_t) value;
	}
	else
#endif
	if	(address < addr68kVideoEnd)
	{
		address -= addr68kVideo;
		*((uint8_t *) (hostVideoAddr + address)) = (uint8_t) value;
		//*((uint8_t*) (HostVideo2Addr + address)) = (uint8_t) value;
		atomic_store(&gbAtariVideoBufChanged, true);
		//DebugInfo("vchg");
		//usleep(100000);
	}
	else
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_write_memory_8(adr = 0x%08lx, dat = 0x%02hx) --- bus error (%s) by process %s",
					 address, (uint8_t) value, AtariAddr2Description(address), procName);

		if	((address == 0xffff8201) || (address == 0xffff8203) || (address == 0xffff820d))
		{
			DebugWarning("m68k_write_memory_8() --- Access to \"Video Base Register\" ignored to make PD.PRG working!");
		}
		else
		if	(address == 0xfffffa11)
		{
			DebugWarning("m68k_write_memory_8() --- Access to \"MFP ISRB\" ignored to make ZBENCH.APP working!");
		}
		else
			sendBusError(address, "write byte");
	}
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: write a single 16-bit halfword in big-endian order
 *
 * @param[in] address		68k address
 * @param[in] value			datum to write
 *
 ************************************************************************************************/

void m68k_write_memory_16(m68k_addr_type address, m68k_data_type value)
{
#ifdef _DEBUG_WRITEPROTECT_ATARI_OS
	if	((address >= addrOsRomStart-1) && (address < addrOsRomEnd))
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_write_memory_16() --- MagiC ROM tried to be overwritten by process %s at address 0x%08x",
						procName, address);
		sendBusError(address, "write word");
	}
#endif
#if !COUNT_CYCLES
	if	(address < addr68kVideo)
	{
		// write regular 68k memory (big endian)
		setAtariBE16(addrOpcodeROM + address, value);
	}
	else
#endif
	if	(address < addr68kVideoEnd)
	{
		address -= addr68kVideo;
		if (gbAtariVideoRamHostEndian)
		{
			// x86 has bgr instead of rgb
			*((uint16_t *) (hostVideoAddr + address)) = (uint16_t) value;
		}
		else
		{
			setAtariBE16(hostVideoAddr + address, value);
		}

// //		*((uint16_t*) (HostVideo2Addr + address)) = (uint16_t) htobe16(value);
//		*((uint16_t *) (HostVideo2Addr + address)) = (uint16_t) value;	// x86 has bgr instead of rgb
		atomic_store(&gbAtariVideoBufChanged, true);
		//DebugInfo("vchg");
	}
	else
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_write_memory_16(addr = 0x%08lx, dat = 0x%04hx) --- bus error (%s) by process %s",
					 address, (uint16_t) value, AtariAddr2Description(address), procName);
		sendBusError(address, "write word");
	}
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: write a single 32-bit word in big-endian order
 *
 * @param[in] address		68k address
 * @param[in] value			datum to write
 *
 ************************************************************************************************/

void m68k_write_memory_32(m68k_addr_type address, m68k_data_type value)
{
#ifdef _DEBUG_WRITEPROTECT_ATARI_OS
	if	((address >= addrOsRomStart - 3) && (address < addrOsRomEnd))
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_write_memory_32() --- MagiC ROM tried to be overwritten by process %s at address 0x%08x",
						procName, address);
		sendBusError(address, "write long");
	}
#endif
#if !COUNT_CYCLES
	if	(address < addr68kVideo)
	{
		// write regular 68k memory (big endian)
		setAtariBE32(addrOpcodeROM + address, value);
	}
	else
#endif
	if	(address < addr68kVideoEnd)
	{
#ifdef MAGICMACX_DEBUG_SCREEN_ACCESS
		if	((DebugCurrentPC >= p68k_ScreenDriver + 0x97a) &&
			 (DebugCurrentPC <= p68k_ScreenDriver + 0xa96))
		{
			(WriteCounters[0])++;
			// Screen driver
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_ScreenDriver + 0xbd8)
		{
			(WriteCounters[1])++;
			// Screen driver
			// 8*16 Text im Textmodus (echter VT52)
			//value = 0x00ff0000;
		}
		else
		if	(DebugCurrentPC == p68k_ScreenDriver + 0xbec)
		{
			(WriteCounters[2])++;
			// 8*16 Texthintergrund im Textmodus (echter VT52)
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_ScreenDriver + 0xfe4) &&
			 (DebugCurrentPC <= p68k_ScreenDriver + 0xff2))
		{
			(WriteCounters[3])++;
			//value = 0x00ff0000;
		}
		else
		if	((DebugCurrentPC >= p68k_ScreenDriver + 0x1018) &&
			 (DebugCurrentPC <= p68k_ScreenDriver + 0x1026))
		{
			(WriteCounters[4])++;
			// gelöschter Bildschirm im Textmodus (echter VT52)
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_ScreenDriver + 0x107c) &&
			 (DebugCurrentPC <= p68k_ScreenDriver + 0x1084))
		{
			(WriteCounters[5])++;
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_ScreenDriver + 0x10ac) &&
			 (DebugCurrentPC <= p68k_ScreenDriver + 0x10ca))
		{
			(WriteCounters[6])++;
			// Maushintergrund
			//value = 0x00ff0000;
		}
		else
		if	((DebugCurrentPC >= p68k_ScreenDriver + 0x1188) &&
			 (DebugCurrentPC <= p68k_ScreenDriver + 0x11a6))
		{
			(WriteCounters[7])++;
			// Bildschirm-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_ScreenDriver + 0x11ea)
		{
			(WriteCounters[8])++;
			// Bildschirm-Treiber
			// Mauszeiger-Umriß
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_ScreenDriver + 0x11f8)
		{
			(WriteCounters[9])++;
			// Bildschirm-Treiber
			// Mauszeiger-Inneres
			//value = 0x00ffff00;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0xf7a)
		{
			(WriteCounters[10])++;
			// Offscreen-Treiber
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0xfac)
		{
			(WriteCounters[11])++;
			// Offscreen-Treiber
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0xfca)
		{
			(WriteCounters[12])++;
			// Offscreen-Treiber
			//value = 0x00ff0000;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0xff6)
		{
			(WriteCounters[13])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x100a)
		{
			(WriteCounters[14])++;
			// Offscreen-Treiber
			// Linien
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x108a) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x10c6))
		{
			(WriteCounters[15])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x10e8)
		{
			(WriteCounters[16])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1126) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1162))
		{
			(WriteCounters[17])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x11ae) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x11ee))
		{
			(WriteCounters[18])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1212) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x124e))
		{
			(WriteCounters[19])++;
			// senkrechte Linien
			// Offscreen-Treiber
			//value = 0x00ff0000;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1306)
		{
			(WriteCounters[20])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1312)
		{
			(WriteCounters[21])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1328)
		{
			(WriteCounters[22])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1344)
		{
			(WriteCounters[23])++;
			// Offscreen-Treiber
			//value = 0x00ff0000;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x136c)
		{
			(WriteCounters[24])++;
			// Offscreen-Treiber
			//value = 0x00ff0000;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x13c0)
		{
			(WriteCounters[25])++;
			// Offscreen-Treiber
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x13de)
		{
			(WriteCounters[26])++;
			// Offscreen-Treiber
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1444) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1452))
		{
			(WriteCounters[27])++;
			// Offscreen-Treiber
			// gefüllte Flächen (schon PPC)
			//value = 0x00ff0000;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1668) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1686))
		{
			(WriteCounters[28])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1690) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x16ae))
		{
			(WriteCounters[29])++;
			// Offscreen-Treiber
			// Teile gefüllter Flächen
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x16c4) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x16e2))
		{
			(WriteCounters[30])++;
			// Offscreen-Treiber
			//value = 0x00ff0000;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1742) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1760))
		{
			(WriteCounters[31])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x18f8) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1916))
		{
			(WriteCounters[32])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x19ec) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1a28))
		{
			(WriteCounters[33])++;
			// Offscreen-Treiber
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1b34)
		{
			(WriteCounters[34])++;
			// Offscreen-Treiber
			// Textvordergrund
			//value = 0x00ff0000;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1b3c)
		{
			(WriteCounters[35])++;
			// Offscreen-Treiber
			// Texthintergrund
			//value = 0x000000ff;
		}
		else
		if	(DebugCurrentPC == p68k_OffscreenDriver + 0x1b6a)
		{
			(WriteCounters[36])++;
			// Offscreen-Treiber
			// value = 0x000000ff;
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x1d6e) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x1d74))
		{
			(WriteCounters[37])++;
			// Offscreen-Treiber
			// Fenster verschieben!
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x207a) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x2086))
		{
			(WriteCounters[38])++;
			// Rasterkopie im Modus "Quelle AND Ziel"
			// Offscreen-Treiber
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x20cc) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x20d2))
		{
			(WriteCounters[39])++;
			// Offscreen-Treiber
		}
		else
		if	((DebugCurrentPC >= p68k_OffscreenDriver + 0x211a) &&
			 (DebugCurrentPC <= p68k_OffscreenDriver + 0x2126))
		{
			(WriteCounters[40])++;
			// Offscreen-Treiber
		}
		else
		{
			(WriteCounters[41])++;
			CDebug::DebugError("#### falscher Bildspeicherzugriff bei PC = 0x%08x", DebugCurrentPC);
		}

#endif
		address -= addr68kVideo;
		if (gbAtariVideoRamHostEndian)
		{
			// x86 has brg instead of rgb
			*((uint32_t *) (hostVideoAddr + address)) = value;
		}
		else
		{
			setAtariBE32(hostVideoAddr + address, value);
		}

// //		*((uint32_t*) (HostVideo2Addr + address)) = value;		// x86 has brg instead of rgb
//		*((uint32_t *) (HostVideo2Addr + address)) = htobe32(value);
		atomic_store(&gbAtariVideoBufChanged, true);
		//DebugInfo("vchg");
	}
	else
	{
		const char *procName;
		uint32_t act_pd;
		getAtariPrg(&procName, &act_pd);

		DebugError("m68k_write_memory_32(addr = 0x%08lx, dat = 0x%08lx) --- bus error (%s) by process %s",
					 address, value, AtariAddr2Description(address), procName);
		sendBusError(address, "write long");
	}
}

} // end extern "C"
