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
* Does everything that deals with "MagicMac OS"
*
*/

#include "config.h"
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <sys/stat.h>
#include <sys/param.h>
#include <time.h>
#include "emulation_globals.h"
#include "Debug.h"
#include "Globals.h"
#include "MagiC.h"
#include "MagiCSerial.h"
#include "MagiCPrint.h"
#include "Atari.h"
#include "volume_images.h"
#include "mem_access_68k.h"
#include "conversion.h"
#include "gui.h"

//#define _DEBUG_KB_CRITICAL_REGION 1

static CMagiC *pTheMagiC = nullptr;


#ifndef NDEBUG
bool CMagiC__sNoAtariInterrupts = false;    // for debugging
#endif


void sendBusError(uint32_t addr, const char *AccessMode)
{
    pTheMagiC->SendBusError(addr, AccessMode);
}
void getActAtariPrg(const char **pName, uint32_t *pact_pd)
{
    pTheMagiC->GetActAtariPrg(pName, pact_pd);
}


/**********************************************************************
 *
 * retro stuff
 *
 **********************************************************************/

static inline void OS_CreateCriticalRegion(pthread_mutex_t *criticalRegion)
{
    //DebugInfo2("(%p)", criticalRegion);
    *criticalRegion = PTHREAD_MUTEX_INITIALIZER;
}


// This may NOT be nested!
static inline void OS_EnterCriticalRegion(pthread_mutex_t *criticalRegion)
{
    //DebugInfo2("(%p)", criticalRegion);
    pthread_mutex_lock(criticalRegion);
}

static inline void OS_ExitCriticalRegion(pthread_mutex_t *criticalRegion)
{
    //DebugInfo2("(%p)", criticalRegion);
    pthread_mutex_unlock(criticalRegion);
}

static inline void OS_CreateEvent(uint32_t *eventId)
{
    *eventId = 0;
}


#if __UINTPTR_MAX__ == 0xFFFFFFFFFFFFFFFF

// 68k emulator callback jump table
#define JUMP_TABLE_LEN 256
void *jump_table[256];    // pointers to functions
unsigned jump_table_len = 0;
#define SELF_TABLE_LEN 32
void *self_table[32];    // pointers to objects
unsigned self_table_len = 0;

void setHostCallback(PTR32_HOST *dest, tfHostCallback callback)
{
    assert(jump_table_len < JUMP_TABLE_LEN);
    dest[0] = jump_table_len;
    jump_table[jump_table_len++] = (void *) callback;
}

static void setSelf(void *pthis, uint32_t *pIndex)
{
    // check if pointer is already registered
    unsigned i;
    for (i = 0; i < self_table_len; i++)
    {
        if (self_table[i] == pthis)
        {
            // found
            *pIndex = i;
            return;
        }
    }
    assert(self_table_len < SELF_TABLE_LEN);
    *pIndex = self_table_len;
    self_table[self_table_len++] = pthis;
}

static void setMethodCallback(PTR32x4_HOST *dest4, void *callback, void *pthis)
{
    assert(jump_table_len < JUMP_TABLE_LEN);
    uint32_t *dest = (uint32_t *) dest4;
    *dest++ = jump_table_len;
    jump_table[jump_table_len++] = callback;
    setSelf(pthis, dest);
}


// For whatever reasons the pointer to a class method occupies two pointer variables, not one,
// thus the conversion to a (void *) is discouraged. However, it seems that only the first of
// these pointers is needed, maybe the second one is necessary for virtual functions.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

void setCMagiCHostCallback(PTR32x4_HOST *dest4, tpfCMagiC_HostCallback callback, CMagiC *pthis)
{
    setMethodCallback(dest4, (void *) callback, (void *) pthis);
}

void setCXCmdHostCallback(PTR32x4_HOST *dest4, tpfCXCmd_HostCallback callback, CXCmd *pthis)
{
    setMethodCallback(dest4, (void *) callback, (void *) pthis);
}

void setCHostXFSHostCallback(PTR32x4_HOST *dest4, tpfCHostXFS_HostCallback callback, CHostXFS *pthis)
{
    setMethodCallback(dest4, (void *) callback, (void *) pthis);
}
#pragma GCC diagnostic pop


#else

void setHostCallback(PTR32_HOST *dest, tfHostCallback callback)
{
    dest[0] = callback;
}
void setCMagiCHostCallback(PTR32x4_HOST *dest, tpfCMagiC_HostCallback callback, CMagiC *pthis)
{
    dest[0] = callback;
    dest[2] = pThis;
}
void setCXCmdHostCallback(PTR32x4_HOST *dest, tpfCXCmd_HostCallback callback, CMagiC *pthis)
{
    dest[0] = callback;
    dest[2] = pThis;
}
void setCHostXFSHostCallback(PTR32x4_HOST *dest, tpfCHostXFS_HostCallback callback, CMagiC *pthis)
{
    dest[0] = callback;
    dest[2] = pThis;
}
#endif


/** **********************************************************************************************
 *
 * @brief Constructor
 *
 ************************************************************************************************/
CMagiC::CMagiC()
{
    m_CurrModifierKeys = 0;
    mem68k = nullptr;
    mem68kSize = 0;
    m_BasePage = NULL;
    memset(&m_EmuTaskID, 0, sizeof(m_EmuTaskID));

    m_EventId = 0;
    m_InterruptEventsId = 0;

    m_KbCriticalRegionId = PTHREAD_MUTEX_INITIALIZER;
    // m_AECriticalRegionId = PTHREAD_MUTEX_INITIALIZER;    // remnant from MagicMac(X) and AtariX
    m_ScrCriticalRegionId = PTHREAD_MUTEX_INITIALIZER;

    //m_iNoOfAtariFiles = 0;  // remnant from MagicMac(X) and AtariX
    m_pKbWrite = m_pKbRead = m_cKeyboardOrMouseData;
    m_bShutdown = false;
    m_bBusErrorPending = false;
    m_bInterrupt200HzPending = false;
    m_bInterruptVBLPending = false;
    m_bInterruptMouseKeyboardPending = false;
    m_bInterruptMouseButton[0] = m_bInterruptMouseButton[1] = false;
    m_InterruptMouseWhere.y = m_InterruptMouseWhere.x = 0;
    m_bInterruptPending = false;
    m_LineAVars = NULL;
    m_pMagiCScreen = NULL;
//    m_PrintFileRefNum = 0;
    pTheMagiC = this;
    m_bEmulatorHasEnded = false;
    m_bScreenBufferChanged = false;
    m_bEmulatorIsRunning = false;

    // inter-thread synchronisation
    m_EventMutex = PTHREAD_MUTEX_INITIALIZER;
    m_ConditionMutex = PTHREAD_MUTEX_INITIALIZER;
    m_Cond = PTHREAD_COND_INITIALIZER;

    atomic_init(&gbAtariVideoBufChanged, false);
}


/** **********************************************************************************************
 *
 * @brief Destructor
 *
 ************************************************************************************************/
CMagiC::~CMagiC()
{
    if (m_EmuTaskID)
    {
        // kill thread
        pthread_cancel(m_EmuTaskID);
        // wait for thread termination
        pthread_join(m_EmuTaskID, nullptr);
        m_bEmulatorIsRunning = false;
    }

    if (mem68k != nullptr)
    {
        free(mem68k);
    }
    CVolumeImages::exit();
}


/** **********************************************************************************************
 *
 * @brief Send message to 68k emulator
 *
 * @param[in] event        bit vector where to set flags
 * @param[in] flags        bit mask of flags to be set
 *
 ************************************************************************************************/
void CMagiC::OS_SetEvent(uint32_t *event, uint32_t flags)
{
    pthread_mutex_lock(&m_EventMutex);
    *event |= flags;
    pthread_mutex_unlock(&m_EventMutex);

    pthread_mutex_lock(&m_ConditionMutex);
    if (*event != 0)
    {
        pthread_cond_signal(&m_Cond);
    }
    pthread_mutex_unlock(&m_ConditionMutex);
}


/** **********************************************************************************************
 *
 * @brief 68k emulator waits for a message
 *
 * @param[in,out]  event        bit vector to wait for, is cleared on arrival
 * @param[out]     flags        bit vector with '1' and '0' flags
 *
 ************************************************************************************************/
void CMagiC::OS_WaitForEvent(uint32_t *event, uint32_t *flags)
{
    pthread_mutex_lock(&m_ConditionMutex);
    while (*event == 0)
    {
        // pthread_cond_timedwait() provides additional timeout
        pthread_cond_wait(&m_Cond, &m_ConditionMutex);
    }
    pthread_mutex_unlock(&m_ConditionMutex);

    pthread_mutex_lock(&m_EventMutex);
    *flags = *event;
    *event = 0;
    pthread_mutex_unlock(&m_EventMutex);
}


/**//**************************************************************************
*
*  @brief Get file size from host path
*
******************************************************************************/
static int64_t getFileSize(const char *hostPath)
{
	int ret;
	struct stat statbuf;

	ret = stat(hostPath, &statbuf);
	if (ret == 0)
	{
		return statbuf.st_size;
	}

	return -1;
}


/** **********************************************************************************************
*
* @brief Load and relocate an Atari executable file, here the MagiC kernel
*
* @param[in]   path           host path of Atari exe file
* @param[in]   stackSize      space needed after BSS segment
* @param[in]   reladdr        load to here, -1: load to end (default)
* @param[out]  basePage       pointer to loaded base page
*
* @return 0 = OK, otherwise error code
*
 ************************************************************************************************/
int CMagiC::LoadReloc
(
    const char *path,
    uint32_t stackSize,
    int32_t  reladdr,
    BasePage **basePage
)
{
    int err = 0;
    size_t len, codlen;
    ExeHeader exehead;
    BasePage *bp;
    unsigned char *relp;
    unsigned char relb;
    unsigned char *tpaStart, *relBuf = NULL, *reloff, *tbase, *bbase;
    unsigned long loff, tpaSize;
    long Fpos;
    FILE *f = nullptr;


    DebugInfo("CMagiC::LoadReloc(\"%s\")", path);

    int64_t fileSize = getFileSize(path);
    DebugInfo("CMagiC::LoadReloc() - kernel file size = %ld", fileSize);
    if (fileSize > 0)
    {
        f = fopen(path, "rb");
    }

    if (f == nullptr)
    {
        DebugError("CMagiC::LoadReloc() - Cannot open file \"%s\")", path);
/*
MagicMacX kann die MagiC-Kernel-Datei MagicMacX.OS nicht finden.

Installieren Sie die Applikation neu.
[MagiCMacX beenden]
MagicMacX could not find the MagiC kernel file "MagicMacX.OS".

Reinstall the application.
[Quit program]
*/
        (void) showAlert("The emulator cannot find the kernel file MAGICLIN.OS", "Repair configuration file!", 1);
        return -1;
    }

    size_t items_read;
    len = sizeof(ExeHeader);
    // read PRG Header
    items_read = fread(&exehead, len, 1, f);
    if (items_read != 1)
    {
        DebugError2("() - MagiC kernel file is too small.");
        goto exitReloc;
    }

    DebugInfo2("() - Size TEXT = %ld", be32toh(exehead.tlen));
    DebugInfo2("() - Size DATA = %ld", be32toh(exehead.dlen));
    DebugInfo2("() - Size BSS  = %ld", be32toh(exehead.blen));

    codlen = be32toh(exehead.tlen) + be32toh(exehead.dlen);
    if (be32toh(exehead.blen) & 1)
    {
    //    exehead.blen++;        // BSS-Segment auf gerade Länge
        exehead.blen = htobe32(be32toh(exehead.blen) + 1);    // big-endian increment
    }
    tpaSize = sizeof(BasePage) + codlen + be32toh(exehead.blen) + stackSize;

    DebugInfo2("() - Size overall, including basepage and stack = 0x%08x (%ld)", tpaSize, tpaSize);

    if (tpaSize > mem68kSize)
    {
        DebugError2("() - Insufficient memory");
        err = 1;
        goto exitReloc;
    }


// hier basepage auf durch 4 teilbare Adresse!

    if (reladdr < 0)
        reladdr = (long) (mem68kSize - ((tpaSize + 2) & ~3));
    if (reladdr + tpaSize > mem68kSize)
    {
        DebugError2("() - Invalid load address");
        err = 2;
        goto exitReloc;
    }

    tpaStart = mem68k + reladdr;
    tbase = tpaStart + sizeof(BasePage);
    reloff = tbase;
    bbase = tbase + codlen;

    // All 68k addresses are relative to m_RAM68k
    bp = (BasePage *) tpaStart;
    memset(bp, 0, sizeof(BasePage));
    bp->p_lowtpa = (PTR32_BE) htobe32(tpaStart - mem68k);
    bp->p_hitpa  = (PTR32_BE) htobe32(tpaStart - mem68k + tpaSize);
    bp->p_tbase  = (PTR32_BE) htobe32(tbase - mem68k);
    bp->p_tlen   = exehead.tlen;
    bp->p_dbase  = (PTR32_BE) htobe32(tbase - mem68k + be32toh(exehead.tlen));
    bp->p_dlen   = exehead.dlen;
    bp->p_bbase  = (PTR32_BE) htobe32(bbase - mem68k);
    bp->p_blen   = exehead.blen;
    bp->p_dta    = (PTR32_BE) htobe32(bp->p_cmdline - mem68k);
    bp->p_parent = 0;

    DebugInfo2("() - Start address Atari = 0x%08lx (host)", mem68k);
    DebugInfo2("() - Memory size Atari = 0x%08lx (= %lu kBytes)", mem68kSize, mem68kSize >> 10);
    DebugInfo2("() - Load address of system (TEXT) = 0x%08lx (68k)", be32toh((uint32_t) (bp->p_tbase)));

    #if defined(_DEBUG_BASEPAGE)
    {
        int i;
        const unsigned char *p = (const unsigned char *) bp;

        for (i = 0; i < sizeof(BasePage); i++)
        {
            DebugInfo("CMagiC::LoadReloc() - BasePage[%d] = 0x%02x", i, p[i]);
        }
    }
    #endif

    // read TEXT+DATA from file
    items_read = fread(tbase, codlen, 1, f);
    if (items_read != 1)
    {
        readerr:
        DebugError2("() - read error");
        goto exitReloc;
    }

    if (!be16toh(exehead.relmod))    // we must relocate
    {
        // seek to relocation table
        Fpos = be32toh(exehead.slen) + codlen + sizeof(exehead);
        DebugInfo2("() - seek to relocation table, file offset %u\n", Fpos);

        err = fseek(f, Fpos, SEEK_SET);
        if (err)
        {
            DebugError2("() - cannot seek to relocation table, file offset %u\n", Fpos);
            DebugError2("() - error code %d\n", errno);
            goto readerr;
        }
        len = 4;
        items_read = fread(&loff, len, 1, f);
        if (items_read != 1)
        {
            goto readerr;
        }

        loff = be32toh(loff);

        if (loff)    // we must relocate
        {
            Fpos += 4;

            // allocate buffer for relocation data
            size_t RelocBufSize = fileSize - Fpos;
            relBuf = (uint8_t *) malloc(RelocBufSize + 2);
            if (relBuf == nullptr)
            {
                err = 1;    // out-of-memory
                goto exitReloc;
            }

            // relocate first 32-bit word in Atari code
            uint32_t *tp = (uint32_t *) (reloff + loff);

            //*tp += (uint32_t) (reloff - mem68k);
            *tp = htobe32((uint32_t) (reloff - mem68k) + be32toh(*tp));

            // Reloc-Tabelle in einem Rutsch einlesen
            items_read = fread(relBuf, RelocBufSize, 1, f);
            if (items_read != 1)
            {
                goto readerr;
            }
            relBuf[RelocBufSize] = 0;    // just to be sure that the table is zero terminated

            relp = relBuf;
            while(*relp)
            {
                relb = *relp++;
                if (relb == 1)
                {
                    tp = (uint32_t *) ((char *) tp + 254);
                }
                else
                {
                    tp = (uint32_t *) ((char *) tp + (unsigned char) relb);

                    //*tp += (uint32_t) (reloff - mem68k);
                    *tp = htobe32((uint32_t) (reloff - mem68k) + be32toh(*tp));
                }
            }
        }
        else
        {
            DebugWarning2("() - No relocation");
        }
    }

    memset (bbase, 0, be32toh(exehead.blen));    // clear BSS

exitReloc:
    if (err)
        *basePage = NULL;
    else
        *basePage = bp;

    if (relBuf)
        free(relBuf);

    if (f != nullptr)
    {
        fclose(f);
    }

    return err;
}


/**********************************************************************
*
* (INTERN) Initialisierung von Atari68kData.m_VDISetupData
*
**********************************************************************/
void CMagiC::Init_CookieData(MgMxCookieData *pCookieData)
{
    pCookieData->mgmx_magic     = htobe32('MgMx');
    pCookieData->mgmx_version   = htobe32(PROGRAM_VERSION_MAJOR);
    pCookieData->mgmx_len       = htobe32(sizeof(MgMxCookieData));
    pCookieData->mgmx_xcmd      = htobe32(0);        // wird vom Kernel gesetzt
    pCookieData->mgmx_xcmd_exec = htobe32(0);        // wird vom Kernel gesetzt
    pCookieData->mgmx_internal  = htobe32(0);        // wird vom Kernel gesetzt
    pCookieData->mgmx_daemon    = htobe32(0);        // wird vom Kernel gesetzt
}


/**********************************************************************
*
* Pixmap ggf. nach big endian wandeln
*
**********************************************************************/
#if !defined(__BIG_ENDIAN__)
static void PixmapToBigEndian(MXVDI_PIXMAP *thePixMap)
{
    DebugInfo2("Host-CPU ist little-endian. Rufe PixmapToBigEndian() auf.");

    // video RAM access is more efficient with host endianess
    gbAtariVideoRamHostEndian = (thePixMap->pixelSize == 32);

    thePixMap->baseAddr      = (PTR32_BE) htobe32((uint32_t) thePixMap->baseAddr);
    thePixMap->rowBytes      = htobe16(thePixMap->rowBytes);
    thePixMap->bounds_top    = htobe16(thePixMap->bounds_top);
    thePixMap->bounds_left   = htobe16(thePixMap->bounds_left);
    thePixMap->bounds_bottom = htobe16(thePixMap->bounds_bottom);
    thePixMap->bounds_right  = htobe16(thePixMap->bounds_right);
    thePixMap->pmVersion     = htobe16(thePixMap->pmVersion);
    thePixMap->packType      = htobe16(thePixMap->packType);
    thePixMap->packSize      = htobe32(thePixMap->packSize);
    thePixMap->hRes          = htobe32(thePixMap->hRes);
    thePixMap->vRes          = htobe32(thePixMap->vRes);
    thePixMap->pixelType     = htobe16(thePixMap->pixelType);
    thePixMap->pixelSize     = htobe16(thePixMap->pixelSize);
    thePixMap->cmpCount      = htobe16(thePixMap->cmpCount);
    thePixMap->cmpSize       = htobe16(thePixMap->cmpSize);
    thePixMap->planeBytes    = htobe32(thePixMap->planeBytes);
/*
    if (thePixMap->pixelFormat == k32BGRAPixelFormat)
    {
        DebugInfo("PixmapToBigEndian() -- k32BGRAPixelFormat => k32ARGBPixelFormat");
        thePixMap->pixelFormat = htobe32(k32ARGBPixelFormat);
    }
*/
}
#endif


/**********************************************************************
*
* Debug-Hilfe
*
**********************************************************************/

#if DEBUG_68K_EMU
void _DumpAtariMem(const char *filename)
{
    if (pTheMagiC)
        pTheMagiC->DumpAtariMem(filename);
}

void CMagiC::DumpAtariMem(const char *filename)
{
    FILE *f;

    if (!m_RAM68k)
    {
        DebugError("DumpAtariMem() -- kein Atari-Speicher?!?");
        return;
    }

    f = fopen(filename, "wb");
    if (!f)
    {
        DebugError("DumpAtariMem() -- kann Datei nicht erstellen.");
        return;
    }
    fwrite(m_RAM68k, 1, m_RAM68ksize, f);
    fclose(f);
}
#endif


/**********************************************************************
*
* Initialisierung
* => 0 = OK, sonst = Fehler
*
* Virtueller 68k-Adreßraum:
*
*    Systemvariablen/(X)BIOS-Variablen
*    frei
*    Atari68kData                        <- _memtop
*    Kernel-Basepage (256 Bytes)
*    MacXSysHdr
*    Rest des Kernels
*    Ende des Atari-RAM
*    Beginn des virtuellen VRAM            <- _v_bas_ad
*    Ende des virtuellen VRAM
*                                        <- _phystop
*
* Die Größe des virtuellen VRAM berechnet sich aus der Anzahl der
* Bildschirmzeilen des Atari und der physikalischen (!) Bildschirm-
* Zeilenlänge des Mac, d.h. ist abhängig von der physikalischen
* Auflösung des Mac-Bildschirms.
*
**********************************************************************/

int CMagiC::Init(CMagiCScreen *pMagiCScreen, CXCmd *pXCmd)
{
    int err;
    Atari68kData *pAtari68kData;
    struct MacXSysHdr *pMacXSysHdr;
    struct SYSHDR *pSysHdr;
    uint32_t AtariMemtop;        // Ende Atari-Benutzerspeicher
    uint32_t chksum;
    unsigned numVideoLines;


    // Konfiguration

    DebugInfo("MultiThread version for Linux");
#ifdef EMULATE_68K_TRACE
    DebugInfo("68k trace is emulated (slows down the emulator a bit)");
#else
    DebugInfo("68k trace is not emulated (makes the emulator a bit faster)");
#endif
#ifdef _DEBUG_WRITEPROTECT_ATARI_OS
    DebugInfo("68k ROM is write-protected (slows down the emulator a bit)");
#else
    DebugInfo("68k ROM is not write-protected (makes the emulator a bit faster)");
#endif
#ifdef WATCH_68K_PC
    DebugInfo("68k PC is checked for validity (slows down the emulator a bit)");
#else
    DebugInfo("68k PC is not checked for validity (makes the emulator a bit faster)");
#endif
    //DebugInfo("Mac-Menü %s", (CGlobals::s_bShowMacMenu) ? "ein" : "aus");
    DebugInfo("Autostart %s", (Preferences::bAutoStartMagiC) ? "ON" : "OFF");

    // Atari screen data

    m_pMagiCScreen = pMagiCScreen;

    // Tastaturtabelle lesen

    (void) CMagiCKeyboard::init();

    mem68kSize = Preferences::AtariMemSize;
    numVideoLines = m_pMagiCScreen->m_PixMap.bounds_bottom - m_pMagiCScreen->m_PixMap.bounds_top + 1;
    m_FgBufferLineLenInBytes = (m_pMagiCScreen->m_PixMap.rowBytes & 0x3fff);
    memVideo68kSize = m_FgBufferLineLenInBytes * numVideoLines;
    // get Atari memory
    // TODO: In fact we do not have to add m_Video68ksize here, because video memory
    //       is allocated separately as SDL surface in EmulatorRunner.
    mem68k = (unsigned char *) malloc(mem68kSize + memVideo68kSize);
    if (mem68k == nullptr)
    {
/*
Der Applikation steht nicht genügend Speicher zur Verfügung.

Weisen Sie der Applikation mit Hilfe des Finder-Dialogs "Information" mehr Speicher zu!
[Abbruch]
The application ran out of memory.

Assign more memory to the application using the Finder dialogue "Information"!
[Cancel]
*/
        showAlert("The emulator cannot reserve enough memory", "Reduce Atari memory size in configuration file", 1);
        return(1);
    }

    // Atari-Kernel lesen
    err = LoadReloc(Preferences::AtariKernelPath, 0, -1, &m_BasePage);
    if (err)
        return(err);
    DebugInfo("MagiC kernel loaded and relocated successfully");

    // 68k Speicherbegrenzungen ausrechnen
    addr68kVideo = mem68kSize;
    DebugInfo("68k video memory starts at 68k address 0x%08x and uses %u (0x%08x) bytes.", addr68kVideo, memVideo68kSize, memVideo68kSize);
    addr68kVideoEnd = addr68kVideo + memVideo68kSize;
    DebugInfo("68k video memory and general memory end is 0x%08x", addr68kVideoEnd);
    // real (host) address of video memory
    //m_pFgBuffer = m_RAM68k + Preferences::AtariMemSize;    // unused

    //
    // Initialise Atari system variables
    //

    setAtariBE32(mem68k + phystop,  addr68kVideoEnd);
    setAtariBE32(mem68k + _v_bas_ad, mem68kSize);
    AtariMemtop = ((uint32_t) ((unsigned char *) m_BasePage - mem68k)) - sizeof(Atari68kData);
    setAtariBE32(mem68k + _memtop, AtariMemtop);
    setAtariBE16(mem68k + sshiftmd, 2);                // ST high resolution (640*400*2)
    setAtariBE16(mem68k +_cmdload, 0);                // boot AES
    setAtariBE16(mem68k +_nflops, 0);                    // no floppy disk drives

    // Atari-68k-Daten setzen

    pAtari68kData = (Atari68kData *) (mem68k + AtariMemtop);
    pAtari68kData->m_PixMap = m_pMagiCScreen->m_PixMap;
    // left and top seem to be ignored, i.e. only  right and bottom are relevant
    pAtari68kData->m_PixMap.baseAddr = (PTR32_BE) addr68kVideo;        // virtual 68k address

    #if !defined(__BIG_ENDIAN__)
    PixmapToBigEndian(&pAtari68kData->m_PixMap);
    #endif

    DebugInfo("CMagiC::Init() - basepage address of system = 0x%08lx (68k)", AtariMemtop + sizeof(Atari68kData));
    DebugInfo("CMagiC::Init() - address of Atari68kData = 0x%08lx (68k)", AtariMemtop);

    // neue Daten

    Init_CookieData(&pAtari68kData->m_CookieData);

    // Alle Einträge der Übergabestruktur füllen

    pMacXSysHdr = (MacXSysHdr *) (m_BasePage + 1);        // Zeiger hinter Basepage

    if (be32toh(pMacXSysHdr->MacSys_magic) != 'MagC')
    {
        DebugError("CMagiC::Init() - magic value mismatch");
        goto err_inv_os;
    }

    DebugInfo("CMagiC::Init() - sizeof(CMagiC_CPPCCallback) = %u", (unsigned) sizeof(CMagiC_CPPCCallback));
    typedef UINT32 (CMagiC::*CMagiC_PPCCallback)(UINT32 params, uint8_t *AdrOffset68k);
    DebugInfo("CMagiC::Init() - sizeof(CMagiC_PPCCallback) = %u", (unsigned) sizeof(CMagiC_PPCCallback));

    assert(sizeof(CMagiC_CPPCCallback) == 16);

    if (be32toh(pMacXSysHdr->MacSys_len) != sizeof(*pMacXSysHdr))
    {
        DebugError("CMagiC::Init() - Length of struct does not match (header: %u bytes, should be: %u bytes)",
                         be32toh(pMacXSysHdr->MacSys_len), sizeof(*pMacXSysHdr));
        err_inv_os:
/*
Die Datei "MagicMacX.OS" ist defekt oder gehört zu einer anderen Programmversion als der Emulator.

Installieren Sie die Applikation neu.
[MagiCMacX beenden]
The file "MagicMacX.OS" seems to be corrupted or belongs to a different (newer or older) version as the emulator program.

Reinstall the application.
[Quit program]
*/
        showAlert("The kernel file MAGICLIN.OS is invalid or outdated", "Review your configuration", 1);
        return(1);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    pMacXSysHdr->MacSys_verMac = htobe32(10);
    pMacXSysHdr->MacSys_cpu = htobe16(20);        // 68020
    pMacXSysHdr->MacSys_fpu = htobe16(0);        // keine FPU
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_init, &CMagiC::AtariInit, this);
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_biosinit, &CMagiC::AtariBIOSInit, this);
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_VdiInit, &CMagiC::AtariVdiInit, this);
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_Exec68k, &CMagiC::AtariExec68k, this);
    pMacXSysHdr->MacSys_pixmap = htobe32((uint32_t) (((uint64_t) &pAtari68kData->m_PixMap) - (uint64_t) mem68k));
    pMacXSysHdr->MacSys_pMMXCookie = htobe32((uint32_t) (((uint64_t) &pAtari68kData->m_CookieData) - (uint64_t) mem68k));
    setCXCmdHostCallback(&pMacXSysHdr->MacSys_Xcmd, &CXCmd::Command, pXCmd);
    pMacXSysHdr->MacSys_PPCAddr = 0;                // on 32-bit host: mem68k
    pMacXSysHdr->MacSys_VideoAddr = 0x80000000;        // on 32-bit host: m_pMagiCScreen->m_PixMap.baseAddr
    setHostCallback(&pMacXSysHdr->MacSys_gettime,    AtariGettime);
    setHostCallback(&pMacXSysHdr->MacSys_settime,    AtariSettime);
    setHostCallback(&pMacXSysHdr->MacSys_Setpalette, AtariSetpalette);
    setHostCallback(&pMacXSysHdr->MacSys_Setcolor,   AtariSetcolor);
    setHostCallback(&pMacXSysHdr->MacSys_VsetRGB,    AtariVsetRGB);
    setHostCallback(&pMacXSysHdr->MacSys_VgetRGB,    AtariVgetRGB);
    setHostCallback(&pMacXSysHdr->MacSys_syshalt,    AtariSysHalt);
    setHostCallback(&pMacXSysHdr->MacSys_syserr,     AtariSysErr);
    setHostCallback(&pMacXSysHdr->MacSys_coldboot,   AtariColdBoot);
    setHostCallback(&pMacXSysHdr->MacSys_exit,       AtariExit);
    setHostCallback(&pMacXSysHdr->MacSys_debugout,   AtariDebugOut);
    setHostCallback(&pMacXSysHdr->MacSys_error,      AtariError);
    setHostCallback(&pMacXSysHdr->MacSys_prtos,      &CMagiCPrint::AtariPrtOs);
    setHostCallback(&pMacXSysHdr->MacSys_prtin,      &CMagiCPrint::AtariPrtIn);
    setHostCallback(&pMacXSysHdr->MacSys_prtout,     &CMagiCPrint::AtariPrtOut);
    setHostCallback(&pMacXSysHdr->MacSys_prtouts,    &CMagiCPrint::AtariPrtOutS);
    setHostCallback(&pMacXSysHdr->MacSys_serconf,    &CMagiCSerial::AtariSerConf);
    setHostCallback(&pMacXSysHdr->MacSys_seris,      &CMagiCSerial::AtariSerIs);
    setHostCallback(&pMacXSysHdr->MacSys_seros,      &CMagiCSerial::AtariSerOs);
    setHostCallback(&pMacXSysHdr->MacSys_serin,      &CMagiCSerial::AtariSerIn);
    setHostCallback(&pMacXSysHdr->MacSys_serout,     &CMagiCSerial::AtariSerOut);
    setHostCallback(&pMacXSysHdr->MacSys_SerOpen,    &CMagiCSerial::AtariSerOpen);
    setHostCallback(&pMacXSysHdr->MacSys_SerClose,   &CMagiCSerial::AtariSerClose);
    setHostCallback(&pMacXSysHdr->MacSys_SerRead,    &CMagiCSerial::AtariSerRead);
    setHostCallback(&pMacXSysHdr->MacSys_SerWrite,   &CMagiCSerial::AtariSerWrite);
    setHostCallback(&pMacXSysHdr->MacSys_SerStat,    &CMagiCSerial::AtariSerStat);
    setHostCallback(&pMacXSysHdr->MacSys_SerIoctl,   &CMagiCSerial::AtariSerIoctl);
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_GetKeybOrMouse, &CMagiC::AtariGetKeyboardOrMouseData, this);
    setHostCallback(&pMacXSysHdr->MacSys_dos_macfn, AtariDOSFn);
    setCHostXFSHostCallback(&pMacXSysHdr->MacSys_xfs, &CHostXFS::XFSFunctions, &m_HostXFS);
    setCHostXFSHostCallback(&pMacXSysHdr->MacSys_xfs_dev, &CHostXFS::XFSDevFunctions, &m_HostXFS);
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_drv2devcode, &CMagiC::Drv2DevCode, this);
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_rawdrvr, &CMagiC::RawDrvr, this);
#if defined(MAGICLIN)
    setHostCallback(&pMacXSysHdr->MacSys_Daemon, MmxDaemon);
    setHostCallback(&pMacXSysHdr->MacSys_BlockDevice, CVolumeImages::AtariBlockDevice);
#else
    setCMagiCHostCallback(&pMacXSysHdr->MacSys_Daemon, &CMagiC::MmxDaemon, this);
#endif
    setHostCallback(&pMacXSysHdr->MacSys_Yield, AtariYield);
#pragma GCC diagnostic pop

    // ssp nach Reset
    setAtariBE32(mem68k + 0, 512*1024);        // Stack auf 512k
    // pc nach Reset
    setAtari32(mem68k + 4, /*big endian*/ pMacXSysHdr->MacSys_syshdr);

    // TOS-SYSHDR bestimmen

    pSysHdr = (SYSHDR *) (mem68k + be32toh(pMacXSysHdr->MacSys_syshdr));

    // Adresse für kbshift, kbrepeat und act_pd berechnen

    m_AtariKbData = mem68k + be32toh(pSysHdr->kbshift);
    m_pAtariActPd = (uint32_t *) (mem68k + be32toh(pSysHdr->_run));
    m_HostXFS.setActPdAddr(m_pAtariActPd);

    // Andere Atari-Strukturen

    m_pAtariActAppl = (uint32_t *) (mem68k + be32toh(pMacXSysHdr->MacSys_act_appl));

    // Prüfsumme für das System berechnen

    chksum = 0;
    uint32_t *fromptr = (uint32_t *) (mem68k + be32toh(pMacXSysHdr->MacSys_syshdr));
    uint32_t *toptr = (uint32_t *) (mem68k + be32toh((uint32_t) m_BasePage->p_tbase) + be32toh(m_BasePage->p_tlen) + be32toh(m_BasePage->p_dlen));
#ifdef _DEBUG
//    AdrOsRomStart = be32toh(pMacXSysHdr->MacSys_syshdr);            // Beginn schreibgeschützter Bereich
    addrOsRomStart = be32toh((uint32_t) m_BasePage->p_tbase);        // Beginn schreibgeschützter Bereich
    addrOsRomEnd = be32toh((uint32_t) m_BasePage->p_tbase) + be32toh(m_BasePage->p_tlen) + be32toh(m_BasePage->p_dlen);    // Ende schreibgeschützter Bereich
#endif
    do
    {
        chksum += htobe32(*fromptr++);
    }
    while(fromptr < toptr);

    setAtariBE32(mem68k + os_chksum, chksum);

    // dump Atari

//    DumpAtariMem("AtariMemAfterInit.data");

    // Pass all XFS drives to the Atari

    setAtariBE32(mem68k + _drvbits, 0);        // no Atari drives yet
    m_HostXFS.activateXfsDrives(mem68k);

    uint32_t volumes_drvbits;
    CVolumeImages::init(&volumes_drvbits);
    uint32_t drvbits = getAtariBE32(mem68k + _drvbits);
    drvbits |= volumes_drvbits;
    setAtariBE32(mem68k + _drvbits, drvbits);

    setAtariBE16(mem68k + _bootdev, 'C'-'A');    // Atari boot drive C:
    // If _nflops < 2, drive U: will suppress B:
    if (drvbits & 2)
    {
        setAtariBE16(mem68k + _nflops, 2);
    }
    else
    if (drvbits & 1)
    {
        setAtariBE16(mem68k + _nflops, 1);
    }

    //
    // initialise 68k emulator (Musashi)
    //

#if defined(USE_ASGARD_PPC_68K_EMU)

    OpcodeROM = mem68k;    // ROM == RAM
    Asgard68000SetIRQCallback(IRQCallback, this);
    Asgard68000SetHiMem(m_RAM68ksize);
    m_bSpecialExec = false;
//    Asgard68000Reset();
//    CPU mit vbr, sr und cacr
    Asgard68000Reset();

#else
    // The 68020 is the most powerful CPU that is supported by Musashi
    m68k_set_cpu_type(M68K_CPU_TYPE_68020);
    m68k_init();

    // Emulator starten

    addrOpcodeROM = mem68k;    // ROM == RAM
    m68k_set_int_ack_callback(IRQCallback);
    m68k_SetBaseAddr(mem68k);
    m68k_SetHiMem(mem68kSize);
    m_bSpecialExec = false;

    // Reset Musashi 68k emulator
    m68k_pulse_reset();

#endif

    return 0;
}


/**********************************************************************
*
* (STATISCH) gibt drvbits zurück
*
**********************************************************************/
/*
uint32_t CMagiC::GetAtariDrvBits(void)
{
    *((uint32_t *)(pTheMagiC->m_RAM68k + _drvbits)) = htobe32(0);        // noch keine Laufwerke

    newbits |= (1L << ('m'-'a'));    // virtuelles Laufwerk M: immer präsent
    *(long*)(&addrOffset68k[_drvbits]) &= -1L-xfs_drvbits;        // alte löschen
    *(long*)(&addrOffset68k[_drvbits]) |= newbits;            // neue setzen
    xfs_drvbits = newbits;
}
*/

/**********************************************************************
*
* (STATISCH) gibt Namen und PD des aktuellen Atari-Programms zurück
*
**********************************************************************/

void CMagiC::GetActAtariPrg(const char **pName, uint32_t *pact_pd)
{
    uint32_t pprocdata;
    MagiC_PD *pMagiCPd;
    MagiC_ProcInfo *pMagiCProcInfo;


    *pact_pd = be32toh(*pTheMagiC->m_pAtariActPd);
    if ((*pact_pd != 0) && (*pact_pd < mem68kSize))
    {
        pMagiCPd = (MagiC_PD *) (addrOpcodeROM + *pact_pd);
        pprocdata = be32toh(pMagiCPd->p_procdata);
        if ((pprocdata != 0) && (pprocdata < mem68kSize))
        {
            pMagiCProcInfo = (MagiC_ProcInfo *) (addrOpcodeROM + pprocdata);
            *pName = pMagiCProcInfo->pr_fname;    // array, not pointer!
        }
        else
            *pName = NULL;
    }
    else
        *pName = NULL;

#if 0
    MagiC_APP *pMagiCApp;
    uint32_t pact_appl = be32toh(*pTheMagiC->m_pAtariActAppl);
    if ((pact_appl != 0) && (pact_appl < pTheMagiC->m_RAM68ksize))
    {
        pMagiCApp = (MagiC_APP *) (OpcodeROM + pact_appl);
    }
#endif
}


#if 0
/**********************************************************************
*
* Atari-Laufwerk hat sich geändert
*
**********************************************************************/

void CMagiC::ChangeXFSDrive(short drvNr)
{
    CHostXFS::HostXFSDrvType NewType;

    if (drvNr == 'U' - 'A')
        return;                    // C:, M:, U:  sind nicht änderbar

    if ((drvNr == 'C' - 'A') || (drvNr == 'M' - 'A'))
    {
        m_HostXFS.ChangeXFSDriveFlags(
                    drvNr,                // Laufwerknummer
                    (Preferences::drvFlags[drvNr] & 2) ? false : true,    // lange Dateinamen
                    (Preferences::drvFlags[drvNr] & 1) ? true : false    // umgekehrte Verzeichnis-Reihenfolge (Problem bei OS X 10.2!)
                    );
    }
    else
    {
        NewType = (Preferences::drvPath[drvNr] == NULL) ? CHostXFS::eNoHostXFS : CHostXFS::eHostDir;

        m_HostXFS.SetXFSDrive(
                    drvNr,                // Laufwerknummer
                    NewType,            // Laufwerktyp: Mac-Verzeichnis oder nichts
                    Preferences::drvPath[drvNr],
                    (Preferences::drvFlags[drvNr] & 2) ? false : true,    // lange Dateinamen
                    (Preferences::drvFlags[drvNr] & 1) ? true : false,    // umgekehrte Verzeichnis-Reihenfolge (Problem bei OS X 10.2!)
                    m_RAM68k);
        }
}
#endif


/**********************************************************************
*
* Ausführungs-Thread starten
* => 0 = OK, sonst = Fehler
*
* Erstellt und startet den MP-Thread, der den Atari ausführt.
*
**********************************************************************/

int CMagiC::CreateThread( void )
{
    if (m_BasePage == nullptr)
    {
        return(-1);
    }

    m_bCanRun = false;        // nicht gestartet

    // Event für Start/Stop erstellen

    OS_CreateEvent(&m_EventId);

    // Event für Interrupts erstellen (idle loop aufwecken)

    OS_CreateEvent(&m_InterruptEventsId);

    // CriticalRegion für Tastaturpuffer und Bildschirmpufferadressen erstellen

    OS_CreateCriticalRegion(&m_KbCriticalRegionId);
    //OS_CreateCriticalRegion(&m_AECriticalRegionId);// remnant from MagicMac(X) and AtariX
    OS_CreateCriticalRegion(&m_ScrCriticalRegionId);

    // create emulation thread
    int err = pthread_create(
        &m_EmuTaskID,    // out: thread descriptor
        nullptr,        // no special attributes, use default
        _EmuThread,        // start routine
        this                // Parameter
        );
    if (err)
    {
        DebugError("CMagiC::CreateThread() - Fehler beim Erstellen des Threads");
        return err;
    }

    DebugInfo("CMagiC::CreateThread() - erfolgreich");

    /*
    //TODO: is there a task weight?
    //errl = MPSetTaskWeight(m_EmuTaskID, 300);    // 100 ist default, 200 ist die blue task

    // Workaround für Fehler in OS X 10.0.0
    if (errl > 0)
    {
        DebugWarning("CMagiC::CreateThread() - Betriebssystem-Fehler beim Priorisieren des Threads");
        errl = 0;
    }

    if (errl)
    {
        DebugError("CMagiC::CreateThread() - Fehler beim Priorisieren des Threads");
        return errl;
    }
    */

    return 0;
}


/**********************************************************************
*
* Läßt den Ausführungs-Thread loslaufen
* => 0 = OK, sonst = Fehler
*
**********************************************************************/

void CMagiC::StartExec( void )
{
    m_bCanRun = true;        // darf laufen
    m_AtariKbData[0] = 0;        // kbshift löschen
    m_AtariKbData[1] = 0;        // kbrepeat löschen
    OS_SetEvent(            // aufwecken
            &m_EventId,
            EMU_EVNT_RUN);
}


/**********************************************************************
*
* Hält den Ausführungs-Thread an
* => 0 = OK, sonst = Fehler
*
**********************************************************************/

void CMagiC::StopExec( void )
{
#if defined(USE_ASGARD_PPC_68K_EMU)
    Asgard68000SetExitImmediately();
#else
    m68k_StopExecution();
#endif
    m_bCanRun = false;        // darf nicht laufen
#ifdef MAGICMACX_DEBUG68K
    for    (register int i = 0; i < 100; i++)
        CDebug::DebugInfo("### VideoRamWriteCounter(%2d) = %d", i, WriteCounters[i]);
#endif
}


/**********************************************************************
*
* Terminiert den Ausführungs-Thread bei Programm-Ende
*
**********************************************************************/

void CMagiC::TerminateThread(void)
{
    DebugInfo("CMagiC::TerminateThread()");
    OS_SetEvent(
            &m_EventId,
            EMU_EVNT_TERM);
    StopExec();
}


/**********************************************************************
*
* This is the worker thread
* TODO: what is the meaning of the return pointer?
*
**********************************************************************/

void *CMagiC::_EmuThread(void *param)
{
    return (void *) (uint64_t) (((CMagiC *) param)->EmuThread());
}

int CMagiC::EmuThread( void )
{
    uint32_t EventFlags;
    bool bNewBstate[2];
    bool bNewMpos;
    bool bNewKey;


    m_bEmulatorIsRunning = true;

    for    (;;)
    {

        while (!m_bCanRun)
        {
            // wir warten darauf, daß wir laufen dürfen

            DebugInfo("CMagiC::EmuThread() -- MPWaitForEvent");
            OS_WaitForEvent(
                        &m_EventId,
                        &EventFlags);

            DebugInfo("CMagiC::EmuThread() -- MPWaitForEvent beendet");

            // wir prüfen, ob wir zum Beenden aufgefordert wurden

            if (EventFlags & EMU_EVNT_TERM)
            {
                DebugInfo("CMagiC::EmuThread() -- normaler Abbruch");
                goto end_of_thread;    // normaler Abbruch, Thread-Ende
            }
        }

        // längere Ausführungsphase

//        CDebug::DebugInfo("CMagiC::EmuThread() -- Starte 68k-Emulator");
        m_bWaitEmulatorForIRQCallback = false;
#if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000Execute();
#else
        m68k_execute();
#endif
//        CDebug::DebugInfo("CMagiC::EmuThread() --- %d 68k-Zyklen", CyclesExecuted);

        // Bildschirmadressen geändert
        if (m_bScreenBufferChanged)
        {
            OS_EnterCriticalRegion(&m_ScrCriticalRegionId);
            m_bScreenBufferChanged = false;
            OS_ExitCriticalRegion(&m_ScrCriticalRegionId);
        }

        // ausstehende Busfehler bearbeiten
        if (m_bBusErrorPending)
        {
#if defined(USE_ASGARD_PPC_68K_EMU)
            Asgard68000SetBusError();
#else
            m68k_exception_bus_error();
#endif
            m_bBusErrorPending = false;
        }

        // ausstehende Interrupts bearbeiten
        if (m_bInterruptPending)
        {
            m_bWaitEmulatorForIRQCallback = true;
            do
            {
#if defined(USE_ASGARD_PPC_68K_EMU)
                Asgard68000Execute();        // warte bis IRQ-Callback
#else
                m68k_execute();        // warte bis IRQ-Callback
#endif
//                CDebug::DebugInfo("CMagiC::Exec() --- Interrupt Pending => %d 68k-Zyklen", CyclesExecuted);
            }
            while(m_bInterruptPending);
        }

        // aufgelaufene Maus-Interrupts bearbeiten

        if (m_bInterruptMouseKeyboardPending)
        {
#ifdef _DEBUG_KB_CRITICAL_REGION
            CDebug::DebugInfo("CMagiC::EmuThread() --- Enter critical region m_KbCriticalRegionId");
#endif
            OS_EnterCriticalRegion(&m_KbCriticalRegionId);
            if (GetKbBufferFree() < 3)
            {
                DebugError("CMagiC::EmuThread() --- Tastenpuffer ist voll");
            }
            else
            {
                bNewBstate[0] = CMagiCMouse::setNewButtonState(0, m_bInterruptMouseButton[0]);
                bNewBstate[1] = CMagiCMouse::setNewButtonState(1, m_bInterruptMouseButton[1]);
                bNewMpos =  CMagiCMouse::setNewPosition(m_InterruptMouseWhere);
                bNewKey = (m_pKbRead != m_pKbWrite);
                if (bNewBstate[0] || bNewBstate[1] || bNewMpos || bNewKey)
                {
                    // Interrupt-Vektor 70 für Tastatur/MIDI mitliefern
                    m_bInterruptPending = true;
#if defined(USE_ASGARD_PPC_68K_EMU)
                    Asgard68000SetIRQLineAndExcVector(k68000IRQLineIRQ6, k68000IRQStateAsserted, 70);
#else
                    m68k_set_irq(M68K_IRQ_6);    // autovector interrupt 70
#endif
                }
            }
            m_bInterruptMouseKeyboardPending = false;

/*
            errl = MPResetEvent(            // kein "pending kb interrupt"
                    m_InterruptEventsId,
                    EMU_INTPENDING_KBMOUSE);
*/
            OS_ExitCriticalRegion(&m_KbCriticalRegionId);
#ifdef _DEBUG_KB_CRITICAL_REGION
            CDebug::DebugInfo("CMagiC::EmuThread() --- Exited critical region m_KbCriticalRegionId");
#endif
            m_bWaitEmulatorForIRQCallback = true;
            while(m_bInterruptPending)
#if defined(USE_ASGARD_PPC_68K_EMU)
                Asgard68000Execute();        // warte bis IRQ-Callback
#else
                m68k_execute();        // warte bis IRQ-Callback
#endif
        }

        // aufgelaufene 200Hz-Interrupts bearbeiten

        if (m_bInterrupt200HzPending)
        {
            m_bInterrupt200HzPending = false;
/*
            errl = MPResetEvent(            // kein "pending kb interrupt"
                    m_InterruptEventsId,
                    EMU_INTPENDING_200HZ);
*/
            m_bInterruptPending = true;
            m_bWaitEmulatorForIRQCallback = true;
#if defined(USE_ASGARD_PPC_68K_EMU)
            Asgard68000SetIRQLineAndExcVector(k68000IRQLineIRQ5, k68000IRQStateAsserted, 69);
#else
            m68k_set_irq(M68K_IRQ_5);        // autovector interrupt 69
#endif
            while(m_bInterruptPending)
            {
#if defined(USE_ASGARD_PPC_68K_EMU)
                Asgard68000Execute();        // warte bis IRQ-Callback
#else
                m68k_execute();        // warte bis IRQ-Callback
#endif
//                CDebug::DebugInfo("CMagiC::EmuThread() --- m_bInterrupt200HzPending => %d 68k-Zyklen", CyclesExecuted);
            }
        }

        // aufgelaufene VBL-Interrupts bearbeiten

        if (m_bInterruptVBLPending)
        {
            m_bInterruptVBLPending = false;
/*
            errl = MPResetEvent(            // kein "pending kb interrupt"
                    m_InterruptEventsId,
                    EMU_INTPENDING_VBL);
*/
            m_bInterruptPending = true;
            m_bWaitEmulatorForIRQCallback = true;
#if defined(USE_ASGARD_PPC_68K_EMU)
            Asgard68000SetIRQLine(k68000IRQLineIRQ4, k68000IRQStateAsserted);
#else
            m68k_set_irq(M68K_IRQ_4);
#endif
            while(m_bInterruptPending)
            {
#if defined(USE_ASGARD_PPC_68K_EMU)
                Asgard68000Execute();        // warte bis IRQ-Callback
#else
                m68k_execute();        // warte bis IRQ-Callback
#endif
//                CDebug::DebugInfo("CMagiC::Exec() --- m_bInterruptVBLPending => %d 68k-Zyklen", CyclesExecuted);
            }
        }

        // ggf. Druckdatei abschließen

        if (*((uint32_t *)(mem68k +_hz_200)) - CMagiCPrint::s_LastPrinterAccess > 200 * 10)
        {
            CMagiCPrint::ClosePrinterFile();
        }
    }    // for


  end_of_thread:

    // Main Task mitteilen, daß der Emulator-Thread beendet wurde
    pTheMagiC->m_bEmulatorHasEnded = true;
//    SendMessageToMainThread(true, kHICommandQuit);        // veraltet?

    m_bEmulatorIsRunning = false;
    return 0;
}


/**********************************************************************
*
* (statisch) IRQ-Callback.
*
**********************************************************************/

#if defined(USE_ASGARD_PPC_68K_EMU)
int CMagiC::IRQCallback(int IRQLine, void *thisPtr)
{
    CMagiC *cm = (CMagiC *) thisPtr;
    // Interrupt-Leitungen zurücksetzen
    Asgard68000SetIRQLine(IRQLine, k68000IRQStateClear);
    // Verarbeitung bestätigen
    cm->m_bInterruptPending = false;
    if (cm->m_bWaitEmulatorForIRQCallback)
        Asgard68000SetExitImmediately();
    return 0;
}
#else
int CMagiC::IRQCallback(int IRQLine)
{
    CMagiC *cm = (CMagiC *) pTheMagiC;
    // Interrupt-Leitungen zurücksetzen
    m68k_clear_irq(IRQLine);
    //Asgard68000SetIRQLine(IRQLine, k68000IRQStateClear);
    // Verarbeitung bestätigen
    cm->m_bInterruptPending = false;
    if (cm->m_bWaitEmulatorForIRQCallback)
        m68k_StopExecution();

    if (IRQLine == M68K_IRQ_5)
        return 69;        // autovector
    else
    if (IRQLine == M68K_IRQ_6)
        return 70;        // autovector
    else

    // dieser Rückgabewert sollte die Interrupt-Anforderung löschen
    return M68K_INT_ACK_AUTOVECTOR;
}
#endif


/**********************************************************************
*
* privat: Freien Platz in Tastaturpuffer ermitteln
*
**********************************************************************/

int CMagiC::GetKbBufferFree( void )
{
    int nCharsInBuffer;

    nCharsInBuffer = (m_pKbRead <= m_pKbWrite) ?
                            (m_pKbWrite - m_pKbRead) :
                            (KEYBOARDBUFLEN - (m_pKbRead - m_pKbWrite));
    return(KEYBOARDBUFLEN - nCharsInBuffer - 1);
}

/**********************************************************************
*
* Called from both SDL event loop thread and emulation thread.
* Make sure to before call OS_EnterCriticalRegion(&m_KbCriticalRegionId);
* and afterwards OS_ExitCriticalRegion(&m_KbCriticalRegionId);
*
* privat: Zeichen in Tastaturpuffer einfügen.
*
* Es muß VORHER sichergestellt werden, daß genügend Platz ist.
*
**********************************************************************/

void CMagiC::PutKeyToBuffer(unsigned char key)
{
    *m_pKbWrite++ = key;
    if (m_pKbWrite >= m_cKeyboardOrMouseData + KEYBOARDBUFLEN)
    {
        m_pKbWrite = m_cKeyboardOrMouseData;
    }
}


/**********************************************************************
*
* Busfehler melden (wird im Emulator-Thread aufgerufen).
*
* <addr> ist im Host-Format, d.h. "little endian" auf x86
*
**********************************************************************/

void CMagiC::SendBusError(uint32_t addr, const char *AccessMode)
{
#if defined(USE_ASGARD_PPC_68K_EMU)
    Asgard68000SetExitImmediately();
#else
    m68k_StopExecution();
#endif
    m_bBusErrorPending = true;
    m_BusErrorAddress = addr;
    strcpy(m_BusErrorAccessMode, AccessMode);
}


/**********************************************************************
*
* Das Programm soll beendet werden, dazu soll sich der Emulator
* sauber, d.h. über Shutdown beenden.
*
* Wird im "main thread" aufgerufen
*
**********************************************************************/

void CMagiC::SendShutdown(void)
{
    m_bShutdown = true;
}


/** **********************************************************************************************
 *
 * @brief Send key press from event loop to the emulated machine
 *
 * @param[in]  sdlScanCode      SDL key scan code
 * @param[in]  KeyUp            true: up, false: down
 *
 * @return non-zero, if keyboard buffer is full
 *
 * @note Called from main event loop
 *
 ************************************************************************************************/
int CMagiC::SendSdlKeyboard(int sdlScanCode, bool keyUp)
{
    unsigned char val;


#ifndef NDEBUG
    if (CMagiC__sNoAtariInterrupts)
    {
        return 0;
    }
#endif
#ifdef _DEBUG_NO_ATARI_KB_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        //    CDebug::DebugInfo("CMagiC::SendKeyboard() --- message == %08x, keyUp == %d", message, (int) keyUp);

#ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendKeyboard() --- Enter critical region m_KbCriticalRegionId");
#endif
        OS_EnterCriticalRegion(&m_KbCriticalRegionId);
        if (GetKbBufferFree() < 1)
        {
            OS_ExitCriticalRegion(&m_KbCriticalRegionId);
#ifdef _DEBUG_KB_CRITICAL_REGION
            CDebug::DebugInfo("CMagiC::SendKeyboard() --- Exited critical region m_KbCriticalRegionId");
#endif
            DebugError2("() -- keyboard buffer full. Ignore key press");
            return 1;
        }

        // Convert from SDL to Atari scancode

        val = CMagiCKeyboard::SdlScanCode2AtariScanCode(sdlScanCode);
        if (!val)
        {
            OS_ExitCriticalRegion(&m_KbCriticalRegionId);
#ifdef _DEBUG_KB_CRITICAL_REGION
            CDebug::DebugInfo("CMagiC::SendKeyboard() --- Exited critical region m_KbCriticalRegionId");
#endif
            DebugError2("() -- unknown key. Ignore key press");
            return 0;
        }

        if (keyUp)
        {
            val |= 0x80;
        }

        PutKeyToBuffer(val);

        // interrupt vector 70 for keyboard/MIDI

        m_bInterruptMouseKeyboardPending = true;
#if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000SetExitImmediately();
#else
        m68k_StopExecution();
#endif

        OS_SetEvent(            // wake up if idle
                    &m_InterruptEventsId,
                    EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
#ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendKeyboard() --- Exited critical region m_KbCriticalRegionId");
#endif
    }

    return 0;    // OK
}


#if 0
/**********************************************************************
*
* Status von Shift/Alt/Ctrl schicken.
*
* Wird von der "main event loop" aufgerufen.
* Löst einen Interrupt 6 aus.
*
* Rückgabe != 0, wenn die letzte Nachricht noch aussteht.
*
**********************************************************************/

int CMagiC::SendKeyboardShift( uint32_t modifiers )
{
    unsigned char val;
    bool bAutoBreak;
    bool done = false;
    int nKeys;


#ifndef NDEBUG
    if (CMagiC__sNoAtariInterrupts)
    {
        return 0;
    }
#endif
#ifdef _DEBUG_NO_ATARI_KB_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {

        // Emulation der rechten Maustaste mit der linken und gedrückter Cmd-Taste
    /*
        if ((!Preferences::KeyCodeForRightMouseButton) &&
             (m_CurrModifierKeys & cmdKey) != (modifiers & cmdKey))
        {
            // linken Mausknopf immer loslassen
            SendMouseButton(1, false);
        }
    */
        m_CurrModifierKeys = modifiers;
    #ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendKeyboardShift() --- Enter critical region m_KbCriticalRegionId");
    #endif
        OS_EnterCriticalRegion(m_KbCriticalRegionId, kDurationForever);
        for    (;;)
        {

            // Umrechnen in Atari-Scancode und abschicken

            val = m_MagiCKeyboard.GetModifierScanCode(modifiers, &bAutoBreak);
            if (!val)
                break;        // unbekannte Taste oder schon gedrückt

            nKeys = (bAutoBreak) ? 2 : 1;
            if (GetKbBufferFree() < nKeys)
            {
                OS_ExitCriticalRegion(m_KbCriticalRegionId);
    #ifdef _DEBUG_KB_CRITICAL_REGION
                CDebug::DebugInfo("CMagiC::SendKeyboardShift() --- Exited critical region m_KbCriticalRegionId");
    #endif
                DebugError("CMagiC::SendKeyboardShift() --- Tastenpuffer ist voll");
                return(1);
            }

    //        CDebug::DebugInfo("CMagiC::SendKeyboardShift() --- val == 0x%04x", (int) val);
            PutKeyToBuffer(val);
            // Bei CapsLock wird der break code automatisch mitgeschickt
            if (bAutoBreak)
                PutKeyToBuffer((unsigned char) (val | 0x80));

            done = true;
        }

        if (done)
        {
            m_bInterruptMouseKeyboardPending = true;
    #if defined(USE_ASGARD_PPC_68K_EMU)
            Asgard68000SetExitImmediately();
    #else
            m68k_StopExecution();
    #endif
        }

        OS_SetEvent(            // aufwecken, wenn in "idle task"
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(m_KbCriticalRegionId);
    #ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendKeyboardShift() --- Exited critical region m_KbCriticalRegionId");
    #endif
    }

    return 0;    // OK
}
#endif


/**********************************************************************
*
* Mausposition schicken (Atari-Bildschirm-relativ).
*
* Je nach Compiler-Schalter:
* -    Wenn EVENT_MOUSE definiert ist, wird die Funktion von der
*    "main event loop" aufgerufen und löst beim Emulator
*    einen Interrupt 6 aus.
* -    Wenn EVENT_MOUSE nicht definiert ist, wird die Funktion im
*    200Hz-Interrupt (mit 50 Hz) aufgerufen.
*
* Rückgabe != 0, wenn die letzte Nachricht noch aussteht.
*
**********************************************************************/

int CMagiC::SendMousePosition(int x, int y)
{
#ifndef NDEBUG
    if (CMagiC__sNoAtariInterrupts)
    {
        return 0;
    }
#endif
#ifdef _DEBUG_NO_ATARI_MOUSE_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;

    #ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendMousePosition() --- Enter critical region m_KbCriticalRegionId");
    #endif
        OS_EnterCriticalRegion(&m_KbCriticalRegionId);
        m_InterruptMouseWhere.x = (short) x;
        m_InterruptMouseWhere.y = (short) y;
        m_bInterruptMouseKeyboardPending = true;
    #if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000SetExitImmediately();
    #else
        m68k_StopExecution();
    #endif

        // wake up emulator, if in "idle task"
        OS_SetEvent(
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    #ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendMousePosition() --- Exited critical region m_KbCriticalRegionId");
    #endif
    }

    return 0;    // OK
}


/**********************************************************************
*
* Mausknopf schicken.
*
* Wird von der "main event loop" aufgerufen.
* Löst einen Interrupt 6 aus.
*
* Rückgabe != 0, wenn die letzte Nachricht noch aussteht.
*
**********************************************************************/

int CMagiC::SendMouseButton(unsigned int NumOfButton, bool bIsDown)
{
#ifndef NDEBUG
    if (CMagiC__sNoAtariInterrupts)
    {
        return 0;
    }
#endif
#ifdef _DEBUG_NO_ATARI_MOUSE_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        if (NumOfButton > 1)
        {
            DebugWarning("CMagiC::SendMouseButton() --- Mausbutton %d nicht unterstützt", NumOfButton + 1);
            return 1;
        }

    #ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendMouseButton() --- Enter critical region m_KbCriticalRegionId");
    #endif
        OS_EnterCriticalRegion(&m_KbCriticalRegionId);
#if 0
        if (!Preferences::KeyCodeForRightMouseButton)
        {

            // Emulation der rechten Maustaste mit der linken und gedrückter Cmd-Taste

            if (bIsDown)
            {
                if (m_CurrModifierKeys & cmdKey)
                {
                    // linke Maustaste gedrückt mit gedrückter Cmd-Taste:
                    // rechte, nicht linke
                    m_bInterruptMouseButton[0] = false;
                    m_bInterruptMouseButton[1] = true;
                }
                else
                {
                    // linke Maustaste gedrückt ohne gedrückter Cmd-Taste:
                    // linke, nicht rechte
                    m_bInterruptMouseButton[0] = true;
                    m_bInterruptMouseButton[1] = false;
                }
            }
            else
            {
                m_bInterruptMouseButton[0] = false;
                m_bInterruptMouseButton[1] = false;
            }
        }
        else
#endif
        {
            m_bInterruptMouseButton[NumOfButton] = bIsDown;
        }

        m_bInterruptMouseKeyboardPending = true;
    #if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000SetExitImmediately();
    #else
        m68k_StopExecution();
    #endif

        // wake up emulator, if in "idle task"
        OS_SetEvent(
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    #ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::SendMouseButton() --- Exited critical region m_KbCriticalRegionId");
    #endif
    }

    return 0;    // OK
}


/**********************************************************************
*
* 200Hz-Timer auslösen.
*
* Wird im Interrupt aufgerufen!
*
* Löst einen 68k-Interrupt 5 aus (abweichend vom Atari, da ist es Interrupt 6,
* der Interruptvektor 69 ist aber derselbe).
*
**********************************************************************/

int CMagiC::SendHz200(void)
{
#ifndef NDEBUG
    if (CMagiC__sNoAtariInterrupts)
    {
        return 0;
    }
#endif
#ifdef _DEBUG_NO_ATARI_HZ200_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        /*
         is of no use, because guest calls: "jsr v_clswk" to close VDI, no more redraws!

        if (m_AtariShutDownDelay)
        {
            // delayed shutdown
            m_AtariShutDownDelay--;
            //atomic_store(p_bVideoBufChanged, true);
            if (!m_AtariShutDownDelay)
            {
                DebugInfo("CMagiC::SendHz200() -- execute delayed shutdown");

                // Emulator-Thread anhalten
                pTheMagiC->m_bCanRun = false;
                //    pTheMagiC->m_bAtariWasRun = false;    (entf. 4.11.07)

                // setze mir selbst einen Event zum Beenden (4.11.07)
                &OS_SetEvent(
                            pTheMagiC->m_EventId,
                            EMU_EVNT_TERM);
                return 0;
            }
        }
         */

        m_bInterrupt200HzPending = true;
#if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000SetExitImmediately();
#else
        m68k_StopExecution();
#endif

        // wake up emulator, if in "idle task"
        OS_SetEvent(
            &m_InterruptEventsId,
            EMU_INTPENDING_200HZ);
    }
    return 0;    // OK
}


/**********************************************************************
*
* VBL auslösen.
*
* Wird im Interrupt aufgerufen!
*
* Löst einen 68k-Interrupt 4 aus.
*
**********************************************************************/

int CMagiC::SendVBL(void)
{
#ifndef NDEBUG
    if (CMagiC__sNoAtariInterrupts)
    {
        return 0;
    }
#endif

#ifdef _DEBUG_NO_ATARI_VBL_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        m_bInterruptVBLPending = true;
#if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000SetExitImmediately();
#else
        m68k_StopExecution();
#endif

        // wake up emulator, if in "idle task"
        OS_SetEvent(
            &m_InterruptEventsId,
            EMU_INTPENDING_VBL);
    }
    return 0;    // OK
}


/**********************************************************************
*
* Callback des Emulators: Erste Initialisierung beendet
*
**********************************************************************/

uint32_t CMagiC::AtariInit(uint32_t params, uint8_t *addrOffset68k)
{
    DebugInfo2("() - ATARI: First initialisation phase done.");
    (void) params;
    (void) addrOffset68k;
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Atari-BIOS initialisiert
*
**********************************************************************/

uint32_t CMagiC::AtariBIOSInit(uint32_t params, uint8_t *addrOffset68k)
{
    DebugInfo2("() - ATARI: BIOS initialisation done.");
    (void) params;
    (void) addrOffset68k;
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: VDI initialisiert
*
**********************************************************************/

uint32_t CMagiC::AtariVdiInit(uint32_t params, uint8_t *addrOffset68k)
{
    DebugInfo2("() - ATARI: VDI initialisation done.");
//    (void) params;
//    (void) addrOffset68k;
    Point PtAtariMousePos;

    m_LineAVars = addrOffset68k + params;
    // Aktuelle Mausposition: Bildschirmmitte
    PtAtariMousePos.x = (short) ((m_pMagiCScreen->m_PixMap.bounds_right - m_pMagiCScreen->m_PixMap.bounds_left) >> 1);
    PtAtariMousePos.y = (short) ((m_pMagiCScreen->m_PixMap.bounds_bottom - m_pMagiCScreen->m_PixMap.bounds_top) >> 1);
    CMagiCKeyboard::init();
    CMagiCMouse::init(m_LineAVars, PtAtariMousePos);

    // Umgehe Fehler in Behnes MagiC-VDI. Bei Bildbreiten von 2034 Pixeln und true colour werden
    // fälschlicherweise 0 Bytes pro Bildschirmzeile berechnet. Bei größeren Bildbreiten werden
    // andere, ebenfalls fehlerhafte Werte berechnet.

#ifdef PATCH_VDI_PPC
    uint16_t *p_linea_BYTES_LIN = (uint16_t *) (m_LineAVars - 2);

    if ((Preferences::bPPC_VDI_Patch) &&
         (CGlobals::s_PhysicalPixelSize == 32) &&
         (CGlobals::s_pixelSize == 32) &&
         (CGlobals::s_pixelSize2 == 32))
    {
        DebugInfo("CMagiC::AtariVdiInit() --- PPC");
//        DebugInfo("CMagiC::AtariVdiInit() --- (LINEA-2) = %u", *((uint16_t *) (m_LineAVars - 2)));
// Hier die Atari-Bildschirmbreite in Bytes eintragen, Behnes VDI kriegt hier ab 2034 Pixel Bildbreite
// immer Null raus, das führt zu Schrott.
//        *((uint16_t *) (m_LineAVars - 2)) = htobe16(8136);    // 2034 * 4
        patchppc(addrOffset68k);
    }
#endif
    return 0;
}


/**********************************************************************
*
* 68k-Code im PPC-Code ausführen.
* params = {pc,sp,arg}        68k-Programm ausführen
* params = NULL            zurück in den PPC-Code
*
**********************************************************************/

uint32_t CMagiC::AtariExec68k(uint32_t params, uint8_t *addrOffset68k)
{
    char Old68kContext[128];
    uint32_t ret;
    struct New68Context
    {
        uint32_t regPC;
        uint32_t regSP;
        uint32_t arg;
    } __attribute__((packed));
    New68Context *pNew68Context = (New68Context *) (addrOffset68k + params);

    if (!pNew68Context)
    {
        if (!m_bSpecialExec)
        {
            DebugError2("() --- Kann speziellen Modus nicht beenden");
            return(0xffffffff);
        }
        // speziellen Modus beenden
        m_bSpecialExec = false;
#if defined(USE_ASGARD_PPC_68K_EMU)
        Asgard68000SetExitImmediately();
#else
        m68k_StopExecution();
#endif
        return 0;
    }

#if defined(USE_ASGARD_PPC_68K_EMU)
    if (Asgard68000GetContext(NULL) > 1024)
#else
    if (m68k_context_size() > 1024)
#endif
    {
        DebugError("CMagiC::AtariExec68k() --- Kontext zu groß");
        return(0xffffffff);
    }

    // alten 68k-Kontext retten
#if defined(USE_ASGARD_PPC_68K_EMU)
    (void) Asgard68000GetContext(Old68kContext);
#else
    (void) m68k_get_context(Old68kContext);
#endif
    // PC und sp setzen
#if defined(USE_ASGARD_PPC_68K_EMU)
    Asgard68000Reset();
    Asgard68000SetReg(k68000RegisterIndexPC, pNew68Context->regPC);
    Asgard68000SetReg(k68000RegisterIndexSP, pNew68Context->regSP);
    Asgard68000SetReg(k68000RegisterIndexA0, pNew68Context->arg);
    Asgard68000SetReg(k68000RegisterIndexSR, 0x2700);

    // 68k im PPC im 68k ausführen
    m_bSpecialExec = true;
    while(m_bSpecialExec)
        Asgard68000Execute();
    // alles zurück
    ret = Asgard68000GetReg(k68000RegisterIndexD0);
    (void) Asgard68000SetContext(Old68kContext);
#else
    m68k_pulse_reset();
    m68k_set_reg(M68K_REG_PC, be32toh(pNew68Context->regPC));
    m68k_set_reg(M68K_REG_SP, be32toh(pNew68Context->regSP));
    m68k_set_reg(M68K_REG_A0, be32toh(pNew68Context->arg));
    m68k_set_reg(M68K_REG_SR, 0x2700);

    // 68k im PPC im 68k ausführen
    m_bSpecialExec = true;
    while(m_bSpecialExec)
        m68k_execute();
    // alles zurück
    ret = m68k_get_reg(NULL, M68K_REG_D0);
    (void) m68k_set_context(Old68kContext);
#endif
    return ret;
}


/**********************************************************************
*
* Callback des Emulators: DOS-Funktionen 0x60-0xfe
*
**********************************************************************/

uint32_t CMagiC::AtariDOSFn(uint32_t params, uint8_t *addrOffset68k)
{
    struct AtariDOSFnParm
    {
        uint16_t dos_fnr;
        uint32_t parms;
    } __attribute__((packed));

#if defined(_DEBUG)
    AtariDOSFnParm *theAtariDOSFnParm = (AtariDOSFnParm *) (addrOffset68k + params);
    DebugInfo("CMagiC::AtariDOSFn(fn = 0x%x)", be16toh(theAtariDOSFnParm->dos_fnr));
#endif
    return (uint32_t) EINVFN;
}


/**********************************************************************************************//**
*
* @brief Emulator callback: XBIOS Gettime
*
* @param[in] params
* @param[in] addrOffset68k
*
* @return bits [0:4]=two-seconds [5:10]=min [11:15]=h [16:20]=day [21:24]=month [25-31]=y-1980
*
**************************************************************************************************/

uint32_t CMagiC::AtariGettime(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    time_t t = time(nullptr);
    struct tm tm;
    (void) localtime_r(&t, &tm);

    return (tm.tm_sec >> 1) |
           (tm.tm_min << 5) |
           (tm.tm_hour << 11) |
           (tm.tm_mday << 16) |
           (tm.tm_mon << 21) |
           ((tm.tm_year) - 80) << 25;
}


/**********************************************************************
*
* Callback des Emulators: XBIOS Settime
*
**********************************************************************/

uint32_t CMagiC::AtariSettime(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
    DebugWarning2("() -- setting of host clock is not supported, yet");
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: XBIOS Setpalette
*
**********************************************************************/

uint32_t CMagiC::AtariSetpalette(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
    DebugWarning2("() -- not supported");
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: XBIOS Setcolor
*
**********************************************************************/

uint32_t CMagiC::AtariSetcolor(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
    DebugWarning2("() -- not supported");
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: XBIOS VsetRGB
*
**********************************************************************/

uint32_t CMagiC::AtariVsetRGB(uint32_t params, uint8_t *addrOffset68k)
{
    int i,j;
    uint32_t c;
    uint32_t *pColourTable;
    struct VsetRGBParm
    {
        uint16_t index;
        uint16_t cnt;
        uint32_t pValues;
    } __attribute__((packed));

    VsetRGBParm *theVsetRGBParm = (VsetRGBParm *) (addrOffset68k + params);
    const uint8_t *pValues = (const uint8_t *) (addrOffset68k + be32toh(theVsetRGBParm->pValues));
    uint16_t index = be16toh(theVsetRGBParm->index);
    uint16_t cnt = be16toh(theVsetRGBParm->cnt);
    DebugInfo("CMagiC::AtariVsetRGB(index=%u, cnt=%u, 0x%02x%02x%02x%02x)",
              (unsigned) index, (unsigned) cnt,
              (unsigned) pValues[0], (unsigned) pValues[1], (unsigned) pValues[2], (unsigned) pValues[3]);

    // durchlaufe alle zu ändernden Farben
    pColourTable = pTheMagiC->m_pMagiCScreen->m_pColourTable;
    j = MIN(MAGIC_COLOR_TABLE_LEN, index + cnt);
    for    (i = index, pColourTable += index;
        i < j;
        i++, pValues += 4,pColourTable++)
    {
        // Atari: 00rrggbb
        // 0xff000000        black
        // 0xffff0000        red
        // 0xff00ff00        green
        // 0xff0000ff        blue
        c = (pValues[1] << 16) | (pValues[2] << 8) | (pValues[3] << 0) | (0xff000000);
        *pColourTable++ = c;
    }

    atomic_store(&gbAtariVideoBufChanged, true);

    return 0;
}


/**********************************************************************
*
* Callback des Emulators: XBIOS VgetRGB
*
**********************************************************************/

uint32_t CMagiC::AtariVgetRGB(uint32_t params, uint8_t *addrOffset68k)
{
    int i,j;
    uint32_t *pColourTable;
    struct VgetRGBParm
    {
        uint16_t index;
        uint16_t cnt;
        uint32_t pValues;
    } __attribute__((packed));

    VgetRGBParm *theVgetRGBParm = (VgetRGBParm *) (addrOffset68k + params);
    uint8_t *pValues = (uint8_t *) (addrOffset68k + be32toh(theVgetRGBParm->pValues));
    uint16_t index = be16toh(theVgetRGBParm->index);
    uint16_t cnt = be16toh(theVgetRGBParm->cnt);
    DebugInfo("CMagiC::AtariVgetRGB(index=%d, cnt=%d)", index, cnt);

    // durchlaufe alle zu ändernden Farben
    pColourTable = pTheMagiC->m_pMagiCScreen->m_pColourTable;
    j = MIN(MAGIC_COLOR_TABLE_LEN, index + cnt);
    for (i = index, pColourTable += index;
        i < j;
        i++, pValues++, pColourTable++)
    {
#if 0//SDL_BYTEORDER == SDL_BIG_ENDIAN
        pValues[0] = 0;
        pValues[1] = (*pColourTable) >> 24;
        pValues[2] = (*pColourTable) >> 16;
        pValues[3] = (*pColourTable) >> 8;
        //        rmask = 0xff000000;
        //        gmask = 0x00ff0000;
        //        bmask = 0x0000ff00;
        //        amask = 0x000000ff;
#else
        pValues[0] = 0;
        pValues[1] = (*pColourTable) >> 0;
        pValues[2] = (*pColourTable) >> 8;
        pValues[3] = (*pColourTable) >> 16;
        //        rmask = 0x000000ff;
        //        gmask = 0x0000ff00;
        //        bmask = 0x00ff0000;
        //        amask = 0xff000000;
#endif
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Convert Atari drive to device code TODO: This is not part of XFS, move to CMagiC!
 *
 * @param[in] params            68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return zero or non-negative device code
 *
 * @note Used by the Atari to eject the correct medium for the respective virtual drive.
 *       Not really useful for Linux host.
 *
 ************************************************************************************************/
uint32_t CMagiC::Drv2DevCode(uint32_t params, uint8_t *addrOffset68k)
{
    uint16_t drv = getAtariBE16(addrOffset68k + params);
    uint32_t aret;

    if (m_HostXFS.isDrvValid(drv))
    {
        aret = (uint32_t) 0x00010000 | (drv + 1);
    }
    else
    if (CVolumeImages::isDrvValid(drv))
    {
        aret = (uint32_t) 0x00020000 | (drv + 1);
    }
    else
    {
        aret = 0;
    }

    DebugInfo2("(drv = %u) => 0x%08x", drv, aret);
    return aret;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Device operations. TODO: This is not part of XFS, move to CMagiC!
 *
 * @param[in] params            68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return zero or negative error code
 *
 * @note Used by the Atari to eject a medium. Not really useful for Linux host.
 *
 ************************************************************************************************/
uint32_t CMagiC::RawDrvr(uint32_t param, uint8_t *addrOffset68k)
{
    struct sparams
    {
        UINT16 opcode;
        UINT32 device;
    } __attribute__((packed));

    sparams *params = (sparams *) (addrOffset68k + param);
    uint16_t opcode = be16toh(params->opcode);
    uint32_t device = be32toh(params->device);

    DebugInfo2("(cmd = %u, device = 0x%08x)", opcode, device);

    if (opcode != 0)
    {
        DebugError2("() -- unsupported opcode %u", opcode);
        return EINVFN;
    }

    uint16_t type = (device >> 16);
    uint16_t drv = (device & 0xffff) - 1;

    if (drv < NDRIVES)
    {
        if (type == 1)
        {
            //
            // Host XFS
            //

            m_HostXFS.eject(drv);
            return E_OK;
        }
        else
        if (type == 2)
        {
            //
            // Volume image
            //

            CVolumeImages::eject(drv);
            return E_OK;
        }
    }

    DebugError2("() -- invalid host device 0x%08x", device);
    return EDRIVE;
}


/**********************************************************************
*
* (STATIC) Nachricht (a)synchron an Haupt-Thread schicken
*
**********************************************************************/

void CMagiC::SendMessageToMainThread(bool bAsync, uint32_t command)
{
    (void) bAsync;
    (void) command;
    DebugWarning2("() -- not supported, yet");

#if 0
    EventRef ev;
    HICommand commandStruct;

    CreateEvent(
            NULL,
            kEventClassCommand,
            kEventProcessCommand,
            GetCurrentEventTime(),
            kEventAttributeNone,
            &ev);

    commandStruct.attributes = 0;
    commandStruct.menu.menuRef = 0;
    commandStruct.menu.menuItemIndex = 0;
    commandStruct.commandID = command;

    SetEventParameter(
            ev,
            kEventParamDirectObject,
            typeHICommand,            // gewünschter Typ
            sizeof(commandStruct),        // max. erlaubte Länge
            (void *) &commandStruct
            );

    if (bAsync)
        PostEventToQueue(GetMainEventQueue(), ev, kEventPriorityStandard);
    else
        SendEventToApplication(ev);
#endif
}


// try to mount image as drive A:
// called from main thread
// TODO: add semaphore
bool CMagiC::sendDragAndDropFile(const char *allocated_path)
{
    uint16_t drv = 'A' - 'A';
    if (!m_HostXFS.isDrvValid(drv) && !CVolumeImages::isDrvValid(drv))
    {
        // drive A: currently unused. Mount image or path.
        struct stat statbuf;
        if (stat(allocated_path, &statbuf) == 0)
        {
            mode_t ftype = (statbuf.st_mode & S_IFMT);
            if (ftype == S_IFDIR)
            {
                showAlert("Host directory mounted as A:", allocated_path, 1);
                m_HostXFS.setNewDrv(drv, allocated_path, true, true);
                return true;
            }
            else
            if (ftype == S_IFREG)
            {
                showAlert("Volume image (hopefully) mounted as A:", allocated_path, 1);
                CVolumeImages::setNewDrv(drv, allocated_path, true, true, statbuf.st_size);
                return true;
            }
        }
    }
    return false;
}

/**********************************************************************
*
* Callback des Emulators: System aufgrund eines fatalen Fehlers anhalten
*
**********************************************************************/

uint32_t CMagiC::AtariSysHalt(uint32_t params, uint8_t *addrOffset68k)
{
    char *errMsg = (char *) (addrOffset68k + params);

    DebugError2("() -- %s", errMsg);

// Daten werden getrennt von der Nachricht geliefert

    showAlert("The emulator was halted", errMsg, 1);
    pTheMagiC->StopExec();
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Fehler ausgeben (68k-Exception)
*
**********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif
    #if DEBUG_68K_EMU
    extern void m68k_trace_print(const char *fname);
    #endif
#ifdef __cplusplus
}
#endif

uint32_t CMagiC::AtariSysErr(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    uint32_t act_pd;
    uint32_t m68k_pc;
    const char *AtariPrgFname;

#if 0        // #ifdef PATCH_VDI_PPC
    // patche den Bildschirm(Offscreen-)Treiber
    static int patched = 0;
    if (!patched)
    {
        patchppc(addrOffset68k);
        patched = 1;
    }
#endif

    DebugError2("()");

    // FÜR DIE FEHLERSUCHE: GIB DIE LETZTEN TRACE-INFORMATIONEN AUS.
    #if DEBUG_68K_EMU
    m68k_trace_print("68k-trace-dump-syserr.txt");
    _DumpAtariMem("atarimem.bin");
    #endif

    GetActAtariPrg(&AtariPrgFname, &act_pd);
    m68k_pc = be32toh(*((uint32_t *) (addrOffset68k + proc_stk + 2)));

    DebugInfo("CMagiC::AtariSysErr() -- act_pd = 0x%08lx", act_pd);
    DebugInfo("CMagiC::AtariSysErr() -- Prozeßpfad = %s", (AtariPrgFname) ? AtariPrgFname : "<unknown>");
#if defined(_DEBUG)
    if (m68k_pc < mem68kSize - 8)
    {
        uint16_t opcode1 = be16toh(*((uint16_t *) (addrOffset68k + m68k_pc)));
        uint16_t opcode2 = be16toh(*((uint16_t *) (addrOffset68k + m68k_pc + 2)));
        uint16_t opcode3 = be16toh(*((uint16_t *) (addrOffset68k + m68k_pc + 4)));
        DebugInfo("CMagiC::AtariSysErr() -- opcode = 0x%04x 0x%04x 0x%04x", (unsigned) opcode1, (unsigned) opcode2, (unsigned) opcode3);
    }
#endif

    Send68kExceptionData(
                (uint16_t) (addrOffset68k[proc_pc /*0x3c4*/]),        // Exception-Nummer
                pTheMagiC->m_BusErrorAddress,
                pTheMagiC->m_BusErrorAccessMode,
                m68k_pc,                                                            // pc
                be16toh(*((uint16_t *) (addrOffset68k + proc_stk))),        // sr
                be32toh(*((uint32_t *) (addrOffset68k + proc_usp))),        // usp
                (uint32_t *) (addrOffset68k + proc_regs /*0x384*/),                    // Dx (big endian)
                (uint32_t *) (addrOffset68k + proc_regs + 32),                            // Ax (big endian)
                AtariPrgFname,
                act_pd);

    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Kaltstart
*
* Zur Zeit nur Dummy
*
**********************************************************************/

uint32_t CMagiC::AtariColdBoot(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
    DebugInfo2("()");
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Emulator normal beenden
*
**********************************************************************/

uint32_t CMagiC::AtariExit(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    DebugInfo2("()");

#if 0
//    is of no use, because guest calls: "jsr v_clswk" to close VDI, no more redraws!
    // shutdown is done in the 200Hz timer call, delay shutdown for 0,5s to let guest complete the redraw
    pTheMagiC->m_AtariShutDownDelay = 1000;
    DebugInfo("CMagiC::AtariExit() -- delay for %u ticks", pTheMagiC->m_AtariShutDownDelay);
#else
    // FÜR DIE FEHLERSUCHE: GIB DIE LETZTEN TRACE-INFORMATIONEN AUS.
//    m68k_trace_print("68k-trace-dump-exit.txt");

    // Emulator-Thread anhalten
    pTheMagiC->m_bCanRun = false;
//    pTheMagiC->m_bAtariWasRun = false;    (entf. 4.11.07)

    // setze mir selbst einen Event zum Beenden (4.11.07)
    pTheMagiC->OS_SetEvent(
            &pTheMagiC->m_EventId,
            EMU_EVNT_TERM);

    // Nachricht and Haupt-Thread zum Beenden (entf. 4.11.07)
//    SendMessageToMainThread(true, kHICommandQuit);
#ifdef MAGICMACX_DEBUG68K
    for    (register int i = 0; i < 100; i++)
        CDebug::DebugInfo("### VideoRamWriteCounter(%2d) = %d", i, WriteCounters[i]);
#endif
#endif
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Debug-Ausgaben
*
**********************************************************************/

uint32_t CMagiC::AtariDebugOut(uint32_t params, uint8_t *addrOffset68k)
{
    const unsigned char *text = addrOffset68k + params;
    //printf((char *) text);
    DebugInfo2("(%s)", CConversion::textAtari2Host(text));
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Fehler-Alert
*
* zur Zeit nur Dummy
*
**********************************************************************/

uint32_t CMagiC::AtariError(uint32_t params, uint8_t *addrOffset68k)
{
    uint16_t errorCode = be16toh(*((uint16_t *) (addrOffset68k + params)));

    DebugInfo2("(%hd)", errorCode);
    /*
     Das System kann keinen passenden Grafiktreiber finden.

     Installieren Sie einen Treiber, oder wechseln Sie die Bildschirmauflösung unter MacOS, und starten Sie MagiCMacX neu.
     [MagiCMacX beenden]

     The system could not find an appropriate graphics driver.

     Install a driver, or change the monitor resolution resp. colour depth using the system's control panel. Finally, restart  MagiCMacX.
     [Quit MagiCMacX]
     */
    showAlert("The emulated system could not find a suitable video driver", "Review configuration file!", 1);
    pTheMagiC->StopExec();    // fatal error for execution thread
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Idle Task
* params        Zeiger auf uint32_t
* Rückgabe:
*
**********************************************************************/

uint32_t CMagiC::AtariYield(uint32_t params, uint8_t *addrOffset68k)
{
    struct YieldParm
    {
        uint32_t num;
    } __attribute__((packed));
    uint32_t eventFlags;


    // zuerst testen, ob während des letzten Assembler-Befehls gerade
    // Ereignisse eingetroffen sind, die im Interrupt bearbeitet worden
    // sind. Wenn ja, hier nicht warten, sondern gleich weitermachen.

    YieldParm *theYieldParm = (YieldParm *) (addrOffset68k + params);
    if (be32toh(theYieldParm->num))
        return 0;

//    MPYield();

    pTheMagiC->OS_WaitForEvent(
                &pTheMagiC->m_InterruptEventsId,
                &eventFlags);

/*
    if (EventFlags & EMU_EVNT_TERM)
    {
        DebugInfo("CMagiC::EmuThread() -- normaler Abbruch");
        break;    // normaler Abbruch, Thread-Ende
    }
*/
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Tastatur- und Mausdaten abholen
*
**********************************************************************/

uint32_t CMagiC::AtariGetKeyboardOrMouseData(uint32_t params, uint8_t *addrOffset68k)
{
    (void) addrOffset68k;

#ifdef _DEBUG_KB_CRITICAL_REGION
    CDebug::DebugInfo("CMagiC::AtariGetKeyboardOrMouseData() --- Enter critical region m_KbCriticalRegionId");
#endif
    OS_EnterCriticalRegion(&m_KbCriticalRegionId);
    uint32_t ret = m_pKbRead != m_pKbWrite;        // Daten im Puffer?

    // Wenn keine Taste mehr im Puffer => Maus abfragen
    if (!ret)
    {
        // Die Maus wird erst erkannt, wenn VDI initialisiert ist
        char buf[3];
        if (m_LineAVars != nullptr)
        {
            ret = CMagiCMouse::getNewPositionAndButtonState(buf);
        }
        if (ret)
        {
            PutKeyToBuffer((unsigned char) buf[0]);
            PutKeyToBuffer((unsigned char) buf[1]);
            PutKeyToBuffer((unsigned char) buf[2]);
        }
    }

    if (params)
    {
        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
#ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::AtariGetKeyboardOrMouseData() --- Exited critical region m_KbCriticalRegionId");
#endif
        // Taste wurde verarbeitet. Entspricht beim Atari dem Löschen des
        // "interrupt service bit"
//        if (!ret)
//            Asgard68000SetIRQLine(k68000IRQLineIRQ6, k68000IRQStateClear);
        return ret;        // ggf. weitere Interrupts
    }

    if (!ret)
    {
        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
#ifdef _DEBUG_KB_CRITICAL_REGION
        CDebug::DebugInfo("CMagiC::AtariGetKeyboardOrMouseData() --- Exited critical region m_KbCriticalRegionId");
#endif
        DebugError("AtariGetKeyboardOrMouseData() --- Keine Daten");
        return 0;                    // kein Zeichen?
    }

    ret = *m_pKbRead++;
    if (m_pKbRead >= m_cKeyboardOrMouseData + KEYBOARDBUFLEN)
    {
        m_pKbRead = m_cKeyboardOrMouseData;
    }
    #if defined(_DEBUG_KBD_AND_MOUSE)
    DebugInfo("CMagiC::AtariGetKeyboardOrMouseData() - Sende 0x%02x", ret);
    #endif
    OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    #if defined(_DEBUG_KB_CRITICAL_REGION)
    CDebug::DebugInfo("CMagiC::AtariGetKeyboardOrMouseData() --- Exited critical region m_KbCriticalRegionId");
    #endif
    return ret;
}


/**********************************************************************
*
* Callback des Emulators: Programmstart aus Apple-Events abholen
*
**********************************************************************/

uint32_t CMagiC::MmxDaemon(uint32_t params, uint8_t *addrOffset68k)
{
    uint32_t ret;
    struct MmxDaemonParm
    {
        uint16_t cmd;
        uint32_t parm;
    } __attribute__((packed));


    // TOO OFTEN DebugInfo2("()");
    MmxDaemonParm *theMmxDaemonParm = (MmxDaemonParm *) (addrOffset68k + params);

    switch(be16toh(theMmxDaemonParm->cmd))
    {
        // ermittle zu startende Programme/Dateien aus AppleEvent 'odoc'
        case 1:
#if 0
            OS_EnterCriticalRegion(&m_AECriticalRegionId);
            if (m_iNoOfAtariFiles)
            {
                unsigned char *pBuf;
                // Es liegen Anforderungen vor.
                // Zieladresse:
                pBuf = addrOffset68k + be32toh(theMmxDaemonParm->parm);
                // von Quelladresse kopieren
                strcpy((char *) pBuf, m_szStartAtariFiles[m_iOldestAtariFile]);
                ret = E_OK;
                m_iNoOfAtariFiles--;
            }
            else
                ret = (uint32_t) EFILNF;
            OS_ExitCriticalRegion(&m_AECriticalRegionId);
#endif
            break;

        // ermittle shutdown-Status
        case 2:
            ret = (uint32_t) pTheMagiC->m_bShutdown;
            break;

        default:
            ret = (uint32_t) EUNCMD;
    }

    return ret;
}
