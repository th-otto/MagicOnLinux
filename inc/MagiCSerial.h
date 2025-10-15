/*
 * Copyright (C) 1990-2018/25 Andreas Kromke, andreas.kromke@gmail.com
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
* Serial interface (AUX) functionality
*
*/

#ifndef _MAGICSERIAL_INCLUDED_
#define _MAGICSERIAL_INCLUDED_

//#define USE_SERIAL_SELECT

#ifndef USE_SERIAL_SELECT
#define SERIAL_IBUFLEN    32
#endif

class CMagiCSerial
{
   public:
    static void init();
    static void exit();

    static uint32_t Open();
    static uint32_t Close();
    static bool IsOpen();
    static uint32_t Read(char *pBuf, uint32_t cnt);
    static uint32_t Write(const char *pBuf, uint32_t cnt);
    static bool WriteStatus(void);
    static bool ReadStatus(void);
    static uint32_t Config(
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

    static uint32_t Drain(void);
    static uint32_t Flush(bool bInputBuffer, bool bOutputBuffer);

   private:
    static int m_fd;
#ifndef USE_SERIAL_SELECT
    static char m_InBuffer[SERIAL_IBUFLEN];
    static unsigned int m_InBufFill;
#endif
};

#endif
