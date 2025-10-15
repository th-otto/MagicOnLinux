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

/*
*
* Printing functionality
*
*/

#include <stdio.h>
#include <stdint.h>

class CMagiCPrint
{
   public:
    static void init();
    static void exit();

    static uint32_t GetOutputStatus(void);
    static uint32_t Read(uint8_t *pBuf, uint32_t cnt);
    static uint32_t Write(const uint8_t *pBuf, uint32_t cnt);
    static uint32_t ClosePrinterFile(void);

   private:
    static FILE *m_printFile;
    static int m_PrintFileCounter;
    static bool bTempFileCreated;
};
