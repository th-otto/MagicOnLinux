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
* Serial interface (AUX) functionality
*
*/

#include "config.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/param.h>
#include <unistd.h>
#include <termios.h>
#include "Globals.h"
#include "Debug.h"
#include "MagiCSerial.h"
#include "preferences.h"
#ifdef _DEBUG
//#define DEBUG_VERBOSE
#endif

// static attributes

#define ISPEED_ALWAYS_EQUAL_OSPEED    1

struct termios gOriginalTTYAttrs;
struct termios gActualTTYAttrs;

int CMagiCSerial::m_fd;
#ifndef USE_SERIAL_SELECT
char CMagiCSerial::m_InBuffer[SERIAL_IBUFLEN];
unsigned int CMagiCSerial::m_InBufFill;
#endif


/** **********************************************************************************************
 *
 * @brief Initialisation (called from main thread)
 *
 ************************************************************************************************/
void CMagiCSerial::init()
{
    m_fd = -1;
    gActualTTYAttrs.c_ospeed = gActualTTYAttrs.c_ispeed = 0xffffffff;    // invalid
}


/** **********************************************************************************************
 *
 * @brief Deinitialisation (called from main thread)
 *
 * @note removes all printer files
 *
 ************************************************************************************************/
void CMagiCSerial::exit()
{
    if (m_fd != -1)
    {
        close(m_fd);
    }
}


/** **********************************************************************************************
 *
 * @brief Open serial device when first needed (called from emulator thread)
 *
 * @return zero or negative error code
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::Open()
{
#ifndef USE_SERIAL_SELECT
    m_InBufFill = 0;
#endif

    DebugInfo2("() - Open device file \"%s\"", Preferences::szAuxPath);

    m_fd = open(Preferences::szAuxPath, O_RDWR | O_NOCTTY | O_NDELAY);
    if (m_fd == -1)
    {
        DebugError2("() : open(\"%s\") -> %s", Preferences::szAuxPath, strerror(errno));
        return(uint32_t) -1;
    }

    if (fcntl(m_fd, F_SETFL, 0) == -1)
    {
        DebugError2("() : fcntl(clearing O_NDELAY) -> %s", strerror(errno));
        return(uint32_t) -2;
    }

    // Get the current options and save them for later reset
    if (tcgetattr(m_fd, &gOriginalTTYAttrs) == -1)
    {
        DebugError2("() : tcgetattr() -> %s", strerror(errno));
        return(uint32_t) -3;
    }

    // Set raw input, one second timeout
    // These options are documented in the man page for termios
    // (in Terminal enter: man termios)
    gActualTTYAttrs = gOriginalTTYAttrs;
    gActualTTYAttrs.c_cflag |= (CLOCAL | CREAD);
    gActualTTYAttrs.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    gActualTTYAttrs.c_oflag &= ~OPOST;
    gActualTTYAttrs.c_cc[VMIN] = 0;
    gActualTTYAttrs.c_cc[VTIME] = 0;        // immediately return if no character available. Was 10, i.e. 10*0,1s = 1s;

    // Default: Same speed in both directions
    gActualTTYAttrs.c_ospeed = gActualTTYAttrs.c_ispeed;

    // Default: no parity
    gActualTTYAttrs.c_cflag &= ~(PARENB);

    // Set the options
    if (tcsetattr(m_fd, TCSANOW, &gActualTTYAttrs) == -1)
    {
        DebugError2("() : tcsetattr() -> %s", strerror(errno));
        return (uint32_t) -4;
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Close serial device (currently not called)
 *
 * @return zero or negative error code
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::Close()
{
    if (m_fd != -1)
    {
        DebugInfo2("() -- Close device file");
        close(m_fd);
        m_fd = -1;
    }
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Check if serial interface is open (called from emulator thread)
 *
 * @return true or false
 *
 ************************************************************************************************/
bool CMagiCSerial::IsOpen()
{
    return(m_fd != -1);
}


/**********************************************************************
*
* Input (read)
*
**********************************************************************/

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
uint32_t CMagiCSerial::Read(char *pBuf, uint32_t cnt)
{
    int ret;
    int nBytesRead = 0;

#ifndef USE_SERIAL_SELECT
    nBytesRead = (int) MIN(m_InBufFill, cnt);
    if    (nBytesRead)
    {
        memcpy(pBuf, m_InBuffer, (size_t) nBytesRead);
        memmove(m_InBuffer, m_InBuffer + nBytesRead, (size_t) (m_InBufFill - nBytesRead));
#ifdef DEBUG_VERBOSE
        if    (nBytesRead)
            DebugInfo("CMagiCSerial::Read() -- buflen = %d, %d Zeichen erhalten:", cnt, nBytesRead);

        for    (register int i = 0; i < nBytesRead; i++)
        {
            DebugInfo("CMagiCSerial::Read() -- <== c = 0x%02x (%c)", ((unsigned int) (pBuffer[i])) & 0xff, ((pBuffer[i] >= ' ') && (pBuffer[i] <= 'z')) ? pBuffer[i] : '?');
        }
#endif
        m_InBufFill -= nBytesRead;
        pBuf += nBytesRead;
        cnt -= nBytesRead;
    }

    if    (cnt)
        ret = read(m_fd, pBuf, cnt);
    else
        ret = 0;
#else
    ret = read(m_fd, pBufr, cnt);
#endif

#ifdef DEBUG_VERBOSE
    if (ret != -1)
    {
        if (ret)
            DebugInfo("CMagiCSerial::Read() -- buflen = %d, %d Zeichen erhalten:", cnt, ret);

        for (register int i = 0; i < ret; i++)
        {
            DebugInfo("CMagiCSerial::Read() -- <== c = 0x%02x (%c)", ((unsigned int) (pBuffer[i])) & 0xff, ((pBuffer[i] >= ' ') && (pBuffer[i] <= 'z')) ? pBuffer[i] : '?');
        }
    }
#endif
    if (ret == -1)
    {
        DebugError2("() : read() -> %s", strerror(errno));
        return 0xffffffff;        // Atari error code
    }

    return(uint32_t) ret + nBytesRead;    // number of characters
}


/** **********************************************************************************************
 *
 * @brief Write data to serial device (called from emulator thread)
 *
 * @param[out] pBuf        write buffer
 * @param[in]  cnt         number of bytes to write
 *
 * @return number of bytes written or negative error code
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::Write(const char *pBuf, uint32_t cnt)
{
    int ret;

#ifdef DEBUG_VERBOSE
    DebugInfo("CMagiCSerial::Write() -- %d Zeichen schreiben:", cnt);
    for (register int i = 0; i < cnt; i++)
    {
        DebugInfo("CMagiCSerial::Write() -- ==> c = 0x%02x (%c)", (unsigned int) pBuffer[i], ((pBuffer[i] >= ' ') && (pBuffer[i] <= 'z')) ? pBuffer[i] : '?');
    }
#endif
    ret = write(m_fd, pBuf, cnt);
    if (ret == -1)
    {
        DebugError2("() : write() -> %s", strerror(errno));
        return 0xffffffff;        // Atari error code
    }

    return(uint32_t) ret;    // number of characters
}


/** **********************************************************************************************
 *
 * @brief Get output (write) state (called from emulator thread)
 *
 * @return status
 * @retval true   ready
 * @retval false  not ready
 *
 ************************************************************************************************/
bool CMagiCSerial::WriteStatus(void)
{
    int ret;
    static struct timeval Timeout = {0,0};
    fd_set fdset;

    if (m_fd == -1)
    {
        return false;
    }

    FD_ZERO(&fdset);
    FD_SET(m_fd, &fdset);
    ret = select(1, NULL /*readfds*/, &fdset/*writefds*/, NULL, &Timeout);
    return (ret == 1);    // number of ready fds
}


/** **********************************************************************************************
 *
 * @brief Get input (read) state (called from emulator thread)
 *
 * @return status
 * @retval true   ready
 * @retval false  not ready
 *
 ************************************************************************************************/
bool CMagiCSerial::ReadStatus(void)
{
#ifdef USE_SERIAL_SELECT
    int ret;
    static struct bsd_timeval Timeout = {0,0};
    struct bsd_fdset fdset;


    if    (m_fd == -1)
        return(false);

    MY_FD_ZERO(&fdset);
    MY_FD_SET(m_fd, &fdset);
    ret = CMachO::select(1, &fdset /*readfds*/, NULL/*writefds*/, NULL, &Timeout);
    return(ret == 1);    // number of ready fds
#else
    int ret;

    if    (m_InBufFill)
        return(true);

    ret = read(m_fd, m_InBuffer, SERIAL_IBUFLEN);
    if    ((ret == -1) || (ret == 0))
        return(false);
    m_InBufFill = ret;
    return true;
#endif
}


/** **********************************************************************************************
 *
 * @brief Wait for output buffer to be written (called from emulator thread)
 *
 * @return zero or error code
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::Drain(void)
{
    int ret;

    ret = tcdrain(m_fd);
    if (ret)
        return (uint32_t) -1;

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Flush input and output buffer (called from emulator thread)
 *
 * @return zero or error code
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::Flush(bool bInputBuffer, bool bOutputBuffer)
{
    int action,ret;

#ifndef USE_SERIAL_SELECT
    m_InBufFill = 0;
#endif

    if (bInputBuffer && bOutputBuffer)
        action = TCIOFLUSH;
    else
    if (bInputBuffer)
        action = TCIFLUSH;
    else
    if (bOutputBuffer)
        action = TCOFLUSH;
    else
        action = 0;

    ret = tcflush(m_fd, action);

    if (ret)
        return((uint32_t) -1);

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Get or set baud rate, sync modes etc. (called from emulator thread)
 *
 * @param[in]  bSetInputBaudRate
 * @param[in]  InputBaudRate
 * @param[out] pOldInputBaudrate
 * @param[in]  bSetOutputBaudRate
 * @param[in]  OutputBaudRate
 * @param[out] pOldOutputBaudrate
 * @param[in]  bSetXonXoff
 * @param[in]  bXonXoff
 * @param[out] pbOldXonXoff
 * @param[in]  bSetRtsCts
 * @param[in]  bRtsCts
 * @param[out] pbOldRtsCts,
 * @param[in]  bSetParityEnable
 * @param[in]  bParityEnable
 * @param[out] pbOldParityEnable
 * @param[in]  bSetParityEven
 * @param[in]  bParityEven
 * @param[out] pbOldParityEven
 * @param[in]  bSetnBits
 * @param[in]  nBits
 * @param[out] pOldnBits
 * @param[in]  bSetnStopBits
 * @param[in]  nStopBits
 * @param[out] pOldnStopBits
 *
 * @return zero or negative error code
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::Config
(
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
    unsigned int *pOldnStopBits)
{
    bool bSet;


    DebugInfo2("()");

    if (pOldInputBaudrate != nullptr)
        *pOldInputBaudrate = (unsigned long) gActualTTYAttrs.c_ispeed;

    if (bSetInputBaudRate)
    {
        gActualTTYAttrs.c_ispeed = (long) InputBaudRate;
#if ISPEED_ALWAYS_EQUAL_OSPEED
        gActualTTYAttrs.c_ospeed = gActualTTYAttrs.c_ispeed;
#endif
        DebugInfo("   ispeed = %u, ospeed = %u", gActualTTYAttrs.c_ispeed, gActualTTYAttrs.c_ospeed);
        bSet = true;
    }

    if (pOldOutputBaudrate != nullptr)
        *pOldOutputBaudrate = (unsigned long) gActualTTYAttrs.c_ospeed;

    if (bSetOutputBaudRate)
    {
        gActualTTYAttrs.c_ospeed = (long) OutputBaudRate;
#if ISPEED_ALWAYS_EQUAL_OSPEED
        gActualTTYAttrs.c_ispeed = gActualTTYAttrs.c_ospeed;
#endif
        DebugInfo("   ispeed = %u, ospeed = %u", gActualTTYAttrs.c_ispeed, gActualTTYAttrs.c_ospeed);
        bSet = true;
    }

    // software synchronisation (XON/XOFF) on/off

    if (pbOldXonXoff != nullptr)
    {
        *pbOldXonXoff = (gActualTTYAttrs.c_iflag & (IXON | IXOFF)) != 0;
    }

    if (bSetXonXoff)
    {
        if (bXonXoff)
        {
            gActualTTYAttrs.c_iflag |= (IXON | IXOFF);
        }
        else
        {
            gActualTTYAttrs.c_iflag &= ~(IXON | IXOFF);
        }
        DebugInfo("   XON = %u, XOFF = %u", (gActualTTYAttrs.c_iflag & IXON) != 0, (gActualTTYAttrs.c_iflag & IXOFF) != 0);
        bSet = true;
    }

    // hardware synchronisation (RTS/CTS) on/off

    if (pbOldRtsCts != nullptr)
    {
        *pbOldRtsCts = (gActualTTYAttrs.c_cflag & CRTSCTS) != 0;
    }

    if (bSetRtsCts)
    {
        if (bRtsCts)
        {
            gActualTTYAttrs.c_cflag |= CRTSCTS;
        }
        else
        {
            gActualTTYAttrs.c_cflag &= ~CRTSCTS;
        }
        DebugInfo("   RTS/CTS = %u", (gActualTTYAttrs.c_cflag & CRTSCTS) != 0);
        bSet = true;
    }

    // parity on/off

    if (pbOldParityEnable != nullptr)
    {
        *pbOldParityEnable = (gActualTTYAttrs.c_cflag & PARENB) != 0;
    }

    if (bSetParityEnable)
    {
        if (bParityEnable)
        {
            gActualTTYAttrs.c_cflag |= PARENB;
        }
        else
        {
            gActualTTYAttrs.c_cflag &= ~(PARENB);
        }
        DebugInfo("   parity enable = %u", gActualTTYAttrs.c_cflag & PARENB);
        bSet = true;
    }

    // Parity odd or even

    if (pbOldParityEven != nullptr)
    {
        *pbOldParityEven = (gActualTTYAttrs.c_cflag & PARODD) != 0;
    }

    if (bSetParityEven)
    {
        if (!bParityEven)
        {
            gActualTTYAttrs.c_cflag |= PARODD;
        }
        else
        {
            gActualTTYAttrs.c_cflag &= ~(PARODD);
        }
        DebugInfo("   parity odd = %u", gActualTTYAttrs.c_cflag & PARODD);
        bSet = true;
    }

    // number of bits

    if (pOldnBits != nullptr)
    {
        switch(gActualTTYAttrs.c_cflag & CSIZE)
        {
            case CS5:
                *pOldnBits = 5;
                break;

            case CS6:
                *pOldnBits = 6;
                break;

            case CS7:
                *pOldnBits = 7;
                break;

            case CS8:
                *pOldnBits = 8;
                break;

            default:
                *pOldnBits = 0;
                break;
        }
    }
    if (bSetnBits)
    {
        gActualTTYAttrs.c_cflag &= ~CSIZE;
        switch(nBits)
        {
            case 5:
            gActualTTYAttrs.c_cflag |= CS5;
            DebugInfo("   5 data bits");
            break;

            case 6:
            gActualTTYAttrs.c_cflag |= CS6;
            DebugInfo("   6 data bits");
            break;

            case 7:
            gActualTTYAttrs.c_cflag |= CS7;
            DebugInfo("   7 data bits");
            break;

            case 8:
            default:
            gActualTTYAttrs.c_cflag |= CS8;
            DebugInfo("   8 data bits");
            break;
        }
        bSet = true;
    }

    // One or two stop bits

    if (pOldnStopBits != nullptr)
    {
        *pOldnStopBits = (gActualTTYAttrs.c_cflag & CSTOPB) ? (unsigned int) 2 : (unsigned int) 1;
    }

    if (bSetnStopBits)
    {
        if (nStopBits == 2)
        {
            gActualTTYAttrs.c_cflag |= CSTOPB;
            DebugInfo("   2 stop bits");
        }
        else
        {
            gActualTTYAttrs.c_cflag &= ~(CSTOPB);
            DebugInfo("   1 stop bit");
        }
        bSet = true;
    }

    if (bSet)
    {
        if (tcsetattr(m_fd, TCSANOW, &gActualTTYAttrs) == -1)
        {
            DebugError2("() : tcsetattr() -> %s", strerror(errno));
            return (uint32_t) -1;
        }
    }

    return 0;
}
