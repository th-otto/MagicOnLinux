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


/**********************************************************************
*
* Serielle Schnittstelle für BIOS-Zugriff öffnen, wenn nötig.
* Der BIOS-Aufruf erfolgt auch indirekt über das Atari-GEMDOS,
* wenn die Standard-Schnittstelle AUX: angesprochen wird.
* Die moderne Geräteschnittstelle u:\dev\SERIAL wird durch einen
* nachladbaren Gerätetreiber respräsentiert und verwendet
* andere - effizientere - Routinen.
*
* Wird aufgerufen von:
*    AtariSerOut()
*    AtariSerIn()
*    AtariSerOs()
*    AtariSerIs()
*    AtariSerConf()
*
**********************************************************************/

uint32_t CMagiCSerial::OpenSerialBIOS(void)
{
    // schon geöffnet => OK
    if (m_bBIOSSerialUsed)
    {
        return 0;
    }

    // schon vom DOS geöffnet => Fehler
    if (CMagiCSerial::IsOpen())
    {
        DebugError("CMagiC::OpenSerialBIOS() -- schon vom DOS geöffnet => Fehler");
        return((uint32_t) ERROR);
    }

    if (-1 == (int) CMagiCSerial::Open())
    {
        DebugInfo("CMagiC::OpenSerialBIOS() -- kann \"%s\" nicht öffnen.", Preferences::szAuxPath);
        return((uint32_t) ERROR);
    }

    m_bBIOSSerialUsed = true;
    DebugWarning("CMagiC::OpenSerialBIOS() -- Serielle Schnittstelle vom BIOS geöffnet.");
    DebugWarning("   Jetzt kann sie nicht mehr geschlossen werden!");
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: XBIOS Serconf
*
* Wird aufgerufen von der Atari-BIOS-Funktion Rsconf() für
* die serielle Schnittstelle. Rsconf() wird auch vom
* Atari-GEMDOS aufgerufen für die Geräte AUX und AUXNB,
* und zwar hier mit Spezialwerten:
*    Rsconf(-2,-2,-1,-1,-1,-1, 'iocl', dev, cmd, parm)
*
**********************************************************************/

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
        uint32_t xtend_magic;    // ist ggf. 'iocl'
        uint16_t biosdev;        // Ioctl-Bios-Gerät
        uint16_t cmd;            // Ioctl-Kommando
        uint32_t parm;        // Ioctl-Parameter
        uint32_t ptr2zero;        // deref. und auf ungleich 0 setzen!
    } __attribute__((packed));
    unsigned int nBitsTable[] =
    {
        8,7,6,5
    };
    unsigned int baudtable[] =
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

    // serielle Schnittstelle öffnen, wenn nötig
    if (OpenSerialBIOS())
    {
        DebugInfo2("() -- kann serielle Schnittstelle nicht öffnen => Fehler.");
        return((uint32_t) ERROR);
    }

    SerConfParm *theSerConfParm = (SerConfParm *) (addrOffset68k + params);

    // Rsconf(-2,-2,-1,-1,-1,-1, 'iocl', dev, cmd, parm) macht Fcntl

    if ((be16toh(theSerConfParm->baud) == 0xfffe) &&
         (be16toh(theSerConfParm->ctrl) == 0xfffe) &&
         (be16toh(theSerConfParm->ucr) == 0xffff) &&
         (be16toh(theSerConfParm->rsr) == 0xffff) &&
         (be16toh(theSerConfParm->tsr) == 0xffff) &&
         (be16toh(theSerConfParm->scr) == 0xffff) &&
         (be32toh(theSerConfParm->xtend_magic) == 'iocl'))
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


        DebugInfo("CMagiC::AtariSerConf() -- Fcntl(dev=%d, cmd=0x%04x, parm=0x%08x)", be16toh(theSerConfParm->biosdev), be16toh(theSerConfParm->cmd), be32toh(theSerConfParm->parm));
        *((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->ptr2zero))) = htobe32(0xffffffff);    // wir kennen Fcntl
        switch(be16toh(theSerConfParm->cmd))
        {
            case TIOCBUFFER:
                // Inquire/Set buffer settings
                DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCBUFFER) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCCTLMAP:
                // Inquire I/O-lines and signaling capabilities
                DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCCTLMAP) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCCTLGET:
                // Inquire I/O-lines and signals
                DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCCTLGET) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCCTLSET:
                // Set I/O-lines and signals
                DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCCTLSET) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCGPGRP:
                //get terminal process group
                DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCGPGRP) -- not supported");
                ret = (uint32_t) EINVFN;
                break;

            case TIOCSPGRP:
                //set terminal process group
                grp = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->parm))));
                DebugInfo("CMagiC::AtariSerConf() -- Fcntl(TIOCSPGRP, %d)", (uint32_t) grp);
                ret = (uint32_t) EINVFN;
                break;

            case TIOCFLUSH:
                // Leeren der seriellen Puffer
                mode = be32toh(theSerConfParm->parm);
                DebugInfo("CMagiC::AtariSerConf() -- Fcntl(TIOCFLUSH, %d)", mode);
                switch(mode)
                {
                    // Der Sendepuffer soll komplett gesendet werden. Die Funktion kehrt
                    // erst zurück, wenn der Puffer leer ist (return E_OK, =0) oder ein
                    // systeminterner Timeout abgelaufen ist (return EDRVNR, =-2). Der
                    // Timeout wird vom System sinnvoll bestimmt.
                    case 0:
                        ret = CMagiCSerial::Drain();
                        break;

                    // Der Empfangspuffer wird gelöscht.
                    case 1:
                        ret = CMagiCSerial::Flush(true, false);
                        break;

                    // Der Sendepuffer wird gelöscht.
                    case 2:
                        ret = CMagiCSerial::Flush(false, true);
                        break;

                    // Empfangspuffer und Sendepuffer werden gelîscht.
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
                // Eingabegeschwindigkeit festlegen
                NewBaudrate = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->parm))));
                bSet = ((int) NewBaudrate != -1) && (NewBaudrate != 0);
                DebugInfo("CMagiC::AtariSerConf() -- Fcntl(%s, %d)", (be16toh(theSerConfParm->cmd) == TIOCIBAUD) ? "TIOCIBAUD" : "TIOCOBAUD", NewBaudrate);

                if (be16toh(theSerConfParm->cmd) == TIOCIBAUD)
                    ret = CMagiCSerial::Config(
                        bSet,                        // Input-Rate ggf. ändern
                        NewBaudrate,                // neue Input-Rate
                        &OldBaudrate,                // alte Baud-Rate
                        false,                        // Output-Rate nicht ändern
                        0,                        // neue Output-Rate
                        NULL,                        // alte Output-Rate egal
                        false,                        // Xon/Xoff ändern
                        false,
                        NULL,
                        false,                        // Rts/Cts ändern
                        false,
                        NULL,
                        false,                        // parity enable ändern
                        false,
                        NULL,
                        false,                        // parity even ändern
                        false,
                        NULL,
                        false,                        // n Bits ändern
                        0,
                        NULL,
                        false,                        // Stopbits ändern
                        0,
                        NULL);
                else
                    ret = CMagiCSerial::Config(
                        false,                        // Input-Rate nicht ändern
                        0,                        // neue Input-Rate
                        NULL,                        // alte Input-Rate egal
                        bSet,                        // Output-Rate ggf. ändern
                        NewBaudrate,                // neue Output-Rate
                        &OldBaudrate,                // alte Output-Rate
                        false,                        // Xon/Xoff ändern
                        false,
                        NULL,
                        false,                        // Rts/Cts ändern
                        false,
                        NULL,
                        false,                        // parity enable ändern
                        false,
                        NULL,
                        false,                        // parity even ändern
                        false,
                        NULL,
                        false,                        // n Bits ändern
                        0,
                        NULL,
                        false,                        // Stopbits ändern
                        0,
                        NULL);

                *((uint32_t *) (addrOffset68k + be32toh(theSerConfParm->parm))) = htobe32(OldBaudrate);
                if ((int) ret == -1)
                    ret = (uint32_t) ATARIERR_ERANGE;
                break;

            case TIOCGFLAGS:
                // Übertragungsprotokolleinstellungen erfragen

                DebugInfo("CMagiC::AtariSerConf() -- Fcntl(TIOCGFLAGS, %d)", be32toh(theSerConfParm->parm));
                (void) CMagiCSerial::Config(
                            false,                        // Input-Rate nicht ändern
                            0,                        // neue Input-Rate
                            NULL,                        // alte Input-Rate egal
                            false,                        // Output-Rate nicht ändern
                            0,                        // neue Output-Rate
                            NULL,                        // alte Output-Rate egal
                            false,                        // Xon/Xoff nicht ändern
                            false,                        // neuer Wert
                            &bXonXoff,                    // alter Wert
                            false,                        // Rts/Cts nicht ändern
                            false,
                            &bRtsCts,
                            false,                        // parity enable nicht ändern
                            false,
                            &bParityEnable,
                            false,                        // parity even nicht ändern
                            false,
                            &bParityEven,
                            false,                        // n Bits nicht ändern
                            0,
                            &nBits,
                            false,                        // Stopbits nicht ändern
                            0,
                            &nStopBits);
                // Rückgabewert zusammenbauen
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
                // Übertragungsprotokolleinstellungen setzen
                flags = be16toh(*((uint16_t *) (addrOffset68k + be32toh(theSerConfParm->parm))));
                DebugInfo("CMagiC::AtariSerConf() -- Fcntl(TIOCSFLAGS, 0x%04x)", (uint32_t) flags);
                bXonXoff = (flags & 0x1000) != 0;
                DebugInfo("CMagiC::AtariSerConf() -- XON/XOFF %s", (bXonXoff) ? "ein" : "aus");
                bRtsCts = (flags & 0x2000) != 0;
                DebugInfo("CMagiC::AtariSerConf() -- RTS/CTS %s", (bRtsCts) ? "ein" : "aus");
                bParityEnable = (flags & (0x4000+0x8000)) != 0;
                DebugInfo("CMagiC::AtariSerConf() -- Parität %s", (bParityEnable) ? "ein" : "aus");
                bParityEven= (flags & 0x4000) != 0;
                DebugInfo("CMagiC::AtariSerConf() -- Parität %s", (bParityEven) ? "gerade (even)" : "ungerade (odd)");
                nBits = 8U - ((flags & 0xc) >> 2);
                DebugInfo("CMagiC::AtariSerConf() -- %d Bits", nBits);
                nStopBits = flags & 3U;
                DebugInfo("CMagiC::AtariSerConf() -- %d Stop-Bits%s", nStopBits, (nStopBits == 0) ? " (Synchron-Modus?)" : "");
                if ((nStopBits == 0) || (nStopBits == 2))
                    return((uint32_t) ATARIERR_ERANGE);
                if (nStopBits == 3)
                    nStopBits = 2;
                ret = CMagiCSerial::Config(
                            false,                        // Input-Rate nicht ändern
                            0,                        // neue Input-Rate
                            NULL,                        // alte Input-Rate egal
                            false,                        // Output-Rate nicht ändern
                            0,                        // neue Output-Rate
                            NULL,                        // alte Output-Rate egal
                            true,                        // Xon/Xoff ändern
                            bXonXoff,                    // neuer Wert
                            NULL,                        // alter Wert egal
                            true,                        // Rts/Cts ändern
                            bRtsCts,
                            NULL,
                            true,                        // parity enable ändern
                            bParityEnable,
                            NULL,
                            false,                        // parity even nicht ändern
                            bParityEven,
                            NULL,
                            true,                        // n Bits ändern
                            nBits,
                            NULL,
                            true,                        // Stopbits ändern
                            nStopBits,
                            NULL);
                if ((int) ret == -1)
                    ret = (uint32_t) ATARIERR_ERANGE;
                break;

            default:
                DebugError("CMagiC::AtariSerConf() -- Fcntl(0x%04x -- unbekannt", be16toh(theSerConfParm->cmd) & 0xffff);
                ret = (uint32_t) EINVFN;
                break;
        }
        return ret;
    }

    // Rsconf(-2,-1,-1,-1,-1,-1) gibt aktuelle Baudrate zurück

    if ((be16toh(theSerConfParm->baud) == 0xfffe) &&
         (be16toh(theSerConfParm->ctrl) == 0xffff) &&
         (be16toh(theSerConfParm->ucr) == 0xffff) &&
         (be16toh(theSerConfParm->rsr) == 0xffff) &&
         (be16toh(theSerConfParm->tsr) == 0xffff) &&
         (be16toh(theSerConfParm->scr) == 0xffff))
    {
//        unsigned long OldInputBaudrate;
//        return((uint32_t) pTheSerial->GetBaudRate());
    }

    if (be16toh(theSerConfParm->baud) >= sizeof(baudtable)/sizeof(baudtable[0]))
    {
        DebugError2("() -- ungültige Baudrate von Rsconf()");
        return((uint32_t) ATARIERR_ERANGE);
    }

    nBits = nBitsTable[(be16toh(theSerConfParm->ucr) >> 5) & 3];
    nStopBits = (unsigned int) (((be16toh(theSerConfParm->ucr) >> 3) == 3) ? 2 : 1);

    return CMagiCSerial::Config(
                    true,                        // Input-Rate ändern
                    baudtable[be16toh(theSerConfParm->baud)],    // neue Input-Rate
                    NULL,                        // alte Input-Rate egal
                    true,                        // Output-Rate ändern
                    baudtable[be16toh(theSerConfParm->baud)],    // neue Output-Rate
                    NULL,                        // alte Output-Rate egal
                    true,                        // Xon/Xoff ändern
                    (be16toh(theSerConfParm->ctrl) & 1) != 0,    // neuer Wert
                    NULL,                        // alter Wert egal
                    true,                        // Rts/Cts ändern
                    (be16toh(theSerConfParm->ctrl) & 2) != 0,
                    NULL,
                    true,                        // parity enable ändern
                    (be16toh(theSerConfParm->ucr) & 4) != 0,
                    NULL,
                    true,                        // parity even ändern
                    (be16toh(theSerConfParm->ucr) & 2) != 0,
                    NULL,
                    true,                        // n Bits ändern
                    nBits,
                    NULL,
                    true,                        // Stopbits ändern
                    nStopBits,
                    NULL);
}

/**********************************************************************
*
* Callback des Emulators: Lesestatus der seriellen Schnittstelle
* Rückgabe: -1 = bereit 0 = nicht bereit
*
* Wird aufgerufen von der Atari-BIOS-Funktion Bconstat() für
* die serielle Schnittstelle.
*
**********************************************************************/

uint32_t CMagiCSerial::AtariSerIs(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
//    DebugInfo("CMagiC::AtariSerIs()");

    // serielle Schnittstelle öffnen, wenn nötig
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    return CMagiCSerial::ReadStatus() ? 0xffffffff : 0;
}


/**********************************************************************
*
* Callback des Emulators: Ausgabestatus der seriellen Schnittstelle
* Rückgabe: -1 = bereit 0 = nicht bereit
*
* Wird aufgerufen von der Atari-BIOS-Funktion Bcostat() für
* die serielle Schnittstelle.
*
**********************************************************************/

uint32_t CMagiCSerial::AtariSerOs(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
//    DebugInfo("CMagiC::AtariSerOs()");

    // serielle Schnittstelle öffnen, wenn nötig
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    return CMagiCSerial::WriteStatus() ? 0xffffffff : 0;
}


/**********************************************************************
*
* Callback des Emulators: Zeichen von serieller Schnittstelle lesen
* Rückgabe: Zeichen in Bit 0..7, andere Bits = 0, 0xffffffff bei Fehler
*
* Wird aufgerufen von der Atari-BIOS-Funktion Bconin() für
* die serielle Schnittstelle.
*
**********************************************************************/

uint32_t CMagiCSerial::AtariSerIn(uint32_t params, uint8_t *addrOffset68k)
{
    char c;
    uint32_t ret;
    (void) params;
    (void) addrOffset68k;

//    DebugInfo("CMagiC::AtariSerIn()");

    // serielle Schnittstelle öffnen, wenn nötig
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    ret = CMagiCSerial::Read(&c, 1);
    if (ret > 0)
    {
        return((uint32_t) c & 0x000000ff);
    }
    else
        return(0xffffffff);
}


/**********************************************************************
*
* Callback des Emulators: Zeichen auf serielle Schnittstelle ausgeben
* params        Zeiger auf auszugebendes Zeichen (16 Bit)
* Rückgabe: 1 = OK, 0 = nichts geschrieben, sonst Fehlercode
*
* Wird aufgerufen von der Atari-BIOS-Funktion Bconout() für
* die serielle Schnittstelle.
*
**********************************************************************/

uint32_t CMagiCSerial::AtariSerOut(uint32_t params, uint8_t *addrOffset68k)
{
//    DebugInfo("CMagiC::AtariSerOut()");

    // serielle Schnittstelle öffnen, wenn nötig
    if (!OpenSerialBIOS())
    {
        return 0;
    }

    return CMagiCSerial::Write((char *) addrOffset68k + params + 1, 1);
}


/**********************************************************************
*
* Callback des Emulators: Serielle Schnittstelle öffnen
*
* Wird vom SERIAL-Treiber für MagicMacX verwendet
* (DEV_SER.DEV), jedoch nicht im MagiC-Kernel selbst.
*
**********************************************************************/

uint32_t CMagiCSerial::AtariSerOpen(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    DebugInfo("CMagiC::AtariSerOpen()");

    // schon durch BIOS geöffnet => OK
    if (m_bBIOSSerialUsed)
    {
        DebugInfo("CMagiC::AtariSerOpen() -- schon durch BIOS geöffnet => OK");
        return 0;
    }

    // schon vom DOS geöffnet => Fehler
    if (CMagiCSerial::IsOpen())
    {
        DebugInfo("CMagiC::AtariSerOpen() -- schon vom DOS geöffnet => Fehler");
        return((uint32_t) EACCDN);
    }

    if (-1 == (int) CMagiCSerial::Open())
    {
        DebugInfo("CMagiC::AtariSerOpen() -- kann \"%s\" nicht öffnen.", Preferences::szAuxPath);
        return((uint32_t) ERROR);
    }

    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Serielle Schnittstelle schließen
*
* Wird vom SERIAL-Treiber für MagicMacX verwendet
* (DEV_SER.DEV), jedoch nicht im MagiC-Kernel selbst.
*
**********************************************************************/

uint32_t CMagiCSerial::AtariSerClose(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    DebugInfo("CMagiC::AtariSerClose()");

    // schon durch BIOS geöffnet => OK
    if (m_bBIOSSerialUsed)
        return 0;

    // nicht vom DOS geöffnet => Fehler
    if (!CMagiCSerial::IsOpen())
    {
        return(uint32_t) EACCDN;
    }

    if (CMagiCSerial::Close())
    {
        return (uint32_t) ERROR;
    }

    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Mehrere Zeichen von serieller Schnittstelle lesen
* Rückgabe: Anzahl Zeichen
*
* Wird vom SERIAL-Treiber für MagicMacX verwendet
* (DEV_SER.DEV), jedoch nicht im MagiC-Kernel selbst.
*
**********************************************************************/

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


/**********************************************************************
*
* Callback des Emulators: Mehrere Zeichen auf serielle Schnittstelle ausgeben
* params        Zeiger auf auszugebendes Zeichen (16 Bit)
* Rückgabe: Anzahl Zeichen
*
* Wird vom SERIAL-Treiber für MagicMacX verwendet
* (DEV_SER.DEV), jedoch nicht im MagiC-Kernel selbst.
*
**********************************************************************/

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


/**********************************************************************
*
* Callback des Emulators: Status für serielle Schnittstelle
* params        Zeiger auf Struktur
* Rückgabe: 0xffffffff (kann schreiben) oder 0 (kann nicht schreiben)
*
* Wird vom SERIAL-Treiber für MagicMacX verwendet
* (DEV_SER.DEV), jedoch nicht im MagiC-Kernel selbst.
*
**********************************************************************/

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


/**********************************************************************
*
* Callback des Emulators: Ioctl für serielle Schnittstelle
* params        Zeiger auf Struktur
* Rückgabe: Fehlercode
*
* Wird vom SERIAL-Treiber für MagicMacX verwendet
* (DEV_SER.DEV), jedoch nicht im MagiC-Kernel selbst.
*.
**********************************************************************/

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


    DebugInfo("CMagiC::AtariSerIoctl() -- Fcntl(cmd=0x%04x, parm=0x%08x)", be16toh(theSerIoctlParm->cmd), be32toh(theSerIoctlParm->parm));
    switch(be16toh(theSerIoctlParm->cmd))
    {
        case TIOCBUFFER:
            // Inquire/Set buffer settings
            DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCBUFFER) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCCTLMAP:
            // Inquire I/O-lines and signaling capabilities
            DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCCTLMAP) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCCTLGET:
            // Inquire I/O-lines and signals
            DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCCTLGET) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCCTLSET:
            // Set I/O-lines and signals
            DebugWarning("CMagiC::AtariSerConf() -- Fcntl(TIOCCTLSET) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCGPGRP:
            //get terminal process group
            DebugWarning("CMagiC::AtariSerIoctl() -- Fcntl(TIOCGPGRP) -- not supported");
            ret = (uint32_t) EINVFN;
            break;

        case TIOCSPGRP:
            //set terminal process group
            grp = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))));
            DebugInfo("CMagiC::AtariSerIoctl() -- Fcntl(TIOCSPGRP, %d)", (uint32_t) grp);
            ret = (uint32_t) EINVFN;
            break;

        case TIOCFLUSH:
            // Leeren der seriellen Puffer
            mode = be32toh(theSerIoctlParm->parm);
            DebugInfo("CMagiC::AtariSerIoctl() -- Fcntl(TIOCFLUSH, %d)", mode);
            switch(mode)
            {
                // Der Sendepuffer soll komplett gesendet werden. Die Funktion kehrt
                // erst zurück, wenn der Puffer leer ist (return E_OK, =0) oder ein
                // systeminterner Timeout abgelaufen ist (return EDRVNR, =-2). Der
                // Timeout wird vom System sinnvoll bestimmt.
                case 0:
                    ret = CMagiCSerial::Drain();
                    break;

                // Der Empfangspuffer wird gelöscht.
                case 1:
                    ret = CMagiCSerial::Flush(true, false);
                    break;

                // Der Sendepuffer wird gelöscht.
                case 2:
                    ret = CMagiCSerial::Flush(false, true);
                    break;

                // Empfangspuffer und Sendepuffer werden gelîscht.
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
            // Eingabegeschwindigkeit festlegen
            NewBaudrate = be32toh(*((uint32_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))));
            bSet = ((int) NewBaudrate != -1) && (NewBaudrate != 0);
            DebugInfo("CMagiC::AtariSerIoctl() -- Fcntl(%s, %d)", (be16toh(theSerIoctlParm->cmd) == TIOCIBAUD) ? "TIOCIBAUD" : "TIOCOBAUD", NewBaudrate);

            if (be16toh(theSerIoctlParm->cmd) == TIOCIBAUD)
                ret = CMagiCSerial::Config(
                    bSet,                        // Input-Rate ggf. ändern
                    NewBaudrate,                // neue Input-Rate
                    &OldBaudrate,                // alte Baud-Rate
                    false,                        // Output-Rate nicht ändern
                    0,                        // neue Output-Rate
                    NULL,                        // alte Output-Rate egal
                    false,                        // Xon/Xoff ändern
                    false,
                    NULL,
                    false,                        // Rts/Cts ändern
                    false,
                    NULL,
                    false,                        // parity enable ändern
                    false,
                    NULL,
                    false,                        // parity even ändern
                    false,
                    NULL,
                    false,                        // n Bits ändern
                    0,
                    NULL,
                    false,                        // Stopbits ändern
                    0,
                    NULL);
            else
                ret = CMagiCSerial::Config(
                    false,                        // Input-Rate nicht ändern
                    0,                        // neue Input-Rate
                    NULL,                        // alte Input-Rate egal
                    bSet,                        // Output-Rate ggf. ändern
                    NewBaudrate,                // neue Output-Rate
                    &OldBaudrate,                // alte Output-Rate
                    false,                        // Xon/Xoff ändern
                    false,
                    NULL,
                    false,                        // Rts/Cts ändern
                    false,
                    NULL,
                    false,                        // parity enable ändern
                    false,
                    NULL,
                    false,                        // parity even ändern
                    false,
                    NULL,
                    false,                        // n Bits ändern
                    0,
                    NULL,
                    false,                        // Stopbits ändern
                    0,
                    NULL);

            *((uint32_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))) = htobe32(OldBaudrate);
            if ((int) ret == -1)
                ret = (uint32_t) ATARIERR_ERANGE;
            break;

        case TIOCGFLAGS:
            // Übertragungsprotokolleinstellungen erfragen

            DebugInfo("CMagiC::AtariSerIoctl() -- Fcntl(TIOCGFLAGS, %d)", be32toh(theSerIoctlParm->parm));
            (void) CMagiCSerial::Config(
                        false,                        // Input-Rate nicht ändern
                        0,                        // neue Input-Rate
                        NULL,                        // alte Input-Rate egal
                        false,                        // Output-Rate nicht ändern
                        0,                        // neue Output-Rate
                        NULL,                        // alte Output-Rate egal
                        false,                        // Xon/Xoff nicht ändern
                        false,                        // neuer Wert
                        &bXonXoff,                    // alter Wert
                        false,                        // Rts/Cts nicht ändern
                        false,
                        &bRtsCts,
                        false,                        // parity enable nicht ändern
                        false,
                        &bParityEnable,
                        false,                        // parity even nicht ändern
                        false,
                        &bParityEven,
                        false,                        // n Bits nicht ändern
                        0,
                        &nBits,
                        false,                        // Stopbits nicht ändern
                        0,
                        &nStopBits);
            // Rückgabewert zusammenbauen
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
            // Übertragungsprotokolleinstellungen setzen
            flags = be16toh(*((uint16_t *) (addrOffset68k + be32toh(theSerIoctlParm->parm))));
            DebugInfo("CMagiC::AtariSerIoctl() -- Fcntl(TIOCSFLAGS, 0x%04x)", (uint32_t) flags);
            bXonXoff = (flags & 0x1000) != 0;
            DebugInfo("CMagiC::AtariSerIoctl() -- XON/XOFF %s", (bXonXoff) ? "ein" : "aus");
            bRtsCts = (flags & 0x2000) != 0;
            DebugInfo("CMagiC::AtariSerIoctl() -- RTS/CTS %s", (bRtsCts) ? "ein" : "aus");
            bParityEnable = (flags & (0x4000+0x8000)) != 0;
            DebugInfo("CMagiC::AtariSerIoctl() -- Parität %s", (bParityEnable) ? "ein" : "aus");
            bParityEven= (flags & 0x4000) != 0;
            DebugInfo("CMagiC::AtariSerIoctl() -- Parität %s", (bParityEven) ? "gerade (even)" : "ungerade (odd)");
            nBits = 8U - ((flags & 0xc) >> 2);
            DebugInfo("CMagiC::AtariSerIoctl() -- %d Bits", nBits);
            nStopBits = flags & 3U;
            DebugInfo("CMagiC::AtariSerIoctl() -- %d Stop-Bits%s", nStopBits, (nStopBits == 0) ? " (Synchron-Modus?)" : "");
            if ((nStopBits == 0) || (nStopBits == 2))
                return((uint32_t) ATARIERR_ERANGE);
            if (nStopBits == 3)
                nStopBits = 2;
            ret = CMagiCSerial::Config(
                        false,                        // Input-Rate nicht ändern
                        0,                        // neue Input-Rate
                        NULL,                        // alte Input-Rate egal
                        false,                        // Output-Rate nicht ändern
                        0,                        // neue Output-Rate
                        NULL,                        // alte Output-Rate egal
                        true,                        // Xon/Xoff ändern
                        bXonXoff,                    // neuer Wert
                        NULL,                        // alter Wert egal
                        true,                        // Rts/Cts ändern
                        bRtsCts,
                        NULL,
                        true,                        // parity enable ändern
                        bParityEnable,
                        NULL,
                        false,                        // parity even nicht ändern
                        bParityEven,
                        NULL,
                        true,                        // n Bits ändern
                        nBits,
                        NULL,
                        true,                        // Stopbits ändern
                        nStopBits,
                        NULL);
            if ((int) ret == -1)
                ret = (uint32_t) ATARIERR_ERANGE;
            break;

        default:
            DebugError("CMagiC::AtariSerIoctl() -- Fcntl(0x%04x -- unbekannt", be16toh(theSerIoctlParm->cmd) & 0xffff);
            ret = (uint32_t) EINVFN;
            break;
    }

    return ret;
}
