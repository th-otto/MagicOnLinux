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
*
* Enth�lt alles, was mit "MagicMac OS" zu tun hat
*
*/

// System-Header
// Programm-Header
#include "Globals.h"
#include "osd_cpu.h"
//#include "MagiCScreen.h"
#include "XCmd.h"
#include "HostXFS.h"
#include "MagiCKeyboard.h"
#include "MagiCMouse.h"
#include "MagiCSerial.h"
// Schalter

#define KEYBOARDBUFLEN	32
#define N_ATARI_FILES		8

class CMagiCPrint
{
   public:
	// Konstruktor
	CMagiCPrint();
	// Destruktor
	~CMagiCPrint();

	uint32_t GetOutputStatus(void);
	uint32_t Read(uint8_t *pBuf, uint32_t NumOfBytes);
	uint32_t Write(const uint8_t *pBuf, uint32_t NumOfBytes);
	uint32_t ClosePrinterFile(void);

   private:
	FILE *m_printFile;
	int m_PrintFileCounter;
	static bool bTempFileCreated;
};
