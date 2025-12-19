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
* Printing functionality
*
*/

#include "config.h"

#include <errno.h>

#include "Globals.h"
#include "Debug.h"
#include "Atari.h"
#include "preferences.h"
#include "MagiCPrint.h"


FILE *CMagiCPrint::m_printFile;
int CMagiCPrint::m_PrintFileCounter;
bool CMagiCPrint::bTempFileCreated;
uint32_t CMagiCPrint::s_LastPrinterAccess = 0;


/** **********************************************************************************************
 *
 * @brief Initialisation (called from main thread)
 *
 ************************************************************************************************/
void CMagiCPrint::init()
{
    m_printFile = nullptr;
    m_PrintFileCounter = 0;
    bTempFileCreated = false;
}


/** **********************************************************************************************
 *
 * @brief Deinitialisation (called from main thread)
 *
 * @note removes all printer files
 *
 ************************************************************************************************/
void CMagiCPrint::exit()
{
    if (bTempFileCreated)
    {
        char command[PATH_MAX + 100];
        int ierr;

        // Make sure to enclose path in quotation marks, in case the path contains spaces.
        // Issue two commands, first one moves to path, seconds one deletes files.
        // The concatenation with && makes sure that the rm command is not issued
        // when cd command failed.
        // Redirect stdout and stderr to PrintCommand_rm.txt, for debugging purposes.

        sprintf(command, "cd \"%s\" && rm PrintQueue/MagiCPrintFile????????",
                            Preferences::AtariTempFilesUnixPath);

        strcat(command, " >&\"");
        strcat(command, Preferences::AtariTempFilesUnixPath);
        strcat(command, "PrintQueue/PrintCommand_rm.txt\"");

        DebugInfo2("() -- run %s", command);
        ierr = system(command);
        if    (ierr)
        {
            DebugError2("() -- error code %d when removing files", ierr);
        }
    }
}


/** **********************************************************************************************
 *
 * @brief Get printer output state (called from emulator thread)
 *
 * @return status
 * @retval -1  ready
 * @retval 0   not ready
 *
 ************************************************************************************************/
uint32_t CMagiCPrint::GetOutputStatus(void)
{
    return 0xffffffff;        // always ready
}


/** **********************************************************************************************
 *
 * @brief Read data from printer (called from emulator thread)
 *
 * @param[out] pBuf        read buffer
 * @param[in]  cnt         number of bytes to read
 *
 * @return number of bytes read or negative error code
 *
 * @note Reading from printer is not supported
 *
 ************************************************************************************************/
uint32_t CMagiCPrint::Read(uint8_t *pBuf, uint32_t cnt)
{
    (void) pBuf;
    (void) cnt;
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Write data to printer (called from emulator thread)
 *
 * @param[out] pBuf        write buffer
 * @param[in]  NumOfBytes  number of bytes to write
 *
 * @return number of bytes written or negative error code
 *
 ************************************************************************************************/
uint32_t CMagiCPrint::Write(const uint8_t *pBuf, uint32_t cnt)
{
    char PrintFileName[PATH_MAX + 100];
    long OutCnt;


    if (m_printFile == nullptr)
    {
        // Printer output file not open or already closed. Create new one.

        sprintf(PrintFileName, "%s/PrintQueue/MagiCPrintFile%08d", Preferences::AtariTempFilesUnixPath, m_PrintFileCounter++);
        DebugInfo2("() -- create printer output file \"%s\" an", PrintFileName);
        m_printFile = fopen(PrintFileName, "w");
        if (m_printFile == nullptr)
        {
            DebugError2("() : fopen(\"%s\", \"w\") -> %s", PrintFileName, strerror(errno));
            return 0;        // nothing written
        }
        bTempFileCreated = true;
    }

    // write data to print file

    OutCnt = (long) cnt;
    cnt = fwrite(pBuf, 1, OutCnt, m_printFile);

//    CDebug::DebugInfo("CMagiCPrint::Write() --- s_LastPrinterAccess = %u", s_LastPrinterAccess);

    return cnt;
}


/** **********************************************************************************************
 *
 * @brief Close and send printer output file (called from emulator thread)
 *
 * @return zero or negative error code
 *
 ************************************************************************************************/
uint32_t CMagiCPrint::ClosePrinterFile(void)
{
    char command[PATH_MAX * 3];
    int ierr;


    if ((m_printFile == nullptr) || (!m_PrintFileCounter))
        return 0;

    fclose(m_printFile);
    m_printFile = nullptr;

    // Generate filename of previous print file, enclosed in quotation marks.
    // This is necessary if there are spaces in the path.
    sprintf(command, "%s \"%s/PrintQueue/MagiCPrintFile%08d\" >&\"%s/PrintQueue/PrintCommand_stdout.txt\"",
                        Preferences::szPrintingCommand,
                        Preferences::AtariTempFilesUnixPath,
                        m_PrintFileCounter - 1,
                        Preferences::AtariTempFilesUnixPath
                    );

    DebugInfo2("() -- run command: %s", command);
    ierr = system(command);
    if (ierr)
    {
        DebugError2("() -- error %d during printing", ierr);
    }

    return ierr;
}


/**********************************************************************
*
* Callback des Emulators: Ausgabestatus des Druckers abfragen
* Rückgabe: -1 = bereit 0 = nicht bereit
*
**********************************************************************/

uint32_t CMagiCPrint::AtariPrtOs(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
    return CMagiCPrint::GetOutputStatus();
}


/**********************************************************************
*
* Callback des Emulators: Zeichen von Drucker lesen
* Rückgabe: Zeichen in Bit 0..7, andere Bits = 0
*
**********************************************************************/

uint32_t CMagiCPrint::AtariPrtIn(uint32_t params, uint8_t *addrOffset68k)
{
    unsigned char c;
    uint32_t n;

    (void) params;
    (void) addrOffset68k;
    n = CMagiCPrint::Read(&c, 1);
    if (!n)
        return 0;
    else
        return(c);
}


/**********************************************************************
*
* Callback des Emulators: Zeichen auf Drucker ausgeben
* params        Zeiger auf auszugebendes Zeichen (16 Bit)
* Rückgabe: 0 = Timeout -1 = OK
*
**********************************************************************/

uint32_t CMagiCPrint::AtariPrtOut(uint32_t params, uint8_t *addrOffset68k)
{
    uint32_t ret;

    DebugInfo2("()");
    ret = CMagiCPrint::Write(addrOffset68k + params + 1, 1);
    // Zeitpunkt (200Hz) des letzten Druckerzugriffs merken
    s_LastPrinterAccess = be32toh(*((uint32_t *) (addrOffset68k + _hz_200)));
    if (ret == 1)
        return(0xffffffff);        // OK
    else
        return 0;                // Fehler
}


/**********************************************************************
*
* Callback des Emulators: mehrere Zeichen auf Drucker ausgeben
* Rückgabe: Anzahl geschriebener Zeichen
*
**********************************************************************/

uint32_t CMagiCPrint::AtariPrtOutS(uint32_t params, uint8_t *addrOffset68k)
{
    struct PrtOutParm
    {
        uint32_t buf;
        uint32_t cnt;
    };
     PrtOutParm *thePrtOutParm = (PrtOutParm *) (addrOffset68k + params);
     uint32_t ret;


//    CDebug::DebugInfo("CMagiC::AtariPrtOutS()");
    ret = CMagiCPrint::Write(addrOffset68k + be32toh(thePrtOutParm->buf), be32toh(thePrtOutParm->cnt));
    // Zeitpunkt (200Hz) des letzten Druckerzugriffs merken
    s_LastPrinterAccess = be32toh(*((uint32_t *) (addrOffset68k + _hz_200)));
    return ret;
}
