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
* Serielle Schnittstelle f�r MagicMacX
*
*/

#ifndef _MAGICSERIAL_INCLUDED_
#define _MAGICSERIAL_INCLUDED_

//#define USE_SERIAL_SELECT

#ifndef USE_SERIAL_SELECT
#define SERIAL_IBUFLEN	32
#endif

class CMagiCSerial
{
   public:
	// Konstruktor
	CMagiCSerial();
	// Destruktor
	~CMagiCSerial();
	// Initialisierung
	uint32_t Open(const char *BsdPath);
	// Schlie�en
	uint32_t Close();
	// Ge�ffnet?
	bool IsOpen();
	// Ausgabe
	uint32_t Write(unsigned int cnt, const char *pBuffer);
	// Eingabe
	uint32_t Read(unsigned int cnt, char *pBuffer);
	// Ausgabestatus
	bool WriteStatus(void);
	// Eingabestatus
	bool ReadStatus(void);
	// Konfiguration
	uint32_t Config(
			bool bSetInputBaudRate,
			uint32_t InputBaudRate,
			uint32_t *pOldInputBaudrate,
			bool bSetOutputBaudRate,
			uint32_t OutputBaudRate,
			uint32_t *pOldOutputBaudrate,
			bool bSetXonXoff,
			bool bXonXoff,
			bool *pbOldXonXoff,
			bool bSetRtsCts,
			bool bRtsCts,
			bool *pbOldRtsCts,
			bool bSetParityEnable,
			bool bParityEnable,
			bool *pbOldParityEnable,
			bool bSetParityEven,
			bool bParityEven,
			bool *pbOldParityEven,
			bool bSetnBits,
			unsigned int nBits,
			unsigned int *pOldnBits,
			bool bSetnStopBits,
			unsigned int nStopBits,
			unsigned int *pOldnStopBits);

	// wartet, bis der Ausgabepuffer geleert ist.
	uint32_t Drain(void);
	// l�scht Ein-/Ausgangspuffer
	uint32_t Flush(bool bInputBuffer, bool bOutputBuffer);

   private:
   	// Unix-Dateideskriptor f�r Modem
   	int m_fd;
	// Eingangspuffer
#ifndef USE_SERIAL_SELECT
	char m_InBuffer[SERIAL_IBUFLEN];
	unsigned int m_InBufFill;
#endif
};

#endif
