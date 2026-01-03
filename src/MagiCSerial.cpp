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
#include "Atari.h"
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
bool CMagiCSerial::m_bBIOSSerialUsed = false;
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
        return (uint32_t) -1;
    }

    if (fcntl(m_fd, F_SETFL, 0) == -1)
    {
        DebugError2("() : fcntl(clearing O_NDELAY) -> %s", strerror(errno));
        return (uint32_t) -2;
    }

    // Get the current options and save them for later reset
    if (tcgetattr(m_fd, &gOriginalTTYAttrs) == -1)
    {
        DebugError2("() : tcgetattr() -> %s", strerror(errno));
        return (uint32_t) -3;
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
    return (m_fd != -1);
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
    if (nBytesRead)
    {
        memcpy(pBuf, m_InBuffer, (size_t) nBytesRead);
        memmove(m_InBuffer, m_InBuffer + nBytesRead, (size_t) (m_InBufFill - nBytesRead));
#ifdef DEBUG_VERBOSE
        if (nBytesRead)
            DebugInfo2("() -- buflen = %d, %d characters read:", cnt, nBytesRead);

        for (int i = 0; i < nBytesRead; i++)
        {
            DebugInfo2("() -- <== c = 0x%02x (%c)", ((unsigned int) (pBuffer[i])) & 0xff, ((pBuffer[i] >= ' ') && (pBuffer[i] <= 'z')) ? pBuffer[i] : '?');
        }
#endif
        m_InBufFill -= nBytesRead;
        pBuf += nBytesRead;
        cnt -= nBytesRead;
    }

    if (cnt)
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
            DebugInfo2("() -- buflen = %d, %d characters received:", cnt, ret);

        for (register int i = 0; i < ret; i++)
        {
            DebugInfo2("() -- <== c = 0x%02x (%c)", ((unsigned int) (pBuffer[i])) & 0xff, ((pBuffer[i] >= ' ') && (pBuffer[i] <= 'z')) ? pBuffer[i] : '?');
        }
    }
#endif
    if (ret == -1)
    {
        DebugError2("() : read() -> %s", strerror(errno));
        return 0xffffffff;        // Atari error code
    }

    return (uint32_t) ret + nBytesRead;    // number of characters
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
    DebugInfo2("() -- send %d characters:", cnt);
    for (int i = 0; i < cnt; i++)
    {
        DebugInfo2("() -- ==> c = 0x%02x (%c)", (unsigned int) pBuffer[i], ((pBuffer[i] >= ' ') && (pBuffer[i] <= 'z')) ? pBuffer[i] : '?');
    }
#endif
    ret = write(m_fd, pBuf, cnt);
    if (ret == -1)
    {
        DebugError2("() : write() -> %s", strerror(errno));
        return 0xffffffff;        // Atari error code
    }

    return (uint32_t) ret;    // number of characters
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


    if (m_fd == -1)
        return false;

    MY_FD_ZERO(&fdset);
    MY_FD_SET(m_fd, &fdset);
    ret = CMachO::select(1, &fdset /*readfds*/, NULL/*writefds*/, NULL, &Timeout);
    return (ret == 1);    // number of ready fds
#else
    int ret;

    if (m_InBufFill)
    {
        return true;
    }

    ret = read(m_fd, m_InBuffer, SERIAL_IBUFLEN);
    if ((ret == -1) || (ret == 0))
    {
        return false;
    }

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
 * @param[in]  bInputBuffer     flush read buffer
 * @param[in]  bOutputBuffer    flush write buffer
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
    {
        return (uint32_t) -1;
    }

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


/** **********************************************************************************************
 *
 * @brief Open serial interface for BIOS access, if necessary
 *
 * @return zero or error code
 *
 * @note This BIOS callback is also called indirectly from GEMDOS, via the standard device "AUX:"".
 *       The modern alternative "u:\dev\SERIAL" is implemented in a loadable device driver
 *       and uses more effective functions
 * @note Called by
 *          AtariSerOut()
 *          AtariSerIn()
 *          AtariSerOs()
 *          AtariSerIs()
 *          AtariSerConf()
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::OpenSerialBIOS(void)
{
    // already opened => OK
    if (m_bBIOSSerialUsed)
    {
        return 0;
    }

    // already opened by GEMDOS => error
    if (CMagiCSerial::IsOpen())
    {
        DebugError2("() -- already opened by GEMDOS => error");
        return (uint32_t) ERROR;
    }

    if (-1 == (int) CMagiCSerial::Open())
    {
        DebugError2("() -- cannot open \"%s\"", Preferences::szAuxPath);
        return (uint32_t) ERROR;
    }

    m_bBIOSSerialUsed = true;
    DebugWarning2("() -- Serial device opened by BIOS, no way to ever close it.");
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback of XBIOS Serconf
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return zero or error code
 *
 * @note Called by the Atari BIOS function Rsconf() for the serial interface.
 *       Rsconf() is also called by the Atari GEMDOS for the devices AUX and AUXNB,
 *       in particular with this special parameters:
 *          Rsconf(-2,-2,-1,-1,-1,-1, 'iocl', dev, cmd, parm)
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerConf(uint32_t params, uint8_t *addrOffset68k)
{
    struct SerConfParm
    {
        uint16_t baud;
        uint16_t ctrl;
        uint16_t ucr;
        uint16_t rsr;
        uint16_t tsr;
        uint16_t scr;
        uint32_t xtend_magic;   // may be 'iocl'
        uint16_t biosdev;       // Ioctl BIOS device
        uint16_t cmd;           // Ioctl command
        uint32_t parm;          // Ioctl parameter
        uint32_t ptr2zero;      // dereference and set to non-zero!
    } __attribute__((packed));
    static const unsigned int nBitsTable[] =
    {
        8,7,6,5
    };
    static const unsigned int baudtable[] =
    {
        19200,
        9600,
        4800,
        3600,
        2400,
        2000,
        1800,
        1200,
        600,
        300,
        200,
        150,
        134,
        110,
        75,
        50
    };
    unsigned int nBits, nStopBits;


    DebugInfo2("()");

    // open serial interface, if necessary
    if (OpenSerialBIOS())
    {
        DebugError2("() -- cannot open serial interface");
        return (uint32_t) ERROR;
    }

    const SerConfParm *theSerConfParm = (SerConfParm *) (addrOffset68k + params);

    // Rsconf(-2,-2,-1,-1,-1,-1, 'iocl', dev, cmd, parm) is implemented as fcntl()

    if ((be16toh(theSerConfParm->baud) == 0xfffe) &&
         (be16toh(theSerConfParm->ctrl) == 0xfffe) &&
         (be16toh(theSerConfParm->ucr) == 0xffff) &&
         (be16toh(theSerConfParm->rsr) == 0xffff) &&
         (be16toh(theSerConfParm->tsr) == 0xffff) &&
         (be16toh(theSerConfParm->scr) == 0xffff) &&
         (be32toh(theSerConfParm->xtend_magic) == 0x696f636c)) /* 'iocl' */
    {
        uint32_t grp;
        uint32_t mode;
        uint32_t ret;
        bool bSet;
        uint32_t NewBaudrate, OldBaudrate;
        uint16_t flags;

        bool bXonXoff;
        bool bRtsCts;
        bool bParityEnable;
        bool bParityEven;
        unsigned int nBits;
        unsigned int nStopBits;


        DebugInfo2("() -- Fcntl(dev=%d, cmd=0x%04x, parm=0x%08x)", be16toh(theSerConfParm->biosdev), be16toh(theSerConfParm->cmd), be32toh(theSerConfParm->parm));
        *((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->ptr2zero))) = htobe32(0xffffffff);    // wir kennen Fcntl
        switch(be16toh(theSerConfParm->cmd))
        {
            case TIOCBUFFER:
                // Inquire/Set buffer settings
                DebugWarning2("() -- Fcntl(TIOCBUFFER) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCCTLMAP:
                // Inquire I/O-lines and signaling capabilities
                DebugWarning2("() -- Fcntl(TIOCCTLMAP) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCCTLGET:
                // Inquire I/O-lines and signals
                DebugWarning2("() -- Fcntl(TIOCCTLGET) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCCTLSET:
                // Set I/O-lines and signals
                DebugWarning2("() -- Fcntl(TIOCCTLSET) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCGPGRP:
                //get terminal process group
                DebugWarning2("() -- Fcntl(TIOCGPGRP) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCSPGRP:
                //set terminal process group
                grp = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->parm))));
                DebugInfo2("() -- Fcntl(TIOCSPGRP, %d)", (uint32_t) grp);
                (void) grp;     // TODO: really unused?
                ret = (uint32_t) EINVFN;
                break;

            case TIOCFLUSH:
                // Flush buffers of serial interface
                mode = be32toh(theSerConfParm->parm);
                DebugInfo2("() -- Fcntl(TIOCFLUSH, %d)", mode);
                switch(mode)
                {
                    // The whole transmit buffer shall be sent. The function does not return before
                    // the transmit buffer is empty (return E_OK, =0) or a system internal timeout
                    // has occurred (return EDRVNR, =-2).
                    // The timeout value is defined by the system in a sensible way.
                    case 0:
                        ret = CMagiCSerial::Drain();
                        break;

                    // clear receive buffer
                    case 1:
                        ret = CMagiCSerial::Flush(true, false);
                        break;

                    // clear tansmit buffer
                    case 2:
                        ret = CMagiCSerial::Flush(false, true);
                        break;

                    // clear receive and transmit buffers
                    case 3:
                        ret = CMagiCSerial::Flush(true, true);
                        break;

                    default:
                        ret = (uint32_t) EINVFN;
                        break;
                }
                break;

            case TIOCIBAUD:
            case TIOCOBAUD:
                // set receive speed
                NewBaudrate = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->parm))));
                bSet = ((int) NewBaudrate != -1) && (NewBaudrate != 0);
                DebugInfo2("() -- Fcntl(%s, %d)", (be16toh(theSerConfParm->cmd) == TIOCIBAUD) ? "TIOCIBAUD" : "TIOCOBAUD", NewBaudrate);

                if (be16toh(theSerConfParm->cmd) == TIOCIBAUD)
                    ret = CMagiCSerial::Config(
                        bSet,                       // conditionally change input rate
                        NewBaudrate,                // new input rate
                        &OldBaudrate,               // previous baudrate
                        false,                      // do not change output rate
                        0,                          // new output rate
                        nullptr,                    // old output rate is don't care
                        false,                      // change Xon/Xoff
                        false,
                        nullptr,
                        false,                      // change Rts/Cts
                        false,
                        nullptr,
                        false,                      // change parity enable
                        false,
                        nullptr,
                        false,                      // change parity even
                        false,
                        nullptr,
                        false,                      // change n bits
                        0,
                        nullptr,
                        false,                      // change stop bits
                        0,
                        nullptr);
                else
                    ret = CMagiCSerial::Config(
                        false,                      // do not change input rate
                        0,                          // new input rate
                        nullptr,                    // old input rate is don't care
                        bSet,                       // conditionally change output rate
                        NewBaudrate,                // new output rate
                        &OldBaudrate,               // previous output rate
                        false,                      // change Xon/Xoff
                        false,
                        nullptr,
                        false,                      // change Rts/Cts
                        false,
                        nullptr,
                        false,                      // change parity enable
                        false,
                        nullptr,
                        false,                      // change parity even
                        false,
                        nullptr,
                        false,                      // change n Bits
                        0,
                        nullptr,
                        false,                      // change stop bits
                        0,
                        nullptr);

                *((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->parm))) = htobe32(OldBaudrate);
                if ((int) ret == -1)
                {
                    ret = (uint32_t) ATARIERR_ERANGE;
                }
                break;

            case TIOCGFLAGS:
                // retrieve protocol settings

                DebugInfo2("() -- Fcntl(TIOCGFLAGS, %d)", be32toh(theSerConfParm->parm));
                (void) CMagiCSerial::Config(
                            false,                  // do not change input rate
                            0,                      // new input rate
                            nullptr,                // old input rate is don't care
                            false,                  // do not change output rate
                            0,                      // new output rate
                            nullptr,                // old output rate is don't care
                            false,                  // do not change Xon/Xoff
                            false,                  // new value
                            &bXonXoff,              // old value
                            false,                  // do not change Rts/Cts
                            false,
                            &bRtsCts,
                            false,                  // do not change parity enable
                            false,
                            &bParityEnable,
                            false,                  // do not change parity even
                            false,
                            &bParityEven,
                            false,                  // do not change n Bits
                            0,
                            &nBits,
                            false,                  // do not change Stopbits
                            0,
                            &nStopBits);

                // compose return value

                flags = 0;
                if (bXonXoff)
                    flags |= 0x1000;
                if (bRtsCts)
                    flags |= 0x2000;
                if (bParityEnable)
                    flags |= (bParityEven) ? 0x4000 : 0x8000;
                if (nStopBits == 1)
                    flags |= 1;
                else
                if (nStopBits == 2)
                    flags |= 3;
                if (nBits == 5)
                    flags |= 0xc;
                else
                if (nBits == 6)
                    flags |= 0x8;
                else
                if (nBits == 7)
                    flags |= 0x4;
                *((uint16_t *) (addrOffset68k + be32toh(theSerConfParm->parm))) = htobe16(flags);
                ret = (uint32_t) E_OK;
                break;

            case TIOCSFLAGS:
                // change protocol settings

                flags = be16toh(*((uint16_t *) (addrOffset68k + be32toh(theSerConfParm->parm))));
                DebugInfo2("() -- Fcntl(TIOCSFLAGS, 0x%04x)", (uint32_t) flags);
                bXonXoff = (flags & 0x1000) != 0;
                DebugInfo2("() -- XON/XOFF %s", (bXonXoff) ? "ON" : "OFF");
                bRtsCts = (flags & 0x2000) != 0;
                DebugInfo2("() -- RTS/CTS %s", (bRtsCts) ? "ON" : "OFF");
                bParityEnable = (flags & (0x4000+0x8000)) != 0;
                DebugInfo2("() -- parity %s", (bParityEnable) ? "ON" : "OFF");
                bParityEven= (flags & 0x4000) != 0;
                DebugInfo2("() -- parity %s", (bParityEven) ? "(even)" : "(odd)");
                nBits = 8U - ((flags & 0xc) >> 2);
                DebugInfo2("() -- %d Bits", nBits);
                nStopBits = flags & 3U;
                DebugInfo2("() -- %d stop  bits%s", nStopBits, (nStopBits == 0) ? " (synchronous mode?)" : "");
                if ((nStopBits == 0) || (nStopBits == 2))
                    return (uint32_t) ATARIERR_ERANGE;

                if (nStopBits == 3)
                    nStopBits = 2;

                ret = CMagiCSerial::Config(
                            false,                  // do not change input rate
                            0,                      // new input rate
                            nullptr,                // old input rate is don't care
                            false,                  // do not change output rate
                            0,                      // new output rate
                            nullptr,                // old output rate is don't care
                            true,                   // change Xon/Xoff
                            bXonXoff,               // new value
                            nullptr,                // old value is don't care
                            true,                   // change Rts/Cts
                            bRtsCts,
                            nullptr,
                            true,                   // change parity enable
                            bParityEnable,
                            nullptr,
                            false,                  // do not change parity even
                            bParityEven,
                            nullptr,
                            true,                   // change n Bits
                            nBits,
                            nullptr,
                            true,                   // change Stopbits
                            nStopBits,
                            nullptr);
                if ((int) ret == -1)
                    ret = (uint32_t) ATARIERR_ERANGE;
                break;

            default:
                DebugError2("() -- Fcntl(0x%04x) is unknown", be16toh(theSerConfParm->cmd) & 0xffff);
                ret = (uint32_t) EINVFN;
                break;
        }
        return ret;
    }

    // Rsconf(-2,-1,-1,-1,-1,-1) returns current baudrate

    if ((be16toh(theSerConfParm->baud) == 0xfffe) &&
         (be16toh(theSerConfParm->ctrl) == 0xffff) &&
         (be16toh(theSerConfParm->ucr) == 0xffff) &&
         (be16toh(theSerConfParm->rsr) == 0xffff) &&
         (be16toh(theSerConfParm->tsr) == 0xffff) &&
         (be16toh(theSerConfParm->scr) == 0xffff))
    {
//        unsigned long OldInputBaudrate;
//        return (uint32_t) pTheSerial->GetBaudRate();
    }

    if (be16toh(theSerConfParm->baud) >= sizeof(baudtable)/sizeof(baudtable[0]))
    {
        DebugError2("() -- invalid baudrate passed to Rsconf()");
        return (uint32_t) ATARIERR_ERANGE;
    }

    nBits = nBitsTable[(be16toh(theSerConfParm->ucr) >> 5) & 3];
    nStopBits = (unsigned int) (((be16toh(theSerConfParm->ucr) >> 3) == 3) ? 2 : 1);

    return CMagiCSerial::Config(
                    true,                           // change input rate
                    baudtable[be16toh(theSerConfParm->baud)],    // new input rate
                    nullptr,                        // old input rate is don't care
                    true,                           // change output rate
                    baudtable[be16toh(theSerConfParm->baud)],    // new output rate
                    nullptr,                        // old output rate is don't care
                    true,                           // change Xon/Xoff
                    (be16toh(theSerConfParm->ctrl) & 1) != 0,    // new value
                    nullptr,                        // old value is don't care
                    true,                           // change Rts/Cts
                    (be16toh(theSerConfParm->ctrl) & 2) != 0,
                    nullptr,
                    true,                           // change parity enable
                    (be16toh(theSerConfParm->ucr) & 4) != 0,
                    nullptr,
                    true,                           // change parity even
                    (be16toh(theSerConfParm->ucr) & 2) != 0,
                    nullptr,
                    true,                           // change n bits
                    nBits,
                    nullptr,
                    true,                           // change stop bits
                    nStopBits,
                    nullptr);
}


/** **********************************************************************************************
 *
 * @brief Emulator callback for read status of serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return status
 * @retval -1  ready
 * @retval 0   not ready
 *
 * @note Called by the Atari BIOS function Bconstat() for the serial interface.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerIs(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
//    DebugInfo("CMagiC::AtariSerIs()");

    // open serial port, if necessary
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    return CMagiCSerial::ReadStatus() ? 0xffffffff : 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback for write status of serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return status
 * @retval -1  ready
 * @retval 0   not ready
 *
 * @note Called by the Atari BIOS function Bcostat() for the serial interface.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerOs(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
//    DebugInfo("CMagiC::AtariSerOs()");

    // open serial port, if necessary
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    return CMagiCSerial::WriteStatus() ? 0xffffffff : 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback for reading a character from serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return character in bits 0..7, other bits shall be zero
 * @retval 0xffffffff   error
 *
 * @note Called by the Atari BIOS function Bconin() for the serial interface.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerIn(uint32_t params, uint8_t *addrOffset68k)
{
    char c;
    uint32_t ret;
    (void) params;
    (void) addrOffset68k;

//    DebugInfo("CMagiC::AtariSerIn()");

    // open serial port, if necessary
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    ret = CMagiCSerial::Read(&c, 1);
    if (ret > 0)
    {
        return (uint32_t) c & 0x000000ff;
    }
    else
        return 0xffffffff;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback for writing a character to serial interface
 *
 * @param[in]  params           68k address of character (16-bits, big-endian) to write
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return status or negative error code
 * @retval 0   nothing written
 * @retval 1   OK
 *
 * @note Called by the Atari BIOS function Bconout() for the serial interface.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerOut(uint32_t params, uint8_t *addrOffset68k)
{
//    DebugInfo("CMagiC::AtariSerOut()");

    // open serial port, if necessary
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    return CMagiCSerial::Write((char *) addrOffset68k + params + 1, 1);
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Open serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return zero or negative error code
 *
 * @note Used by the host's SERIAL driver (DEV_SER.DEV), not by the MagiC kernel itself.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerOpen(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    DebugInfo2("()");

    // already opened by BIOS => OK
    if (m_bBIOSSerialUsed)
    {
        DebugInfo2("() -- already opened by BIOS => OK");
        return 0;
    }

    // already opened by GEMDOS => error
    if (CMagiCSerial::IsOpen())
    {
        DebugInfo2("() -- already opend by GEMDOS => error");
        return (uint32_t) EACCDN;
    }

    if (-1 == (int) CMagiCSerial::Open())
    {
        DebugInfo2("() -- cannot open \"%s\"", Preferences::szAuxPath);
        return (uint32_t) ERROR;
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Close serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return zero or negative error code
 *
 * @note Used by the host's SERIAL driver (DEV_SER.DEV), not by the MagiC kernel itself.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerClose(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    DebugInfo2("()");

    // schon durch BIOS geöffnet => OK
    if (m_bBIOSSerialUsed)
        return 0;

    // nicht vom DOS geöffnet => Fehler
    if (!CMagiCSerial::IsOpen())
    {
        return (uint32_t) EACCDN;
    }

    if (CMagiCSerial::Close())
    {
        return (uint32_t) ERROR;
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Read characters from serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return number of characters read
 *
 * @note Used by the host's SERIAL driver (DEV_SER.DEV), not by the MagiC kernel itself.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerRead(uint32_t params, uint8_t *addrOffset68k)
{
    struct SerReadParm
    {
        uint32_t buf;
        uint32_t len;
    } __attribute__((packed));
    uint32_t ret;

    SerReadParm *theSerReadParm = (SerReadParm *) (addrOffset68k + params);
//    DebugInfo("CMagiC::AtariSerRead(buflen = %d)", theSerReadParm->len);

    ret = CMagiCSerial::Read((char *) (addrOffset68k +  be32toh(theSerReadParm->buf)), be32toh(theSerReadParm->len));
    return ret;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Write characters to serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return number of characters written
 *
 * @note Used by the host's SERIAL driver (DEV_SER.DEV), not by the MagiC kernel itself.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerWrite(uint32_t params, uint8_t *addrOffset68k)
{
    struct SerWriteParm
    {
        uint32_t buf;
        uint32_t len;
    } __attribute__((packed));
    uint32_t ret;

    SerWriteParm *theSerWriteParm = (SerWriteParm *) (addrOffset68k + params);
//    DebugInfo("CMagiC::AtariSerWrite(buflen = %d)", theSerWriteParm->len);

    ret = CMagiCSerial::Write((char *) (addrOffset68k +  be32toh(theSerWriteParm->buf)), be32toh(theSerWriteParm->len));
    return ret;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Get status of serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return status
 * @retval 0            cannot write
 * @retval 0xffffffff   can write
 *
 * @note Used by the host's SERIAL driver (DEV_SER.DEV), not by the MagiC kernel itself.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerStat(uint32_t params, uint8_t *addrOffset68k)
{
    struct SerStatParm
    {
        uint16_t rwflag;
    } __attribute__((packed));

//    DebugInfo("CMagiC::AtariSerWrite()");
    SerStatParm *theSerStatParm = (SerStatParm *) (addrOffset68k + params);

    return (be16toh(theSerStatParm->rwflag)) ?
                (CMagiCSerial::WriteStatus() ? 0xffffffff : 0) :
                (CMagiCSerial::ReadStatus() ? 0xffffffff : 0);
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Ioctl for serial interface
 *
 * @param[in]  params           68k address of parameter block
 * @param[in]  addrOffset68k    host address of 68k memory
 *
 * @return zero or error code
 *
 * @note Used by the host's SERIAL driver (DEV_SER.DEV), not by the MagiC kernel itself.
 *
 ************************************************************************************************/
uint32_t CMagiCSerial::AtariSerIoctl(uint32_t params, uint8_t *addrOffset68k)
{
    struct SerIoctlParm
    {
        uint16_t cmd;
        uint32_t parm;
    } __attribute__((packed));

//    DebugInfo("CMagiC::AtariSerWrite()");
    SerIoctlParm *theSerIoctlParm = (SerIoctlParm *) (addrOffset68k + params);

    uint32_t grp;
    uint32_t mode;
    uint32_t ret;
    bool bSet;
    uint32_t NewBaudrate, OldBaudrate;
    uint16_t flags;

    bool bXonXoff;
    bool bRtsCts;
    bool bParityEnable;
    bool bParityEven;
    unsigned int nBits;
    unsigned int nStopBits;


    DebugInfo2("() -- Fcntl(cmd=0x%04x, parm=0x%08x)", be16toh(theSerIoctlParm->cmd), be32toh(theSerIoctlParm->parm));
    switch(be16toh(theSerIoctlParm->cmd))
    {
        case TIOCBUFFER:
            // Inquire/Set buffer settings
            DebugWarning2("() -- Fcntl(TIOCBUFFER) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCCTLMAP:
            // Inquire I/O-lines and signaling capabilities
            DebugWarning2("() -- Fcntl(TIOCCTLMAP) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCCTLGET:
            // Inquire I/O-lines and signals
            DebugWarning2("() -- Fcntl(TIOCCTLGET) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCCTLSET:
            // Set I/O-lines and signals
            DebugWarning2("() -- Fcntl(TIOCCTLSET) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCGPGRP:
            //get terminal process group
            DebugWarning2("() -- Fcntl(TIOCGPGRP) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCSPGRP:
            //set terminal process group
            grp = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))));
            DebugInfo2("() -- Fcntl(TIOCSPGRP, %d)", (uint32_t) grp);
            (void) grp;     // TODO: really unused?
            ret = (uint32_t) EINVFN;
            break;

        case TIOCFLUSH:
            // Leeren der seriellen Puffer
            mode = be32toh(theSerIoctlParm->parm);
            DebugInfo2("() -- Fcntl(TIOCFLUSH, %d)", mode);
            switch(mode)
            {
                // The whole transmit buffer shall be sent. The function does not return before
                // the transmit buffer is empty (return E_OK, =0) or a system internal timeout
                // has occurred (return EDRVNR, =-2).
                // The timeout value is defined by the system in a sensible way.
                case 0:
                    ret = CMagiCSerial::Drain();
                    break;

                // clear receive buffer
                case 1:
                    ret = CMagiCSerial::Flush(true, false);
                    break;

                // clear transmit buffer
                case 2:
                    ret = CMagiCSerial::Flush(false, true);
                    break;

                // clear receive and transmit buffer
                case 3:
                    ret = CMagiCSerial::Flush(true, true);
                    break;

                default:
                    ret = (uint32_t) EINVFN;
                    break;
            }
            break;

        case TIOCIBAUD:
        case TIOCOBAUD:
            // configure receive speed

            NewBaudrate = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))));
            bSet = ((int) NewBaudrate != -1) && (NewBaudrate != 0);
            DebugInfo2("() -- Fcntl(%s, %d)", (be16toh(theSerIoctlParm->cmd) == TIOCIBAUD) ? "TIOCIBAUD" : "TIOCOBAUD", NewBaudrate);

            if (be16toh(theSerIoctlParm->cmd) == TIOCIBAUD)
                ret = CMagiCSerial::Config(
                    bSet,                               // conditionally change input rate
                    NewBaudrate,                        // new input rate
                    &OldBaudrate,                       // old baudrate
                    false,                              // do not change output rate
                    0,                                  // new output rate
                    nullptr,                            // old output rate is don't care
                    false,                              // change Xon/Xoff
                    false,
                    NULL,
                    false,                              // change Rts/Cts
                    false,
                    nullptr,
                    false,                              // change parity enable
                    false,
                    nullptr,
                    false,                              // change parity even
                    false,
                    nullptr,
                    false,                              // change n bits
                    0,
                    nullptr,
                    false,                              // change stop bits
                    0,
                    nullptr);
            else
                ret = CMagiCSerial::Config(
                    false,                              // do not change input rate
                    0,                                  // new input rate
                    nullptr,                            // old input rate is don't care
                    bSet,                               // conditionally change output rate
                    NewBaudrate,                        // new output rate
                    &OldBaudrate,                       // old output rate
                    false,                              // change Xon/Xoff
                    false,
                    nullptr,
                    false,                              // change Rts/Cts
                    false,
                    nullptr,
                    false,                              // change parity enable
                    false,
                    nullptr,
                    false,                              // change parity even
                    false,
                    nullptr,
                    false,                              // change n bits
                    0,
                    nullptr,
                    false,                              // change stop bits
                    0,
                    nullptr);

            *((uint32_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))) = htobe32(OldBaudrate);
            if ((int) ret == -1)
                ret = (uint32_t) ATARIERR_ERANGE;
            break;

        case TIOCGFLAGS:
            // retrieve protocol settings

            DebugInfo2("() -- Fcntl(TIOCGFLAGS, %d)", be32toh(theSerIoctlParm->parm));
            (void) CMagiCSerial::Config(
                        false,                          // do not change input rate
                        0,                              // new input rate
                        nullptr,                        // old input rate is don't care
                        false,                          // do not change output rate
                        0,                              // new output rate
                        nullptr,                        // old output rate is don't care
                        false,                          // do not change Xon/Xoff
                        false,                          // new value
                        &bXonXoff,                      // old value
                        false,                          // do not change Rts/Cts
                        false,
                        &bRtsCts,
                        false,                          // do not change parity enable
                        false,
                        &bParityEnable,
                        false,                          // do not change parity even
                        false,
                        &bParityEven,
                        false,                          // do not change n bits
                        0,
                        &nBits,
                        false,                          // do not change stop bits
                        0,
                        &nStopBits);
            // compose return value
            flags = 0;
            if (bXonXoff)
                flags |= 0x1000;
            if (bRtsCts)
                flags |= 0x2000;
            if (bParityEnable)
                flags |= (bParityEven) ? 0x4000 : 0x8000;
            if (nStopBits == 1)
                flags |= 1;
            else
            if (nStopBits == 2)
                flags |= 3;
            if (nBits == 5)
                flags |= 0xc;
            else
            if (nBits == 6)
                flags |= 0x8;
            else
            if (nBits == 7)
                flags |= 0x4;
            *((uint16_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))) = htobe16(flags);
            ret = (uint32_t) E_OK;
            break;

        case TIOCSFLAGS:
            // configure protocol parameters

            flags = be16toh(*((uint16_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))));
            DebugInfo2("() -- Fcntl(TIOCSFLAGS, 0x%04x)", (uint32_t) flags);
            bXonXoff = (flags & 0x1000) != 0;
            DebugInfo2("() -- XON/XOFF %s", (bXonXoff) ? "ON" : "OFF");
            bRtsCts = (flags & 0x2000) != 0;
            DebugInfo2("() -- RTS/CTS %s", (bRtsCts) ? "ON" : "OFF");
            bParityEnable = (flags & (0x4000+0x8000)) != 0;
            DebugInfo2("() -- parity %s", (bParityEnable) ? "ON" : "OFF");
            bParityEven= (flags & 0x4000) != 0;
            DebugInfo2("() -- parity %s", (bParityEven) ? "(even)" : "(odd)");
            nBits = 8U - ((flags & 0xc) >> 2);
            DebugInfo2("() -- %d bits", nBits);
            nStopBits = flags & 3U;
            DebugInfo2("() -- %d stop bits%s", nStopBits, (nStopBits == 0) ? " (synchronous mode?)" : "");
            if ((nStopBits == 0) || (nStopBits == 2))
            {
                return (uint32_t) ATARIERR_ERANGE;
            }

            if (nStopBits == 3)
                nStopBits = 2;
            ret = CMagiCSerial::Config(
                        false,                          // do not change input rate
                        0,                              // new input rate
                        nullptr,                        // old input rate is don't care
                        false,                          // do not change output rate
                        0,                              // new output rate
                        nullptr,                        // old output rate is don't care
                        true,                           // change Xon/Xoff
                        bXonXoff,                       // new value
                        nullptr,                        // old value is don't care
                        true,                           // change Rts/Cts
                        bRtsCts,
                        nullptr,
                        true,                           // change parity enable
                        bParityEnable,
                        nullptr,
                        false,                          // do not change parity even
                        bParityEven,
                        nullptr,
                        true,                           // change n bits
                        nBits,
                        nullptr,
                        true,                           // change stop bits
                        nStopBits,
                        nullptr);
            if ((int) ret == -1)
                ret = (uint32_t) ATARIERR_ERANGE;
            break;

        default:
            DebugError2("() -- Fcntl(0x%04x) is unknown", be16toh(theSerIoctlParm->cmd) & 0xffff);
            ret = (uint32_t) EINVFN;
            break;
    }

    return ret;
}
