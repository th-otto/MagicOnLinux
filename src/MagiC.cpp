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
#include "network.h"
#include "register_model.h"
#include "mem_access_68k.h"
#include "conversion.h"
#include "gui.h"
#include "nf_objs.h"
#include "audio.h"

#if !defined(_DEBUG_MAGIC)
 #undef DebugInfo
 #define DebugInfo(...)
 #undef DebugInfo2
 #define DebugInfo2(...)
#endif


static CMagiC *pTheMagiC = nullptr;


#ifndef NDEBUG
#if defined(M68K_BREAKPOINTS)
uint32_t m68k_breakpoints[M68K_BREAKPOINTS][2];      //  68k address and range, usually 0
#endif

#if defined(M68K_WRITE_WATCHES)
uint32_t m68k_write_watches[M68K_WRITE_WATCHES];
#endif

int do_not_interrupt_68k = 0;       // for debugging
// disable interrupts, essential for debugging
extern "C" {

void int68k_enable(int enable)
{
    do_not_interrupt_68k = !enable;
    sExitImmediately = 0;
}
void print_app(uint32_t addr68k)
{
    const MagiC_APP *app = (MagiC_APP *) (mem68k + addr68k);
    printf(" ap = 0x%08x\n", addr68k);
    printf(" ap_id = %u\n", be16toh(app->ap_id));
    printf(" ap_type = %u\n", be16toh(app->ap_type));
    printf(" ap_dummy1 = %s\n", app->ap_dummy1);
    printf(" ap_status = %u\n", app->ap_status);
    printf(" ap_cmd = %s\n", app->ap_cmd);
    printf(" ap_tai = %s\n", app->ap_tai);
    printf(" ap_ssp = 0x%08x\n", be32toh(app->ap_ssp));
    printf(" ap_pd = 0x%08x\n", be32toh(app->ap_pd));
    printf(" ap_etvterm = 0x%08x\n", be32toh(app->ap_etvterm));
    printf(" ap_stkchk = 0x%08x\n", be32toh(app->ap_stkchk));
}

// post mortem dump. Move to Atari rootfs and analyse with "memexamn c".
void dumpAtariMem()
{
    if (mem68k != nullptr)
    {
        FILE *f = fopen("_SYS_.$$$", "wb");
        if (f != nullptr)
        {
            uint32_t header[2];
            header[0] = htobe32(0x2912);        // mem_root
            header[1] = htobe32(0x2d72);        // ur_pd;
            fwrite(header, 1, sizeof(header), f);
            fwrite(mem68k + 8, 1, mem68kSize - 8, f);
            fclose(f);
        }
    }
}

}
#endif


void sendBusError(uint32_t addr, const char *AccessMode)
{
    pTheMagiC->sendBusError(addr, AccessMode);
}
void sendVBL(void)
{
    pTheMagiC->sendVBL();
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


// 68k emulator callback jump table
#define JUMP_TABLE_LEN 256
union hostcall jump_table[256];    // pointers to functions
unsigned jump_table_len = 0;

void setHostCallback(PTR32_HOST *dest, tfHostCallback callback)
{
    assert(jump_table_len < JUMP_TABLE_LEN);
    dest[0] = jump_table_len;
    jump_table[jump_table_len++].c = callback;
}

static void setMethodCallback(PTR32x4_HOST *dest, tfHostCallbackCpp callback)
{
    assert(jump_table_len < JUMP_TABLE_LEN);
    (*dest)[0] = jump_table_len;
    jump_table[jump_table_len++].cpp = callback;
}


void setCMagiCHostCallback(PTR32x4_HOST *dest4, tfHostCallbackCpp callback)
{
    setMethodCallback(dest4, callback);
}

void setCXCmdHostCallback(PTR32x4_HOST *dest4, tfHostCallbackCpp callback)
{
    setMethodCallback(dest4, callback);
}

void setCHostXFSHostCallback(PTR32x4_HOST *dest4, tfHostCallbackCpp callback)
{
    setMethodCallback(dest4, callback);
}

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
    m_BasePage = nullptr;
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
    m_InterruptMouseWhereY = m_InterruptMouseWhereX = 0;
    m_InterruptMouseMoveRelX = m_InterruptMouseMoveRelY = 0.0;
    m_bInterruptPending = false;
    m_LineAVars = nullptr;
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
 * @brief 68k emulator asks for a message, but does not clear it
 *
 * @param[in,out]  event        bit vector to ask for
 * @param[out]     flags        bit vector with '1' and '0' flags
 *
 * @return masked bit vector with arrived '1' bits
 *
 ************************************************************************************************/
uint32_t CMagiC::OS_AskEvent(const uint32_t *event, uint32_t flags)
{
    return *event & flags;
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


/** **********************************************************************************************
 *
 * @brief Get file size from host path
 *
 * @param[in]  hostPath     file
 *
 * @return file size or -1 on error
 *
 ************************************************************************************************/
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
 * @brief Relocate an Atari executable file, here the MagiC kernel
 *
 * @param[in]  f            Atari exe file
 * @param[in]  file_size    size of the file
 * @param[in]  tbase        host address of TEXT segment
 * @param[in]  exehead      pointer to loaded program file header (big-endian)
 *
 * @return 0 = OK, otherwise error code
 *
 ************************************************************************************************/
int CMagiC::relocate
(
    FILE *f,
    uint32_t file_size,
    uint8_t *tbase,
    const ExeHeader *exehead
)
{
    //
    // seek to relocation table, skip code, data, symbols and header
    //

    uint32_t relocation_table_fpos = be32toh(exehead->slen) +
                                     be32toh(exehead->tlen) +
                                     be32toh(exehead->dlen)+ sizeof(ExeHeader);
    DebugInfo2("() - seek to relocation table, file offset %u\n", relocation_table_fpos);

    int err = fseek(f, relocation_table_fpos, SEEK_SET);
    if (err)
    {
        DebugError2("() - cannot seek to relocation table, file offset %u\n", relocation_table_fpos);
        DebugError2("() - error code %d\n", errno);
        return -1;
    }

    //
    // read first relocation offset, 32-bit
    //

    uint32_t loff;      // offset of first relocation entry
    if (fread(&loff, sizeof(loff), 1, f) != 1)
    {
        return -2;
    }
    loff = be32toh(loff);
    if (loff == 0)
    {
        DebugWarning2("() - No relocation");
        return 0;
    }
    relocation_table_fpos += sizeof(loff);

    //
    // read all relocation data
    //

    size_t RelocBufSize = file_size - relocation_table_fpos;
    uint8_t *relBuf = (uint8_t *) malloc(RelocBufSize + 2);
    if (relBuf == nullptr)
    {
        return -3;
    }
    if (fread(relBuf, RelocBufSize, 1, f) != 1)
    {
        free(relBuf);
        return -4;
    }
    relBuf[RelocBufSize] = 0;    // just to be sure that the table is zero terminated

    //
    // relocate first 32-bit word in Atari code
    //

    uint32_t *tp = (uint32_t *) (tbase + loff);
    *tp = htobe32((uint32_t) (tbase - mem68k) + be32toh(*tp));

    //
    // relocate the remaining
    //

    uint8_t *relp = relBuf;
    while(*relp)
    {
        uint8_t relb = *relp++;
        if (relb == 1)
        {
            tp = (uint32_t *) ((char *) tp + 254);
        }
        else
        {
            tp = (uint32_t *) ((char *) tp + (unsigned char) relb);

            *tp = htobe32((uint32_t) (tbase - mem68k) + be32toh(*tp));
        }
    }

    free(relBuf);
    return 0;
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
 * @retval 0        OK
 * @retval -1       file not found
 * @retval -2       file is corrupt
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
    ExeHeader exehead;
    BasePage *bp = nullptr;


    DebugInfo2("(\"%s\")", path);
    *basePage = nullptr;    // assume failure by default

    int64_t fileSize = getFileSize(path);
    if (fileSize < 0)
    {
        DebugInfo2("() - kernel file does not exist");
        return -1;
    }

    DebugInfo2("() - kernel file size = %ld", fileSize);
    if ((fileSize <= (int64_t) sizeof(ExeHeader)) || (fileSize > 0x1000000))
    {
        DebugError2("() - Invalid kernel file size %ld", fileSize);
        return -2;
    }

    FILE *f = fopen(path, "rb");
    if (f == nullptr)
    {
        DebugError2("() - Cannot open file \"%s\")", path);
        return -1;
    }

    //
    // read PRG Header
    //

    if (fread(&exehead, sizeof(ExeHeader), 1, f) != 1)
    {
        DebugError2("() - MagiC kernel file is too small.");
        fclose(f);
        return -2;
    }

    DebugInfo2("() - Size TEXT = %ld", be32toh(exehead.tlen));
    DebugInfo2("() - Size DATA = %ld", be32toh(exehead.dlen));
    DebugInfo2("() - Size BSS  = %ld", be32toh(exehead.blen));

    uint32_t codlen = be32toh(exehead.tlen) + be32toh(exehead.dlen);
    if (be32toh(exehead.blen) & 1)
    {
        exehead.blen = htobe32(be32toh(exehead.blen) + 1);    // big-endian increment
    }
    uint32_t tpaSize = sizeof(BasePage) + codlen + be32toh(exehead.blen) + stackSize;

    DebugInfo2("() - Size overall, including basepage and stack = 0x%08x (%ld)", tpaSize, tpaSize);

    if (tpaSize > mem68kSize)
    {
        DebugError2("() - Insufficient memory for kernel");
        fclose(f);
        return -2;
    }

    //
    // determine basepage address, 4-byte aligned
    //

    if (reladdr < 0)
    {
        reladdr = (long) (mem68kSize - ((tpaSize + 2) & ~3));
    }
    if (reladdr + tpaSize > mem68kSize)
    {
        DebugError2("() - Invalid load address");
        fclose(f);
        return -3;
    }

    //
    // initialise basepage
    //

    uint8_t *tpaStart = mem68k + reladdr;           // basepage address in host memory
    uint8_t *tbase = tpaStart + sizeof(BasePage);   // TEXT address in host memory
    uint8_t *bbase = tbase + codlen;                // BSS address in host memory

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
            DebugInfo2("() - BasePage[%d] = 0x%02x", i, p[i]);
        }
    }
    #endif

    //
    // read TEXT+DATA from file
    //

    if (fread(tbase, codlen, 1, f) != 1)
    {
        fclose(f);
        return -2;
    }

    //
    // relocate
    //

    if (!be16toh(exehead.relmod))    // we must relocate
    {
        int err = relocate(f, (uint32_t) fileSize, tbase, &exehead);
        if (err)
        {
            fclose(f);
            return -2;
        }
    }

    //
    // clear BSS
    //

    fclose(f);
    memset(bbase, 0, be32toh(exehead.blen));
    *basePage = bp;
    return 0;
}


/**********************************************************************
*
* (INTERN) Initialisierung von Atari68kData.m_VDISetupData
*
**********************************************************************/
void CMagiC::initCookieData(MgMxCookieData *pCookieData)
{
    pCookieData->mgmx_magic     = htobe32(0x4d674d78); /* 'MgMx' */
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
    DebugInfo2("() -- Convert Pixmap to big-endian");

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
        DebugInfo2("() -- k32BGRAPixelFormat => k32ARGBPixelFormat");
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
 * Member function wrappers
 **********************************************************************/

/* CMagiC */
uint32_t CMagiC::thunk_AtariInit(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->AtariInit(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_AtariBIOSInit(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->AtariBIOSInit(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_AtariVdiInit(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->AtariVdiInit(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_AtariExec68k(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->AtariExec68k(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_AtariGetKeyboardOrMouseData(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->AtariGetKeyboardOrMouseData(params, AdrOffset68k);
}

/* HostXFS */
uint32_t CMagiC::thunk_XFSFunctions(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->m_HostXFS.XFSFunctions(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_XFSDevFunctions(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->m_HostXFS.XFSDevFunctions(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_Drv2DevCode(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->Drv2DevCode(params, AdrOffset68k);
}
uint32_t CMagiC::thunk_RawDrvr(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->RawDrvr(params, AdrOffset68k);
}

/* XCmd */
uint32_t CMagiC::thunk_XCmdCommand(uint32_t params, unsigned char *AdrOffset68k)
{
	return pTheMagiC->m_pXCmd->Command(params, AdrOffset68k);
}

uint32_t CMagiC::UndefinedFunction(uint32_t params, unsigned char *AdrOffset68k)
{
	uint32_t m68k_pc = m68k_get_reg(NULL, M68K_REG_PC);
	(void)params;
	(void)AdrOffset68k;
	char *msg;
	asprintf(&msg, "Unset emulator function called at PC = $%08x\n\n%s", m68k_pc, "Review configuration file!");
    showAlert("The emulator was halted", msg);
    free(msg);
    pTheMagiC->stopExec();
    return 0;
}

/** **********************************************************************************************
 *
 * @brief Initialise Atari-to-host callbacks
 *
 * @param[in] pMacXSysHdr   Atari/host inter-communication data
 * @param[in] pXCmd         currently unused
 *
 ************************************************************************************************/
void CMagiC::initHostCallbacks(struct MacXSysHdr *pMacXSysHdr)
{
	jump_table_len = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
	/*
	 * reserve function #0 to detect cases where the kernel
	 * tries to call functions that we did not install
	 */
	jump_table[jump_table_len++].c = UndefinedFunction;
	
    if (be32toh(pMacXSysHdr->MacSysX_verAtari) >= 1)
    {
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_init, AtariInit);
    	setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_biosinit, AtariBIOSInit);
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_Dosound, AtariDosound);
    } else
    {
	    setCMagiCHostCallback(&pMacXSysHdr->MacLinux_init.v0.MacSysX_init, thunk_AtariInit);
	    setCMagiCHostCallback(&pMacXSysHdr->MacLinux_init.v0.MacSysX_biosinit, thunk_AtariBIOSInit);
	}
    if (be32toh(pMacXSysHdr->MacSysX_verAtari) >= 3)
    {
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_Ikbdws, AtariIkbdws);
	}
    setCMagiCHostCallback(&pMacXSysHdr->MacSysX_VdiInit, thunk_AtariVdiInit);
    setCMagiCHostCallback(&pMacXSysHdr->MacSysX_Exec68k, thunk_AtariExec68k);
    setCXCmdHostCallback(&pMacXSysHdr->MacSysX_Xcmd, thunk_XCmdCommand);
    setHostCallback(&pMacXSysHdr->MacSysX_gettime,    AtariGettime);
    setHostCallback(&pMacXSysHdr->MacSysX_settime,    AtariSettime);
    setHostCallback(&pMacXSysHdr->MacSysX_Setscreen,  AtariSetscreen);
    setHostCallback(&pMacXSysHdr->MacSysX_Setpalette, AtariSetpalette);
    setHostCallback(&pMacXSysHdr->MacSysX_Setcolor,   AtariSetcolor);
    setHostCallback(&pMacXSysHdr->MacSysX_VsetRGB,    AtariVsetRGB);
    setHostCallback(&pMacXSysHdr->MacSysX_VgetRGB,    AtariVgetRGB);
    setHostCallback(&pMacXSysHdr->MacSysX_syshalt,    AtariSysHalt);
    setHostCallback(&pMacXSysHdr->MacSysX_syserr,     AtariSysErr);
    setHostCallback(&pMacXSysHdr->MacSysX_coldboot,   AtariColdBoot);
    setHostCallback(&pMacXSysHdr->MacSysX_exit,       AtariExit);
    setHostCallback(&pMacXSysHdr->MacSysX_debugout,   AtariDebugOut);
    setHostCallback(&pMacXSysHdr->MacSysX_error,      AtariError);
    if (be32toh(pMacXSysHdr->MacSysX_verAtari) >= 2)
    {
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_dev_in, AtariBconin);
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_dev_out, AtariBconout);
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_dev_istat, AtariBconstat);
	    setHostCallback(&pMacXSysHdr->MacLinux_init.v1.MacSysX_dev_ostat, AtariBcostat);
    } else
    {
	    setHostCallback(&pMacXSysHdr->MacSysX_resb8,      &CMagiCPrint::AtariPrtOs);
	    setHostCallback(&pMacXSysHdr->MacSysX_resbc,      &CMagiCPrint::AtariPrtIn);
	    setHostCallback(&pMacXSysHdr->MacSysX_resc0,      &CMagiCPrint::AtariPrtOut);
	    setHostCallback(&pMacXSysHdr->MacSysX_rescc,      &CMagiCSerial::AtariSerIs);
	    setHostCallback(&pMacXSysHdr->MacSysX_resd0,      &CMagiCSerial::AtariSerOs);
	    setHostCallback(&pMacXSysHdr->MacSysX_resd4,      &CMagiCSerial::AtariSerIn);
	    setHostCallback(&pMacXSysHdr->MacSysX_resd8,      &CMagiCSerial::AtariSerOut);
	}
    setHostCallback(&pMacXSysHdr->MacSysX_prtouts,    &CMagiCPrint::AtariPrtOutS);
    setHostCallback(&pMacXSysHdr->MacSysX_serconf,    &CMagiCSerial::AtariSerConf);
    setHostCallback(&pMacXSysHdr->MacSysX_SerOpen,    &CMagiCSerial::AtariSerOpen);
    setHostCallback(&pMacXSysHdr->MacSysX_SerClose,   &CMagiCSerial::AtariSerClose);
    setHostCallback(&pMacXSysHdr->MacSysX_SerRead,    &CMagiCSerial::AtariSerRead);
    setHostCallback(&pMacXSysHdr->MacSysX_SerWrite,   &CMagiCSerial::AtariSerWrite);
    setHostCallback(&pMacXSysHdr->MacSysX_SerStat,    &CMagiCSerial::AtariSerStat);
    setHostCallback(&pMacXSysHdr->MacSysX_SerIoctl,   &CMagiCSerial::AtariSerIoctl);
    setCMagiCHostCallback(&pMacXSysHdr->MacSysX_GetKeybOrMouse, thunk_AtariGetKeyboardOrMouseData);
    setHostCallback(&pMacXSysHdr->MacSysX_dos_macfn, AtariDOSFn);
    setCHostXFSHostCallback(&pMacXSysHdr->MacSysX_xfs, thunk_XFSFunctions);
    setCHostXFSHostCallback(&pMacXSysHdr->MacSysX_xfs_dev, thunk_XFSDevFunctions);
    setCMagiCHostCallback(&pMacXSysHdr->MacSysX_drv2devcode, thunk_Drv2DevCode);
    setCMagiCHostCallback(&pMacXSysHdr->MacSysX_rawdrvr, thunk_RawDrvr);
#if defined(MAGICLIN)
    setHostCallback(&pMacXSysHdr->MacSysX_Daemon, MmxDaemon);
    setHostCallback(&pMacXSysHdr->MacSysX_BlockDevice, CVolumeImages::AtariBlockDevice);
    setHostCallback(&pMacXSysHdr->MacSysX_Network, CNetwork::AtariNetwork);
#else
    setCMagiCHostCallback(&pMacXSysHdr->MacSysX_Daemon, thunk_MmxDaemon);
#endif
    setHostCallback(&pMacXSysHdr->MacSysX_Yield, AtariYield);
#pragma GCC diagnostic pop
}


/** **********************************************************************************************
 *
 * @brief Create and initialise the virtual MagiC machine
 *
 * @param[in] pXCmd         currently unused
 *
 * @return non-zero in case of failure
 * @retval -1   kernel file not found
 * @retval -2   corrupt/invalid kernel file
 * @retval -3   kernel file API mismatch
 * @retval -4   unsufficient host memory
 *
 * @note Called once by the emulator thread
 *
 * @note The virtual machine address range starts with interrupt vectors, system variables,
 *       followed by program and data memory managed by GEMDOS. Following is the kernel
 *       basepage (256 bytes) and the kernel itself which can be configured to be write protected.
 *       Finally, and additionally to the e.g. 8 Megabytes ST-RAM, the virtual screen memory is
 *       located. Its size is calculated from dimension and bit depth of the emulated system,
 *       plus some padding to avoid bus errors (The ST has 32768 bytes reserved for video,
 *       from which 32000 are really used).
 *
 ************************************************************************************************/
int CMagiC::init(CXCmd *pXCmd)
{
    int err;
    Atari68kData *pAtari68kData;
    uint32_t AtariMemtop;       // end of GEMDOS memory, start of kernel basepage
    static_assert(sizeof(struct MacXSysHdr) == 0x228, "");


    /*
    * Some debug output of the compiled-in configuration of the emulator
    */

    DebugInfo2("() - MultiThread version for Linux");
#ifdef _DEBUG_WRITEPROTECT_ATARI_OS
    DebugInfo2("() - 68k ROM is write-protected (slows down the emulator a bit)");
#else
    DebugInfo2("() - 68k ROM is not write-protected (makes the emulator a bit faster)");
#endif
#if defined(EMULATE_NULLPTR_BUSERR)
    DebugInfo2("() - 68k access to 0 and 4 in user mode is prohibited (slows down the emulator a bit)");
#else
    DebugInfo2("() - 68k access to 0 and 4 in user mode is ignored (makes the emulator a bit faster)");
#endif
#if defined(_DEBUG_WATCH_68K_VECTOR_CHANGE)
    DebugInfo2("() - Writing to 68k interrupt vectors is logged (slows down the emulator a bit)");
#else
    DebugInfo2("() - Writing to 68k interrupt vectors is not logged (makes the emulator a bit faster)");
#endif
    //DebugInfo2("() - Autostart %s", (Preferences::bAutoStartMagiC) ? "ON" : "OFF");

    /*
    * Shall read the keyboard table, but currently unused
    */

    (void) CMagiCKeyboard::init();

    /*
    * Allocate memory for virtual machine
    */

    mem68kSize = Preferences::AtariMemSize;
    #if 0
    unsigned numVideoLines = CMagiCScreen::m_PixMap.bounds_bottom - CMagiCScreen::m_PixMap.bounds_top;
    unsigned bufferLineLenInBytes = (CMagiCScreen::m_PixMap.rowBytes & 0x3fff);
    memVideo68kSize = bufferLineLenInBytes * numVideoLines;
    #else
    memVideo68kSize = CMagiCScreen::pixels_size;
    #endif
    // Get Atari memory, note that the <memVideo68kSize> virtual screen RAM is allocated later
    // and actually is an SDL surface.
    mem68k = (unsigned char *) malloc(mem68kSize);
    if (mem68k == nullptr)
    {
        return -4;  // out-of-memory
    }

    /*
    * Load MagiC kernel file to virtual machine memory
    */

    err = LoadReloc(Preferences::AtariKernelPath, 0, -1, &m_BasePage);
    if (err)
    {
        return err;     // -1 or -2
    }
    DebugInfo2("() - MagiC kernel loaded and relocated successfully");

    /*
    * Check kernel header for validity and version.
    * The header is located directly behind the kernel basepage, i.e.
    * at the beginning of its TEXT segment.
    */

    struct MacXSysHdr *pMacXSysHdr = (MacXSysHdr *) (m_BasePage + 1);
    if (be32toh(pMacXSysHdr->MacSysX_magic) != 0x4d616743) /* 'MagC' */
    {
        DebugError2("() - magic value mismatch");
        return -2;
    }
    if (be32toh(pMacXSysHdr->MacSysX_len) != sizeof(*pMacXSysHdr))
    {
        DebugError2("() -- Length of struct does not match (header: %u bytes, should be: %u bytes)",
                         be32toh(pMacXSysHdr->MacSysX_len), sizeof(*pMacXSysHdr));
        return -3;
    }
    if (be32toh(pMacXSysHdr->MacSysX_verAtari) > MAGIC_KERNEL_API_VERSION)
    {
        DebugError2("() - Kernel API version mismatch, is %u instead of expected %u.", be32toh(pMacXSysHdr->MacSysX_verAtari), MAGIC_KERNEL_API_VERSION);
        return -3;
    }
    if (be32toh(pMacXSysHdr->MacSysX_verAtari) < MAGIC_KERNEL_API_VERSION)
    {
        fprintf(stderr, "Kernel API version mismatch, is %u instead of expected %u.\nPlease upgrade\n", be32toh(pMacXSysHdr->MacSysX_verAtari), MAGIC_KERNEL_API_VERSION);
    }

    /*
    * Calculate 68k address ranges
    */

    addr68kVideo = mem68kSize;
    DebugInfo2("() - Atari video mode is %s", Preferences::videoModeToString(Preferences::atariScreenColourMode));
    DebugInfo2("() - 68k video memory starts at 68k address 0x%08x and uses %u (0x%08x) bytes.", addr68kVideo, memVideo68kSize, memVideo68kSize);
    addr68kVideoEnd = addr68kVideo + memVideo68kSize;
    DebugInfo2("() - 68k video memory and general memory end is 0x%08x", addr68kVideoEnd);

    /*
    * Initialise Atari system variables
    */

    setAtariBE32(mem68k + phystop,  addr68kVideoEnd);
    setAtariBE32(mem68k + _v_bas_ad, mem68kSize);
    AtariMemtop = ((uint32_t) ((unsigned char *) m_BasePage - mem68k)) - sizeof(Atari68kData);
    setAtariBE32(mem68k + _memtop, AtariMemtop);
    switch(Preferences::atariScreenColourMode)
    {
        case atariScreenMode16ip:
            mem68k[sshiftmd] = 0;   // ST low resolution (320*200*4 interleaved planes)
            break;

        case atariScreenMode4ip:
            mem68k[sshiftmd] = 1;   // ST medium resolution (640*200*2 interleaved planes)
            break;

        case atariScreenMode2:
            mem68k[sshiftmd] = 2;   // ST high resolution (640*400*2)
            break;

        default:
            // does not make much sense here ...
            mem68k[sshiftmd] = 2;   // ST high resolution (640*400*2)
            break;
    }
    setAtariBE16(mem68k +_cmdload, 0);            // boot AES
    setAtariBE16(mem68k +_nflops, 0);             // no floppy disk drives, yet

    // some Atari addresses

    pAtari68kData = (Atari68kData *) (mem68k + AtariMemtop);        // Pixmap inside Atari memory
    pAtari68kData->m_PixMap = CMagiCScreen::m_PixMap;             // copy host Pixmap to Atari memory
    // left and top seem to be ignored, i.e. only  right and bottom are relevant
    pAtari68kData->m_PixMap.baseAddr = (PTR32_BE) addr68kVideo;        // virtual 68k address

    #if !defined(__BIG_ENDIAN__)
    PixmapToBigEndian(&pAtari68kData->m_PixMap);
    #endif

    DebugInfo2("() - basepage address of system = 0x%08lx (68k)", AtariMemtop + sizeof(Atari68kData));
    DebugInfo2("() - address of Atari68kData = 0x%08lx (68k)", AtariMemtop);

    initCookieData(&pAtari68kData->m_CookieData);

    // Fill up members of the host/Atari shared structure, mainly calls from
    // virtual machine to host.

    pMacXSysHdr->MacSysX_pixmap = htobe32((uint32_t) (((uint64_t) &pAtari68kData->m_PixMap) - (uint64_t) mem68k));
    pMacXSysHdr->MacSysX_pMMXCookie = htobe32((uint32_t) (((uint64_t) &pAtari68kData->m_CookieData) - (uint64_t) mem68k));
    pMacXSysHdr->MacSysX_verMac = htobe32(10);       // must be 10, checked by kernel in HOSTBIOS.S, mismatch -> 68k illegal instruction
    pMacXSysHdr->MacSysX_cpu = htobe16(20);          // 68020
    pMacXSysHdr->MacSysX_fpu = htobe16(0);           // no FPU -- yet -- unfortunately
    pMacXSysHdr->MacSysX_PPCAddr = 0;                // on 32-bit host: mem68k
    pMacXSysHdr->MacSysX_VideoAddr = 0x80000000;     // on 32-bit host: CMagiCScreen::m_PixMap.baseAddr

	/*
	 * set callbacks for kernel
	 */
	m_pXCmd = pXCmd;
    initHostCallbacks(pMacXSysHdr);

    // 68k ssp and PC after reset
    setAtariBE32(mem68k + 0, 512*1024);        // 68k stack to 512k
    setAtari32(mem68k + 4, /*big endian*/ pMacXSysHdr->MacSysX_syshdr);

    // Get TOS SYSHDR (official, i.e. not MagiC specific)

    struct SYSHDR *pSysHdr = (SYSHDR *) (mem68k + be32toh(pMacXSysHdr->MacSysX_syshdr));

    // Get addresses for kbshift, kbrepeat and act_pd, later used by emulator

    m_AtariKbData = mem68k + be32toh(pSysHdr->kbshift);
    m_pAtariActPd = (uint32_t *) (mem68k + be32toh(pSysHdr->_run));
    m_HostXFS.setActPdAddr(m_pAtariActPd);
    m_pAtariActAppl = (uint32_t *) (mem68k + be32toh(pMacXSysHdr->MacSysX_act_appl));

    /*
    * Calculate address ranges and checksum of kernel ROM, used for overwrite
    * detection and, if configured, protection.
    */

    uint32_t chksum = 0;
    uint32_t *fromptr = (uint32_t *) (mem68k + be32toh(pMacXSysHdr->MacSysX_syshdr));
    uint32_t *toptr = (uint32_t *) (mem68k + be32toh((uint32_t) m_BasePage->p_tbase) + be32toh(m_BasePage->p_tlen) + be32toh(m_BasePage->p_dlen));

    // start and end of write-protected 68k address range (system ROM)
    addrOsRomStart = be32toh((uint32_t) m_BasePage->p_tbase);
    addrOsRomEnd = be32toh((uint32_t) m_BasePage->p_tbase) + be32toh(m_BasePage->p_tlen) + be32toh(m_BasePage->p_dlen);
    DebugInfo2("() - OS ROM range from 0x%08x..0x%08x (68k)", addrOsRomStart, addrOsRomEnd);
    do
    {
        chksum += htobe32(*fromptr++);
    }
    while(fromptr < toptr);

    setAtariBE32(mem68k + os_chksum, chksum);

    // dump Atari, for debug purposes
    // DumpAtariMem("AtariMemAfterInit.data");

    //
    // Pass all XFS drives to the Atari
    //

    m_HostXFS.activateXfsDrives();

    //
    // Pass all volume images to the Atari
    //

    CVolumeImages::init();

    //
    // Set _drvbits and _nflops accordingly
    // If _nflops < 2, drive U: will suppress B:
    //

    updateDriveBits(mem68k);

    //
    // boot drive might be irrelevant
    //

    setAtariBE16(mem68k + _bootdev, 'C'-'A');    // Atari boot drive C:

    //
    // initialise 68k emulator (Musashi)
    //

    // The 68020 is the most powerful CPU that is supported by Musashi
    m68k_set_cpu_type(M68K_CPU_TYPE_68020);
    m68k_init();
    NFCreate();

    // start 68k emulator

    addrOpcodeROM = mem68k;    // ROM == RAM
    m68k_set_int_ack_callback(IRQCallback);
    m68k_SetBaseAddr(mem68k);
    m68k_SetHiMem(mem68kSize);
    m_bSpecialExec = false;

    // Reset Musashi 68k emulator
    m68k_pulse_reset();

    CRegisterModel::init();
    /*
    // test model
    uint32_t dummy;
    CRegisterModel::read_word(0x0d2155c2, &dummy);
    */

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

int CMagiC::createThread( void )
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
        DebugError2("() : pthread_create() -> %s", strerror(err));
        return -1;
    }

    return 0;
}


/**********************************************************************
*
* Läßt den Ausführungs-Thread loslaufen
* => 0 = OK, sonst = Fehler
*
**********************************************************************/

void CMagiC::startExec( void )
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

void CMagiC::stopExec( void )
{
    m68k_StopExecution();   // leave inner emulation loop
    m_bCanRun = false;        // darf nicht laufen
}


/**********************************************************************
*
* Terminiert den Ausführungs-Thread bei Programm-Ende
*
**********************************************************************/

void CMagiC::terminateThread(void)
{
    DebugInfo2("()");
    //dumpAtariMem();
    #if defined(M68K_TRACE)
        DebugWarning(" == FINAL TRACE ==");
        m68k_trace_print();
        //dumpAtariMem();
    #endif
    OS_SetEvent(
            &m_EventId,
            EMU_EVNT_TERM);
    stopExec();
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
    bool bNewJoystick;


    m_bEmulatorIsRunning = true;

    for (;;)
    {

        while (!m_bCanRun)
        {
            // wir warten darauf, daß wir laufen dürfen

            DebugInfo2("() -- MPWaitForEvent");
            OS_WaitForEvent(
                        &m_EventId,
                        &EventFlags);

            DebugInfo2("() -- MPWaitForEvent done");

            // wir prüfen, ob wir zum Beenden aufgefordert wurden

            if (EventFlags & EMU_EVNT_TERM)
            {
                DebugInfo2("() -- normal break");
                goto end_of_thread;    // normaler Abbruch, Thread-Ende
            }
        }

        // längere Ausführungsphase

        m_bWaitEmulatorForIRQCallback = false;
        m68k_execute();

        // Bildschirmadressen geändert
        if (m_bScreenBufferChanged)
        {
            OS_EnterCriticalRegion(&m_ScrCriticalRegionId);
            m_bScreenBufferChanged = false;
            OS_ExitCriticalRegion(&m_ScrCriticalRegionId);
        }

#ifndef NDEBUG
        if (do_not_interrupt_68k)
        {
            continue;
            // TODO: replace with bit vector
            /*
            m_bBusErrorPending = false;
            m_bInterruptPending = false;
            m_bInterruptMouseKeyboardPending = false;
            m_bInterruptMouseKeyboardPending = false;
            m_bInterrupt200HzPending = false;
            m_bInterruptVBLPending = false;
            */
        }
#endif

        // ausstehende Busfehler bearbeiten
        if (m_bBusErrorPending)
        {
            m68k_exception_bus_error();
            m_bBusErrorPending = false;
        }

        // ausstehende Interrupts bearbeiten
        if (m_bInterruptPending)
        {
            m_bWaitEmulatorForIRQCallback = true;
            do
            {
                m68k_execute();        // warte bis IRQ-Callback
            }
            while(m_bInterruptPending && !OS_AskEvent(&m_EventId, EMU_EVNT_TERM));
        }

        // aufgelaufene Maus-Interrupts bearbeiten

        if (m_bInterruptMouseKeyboardPending)
        {
            OS_EnterCriticalRegion(&m_KbCriticalRegionId);
            if (GetKbBufferFree() < 3)
            {
                DebugError2("() --- Tastenpuffer ist voll");
            }
            else
            {
                /*
                * Mouse buttons
                */

                bNewBstate[0] = CMagiCMouse::setNewButtonState(0, m_bInterruptMouseButton[0]);
                bNewBstate[1] = CMagiCMouse::setNewButtonState(1, m_bInterruptMouseButton[1]);

                /*
                * Mouse movement
                */

                if (Preferences::bRelativeMouse)
                {
                    bNewMpos = CMagiCMouse::setNewMovement(m_InterruptMouseMoveRelX, m_InterruptMouseMoveRelY);
                }
                else
                {
                    bNewMpos = CMagiCMouse::setNewPosition(m_InterruptMouseWhereX, m_InterruptMouseWhereY);
                }

                /*
                * Keyboard
                */

                bNewKey = (m_pKbRead != m_pKbWrite);

                /*
                * Joystick
                */

                bNewJoystick = m_bInterruptJoystickButtons;
                m_bInterruptJoystickButtons = false;

                if (bNewBstate[0] || bNewBstate[1] || bNewMpos || bNewKey || bNewJoystick)
                {
                    // The "no kbd/mouse data" error occurs with 0 0 1 0:
                    // DebugInfo2("() -- ikbd pending = %u %u %u %u", bNewBstate[0], bNewBstate[1], bNewMpos, bNewKey);
                    // Interrupt-Vektor 70 für Tastatur/MIDI mitliefern
                    m_bInterruptPending = true;
                    m68k_set_irq(M68K_IRQ_6);    // autovector interrupt 70
                }
            }
            m_bInterruptMouseKeyboardPending = false;

/*
            errl = MPResetEvent(            // kein "pending kb interrupt"
                    m_InterruptEventsId,
                    EMU_INTPENDING_KBMOUSE);
*/
            OS_ExitCriticalRegion(&m_KbCriticalRegionId);
            m_bWaitEmulatorForIRQCallback = true;
            while(m_bInterruptPending && !OS_AskEvent(&m_EventId, EMU_EVNT_TERM))
            {
                m68k_execute();        // warte bis IRQ-Callback
            }
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
            m68k_set_irq(M68K_IRQ_5);        // autovector interrupt 69
            while(m_bInterruptPending && !OS_AskEvent(&m_EventId, EMU_EVNT_TERM))
            {
                m68k_execute();        // warte bis IRQ-Callback
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
            m68k_set_irq(M68K_IRQ_4);
            while(m_bInterruptPending && !OS_AskEvent(&m_EventId, EMU_EVNT_TERM))
            {
                m68k_execute();        // warte bis IRQ-Callback
            }
        }

        // ggf. Druckdatei abschließen

        if (*((uint32_t *)(mem68k +_hz_200)) - CMagiCPrint::s_LastPrinterAccess > 200 * 10)
        {
            CMagiCPrint::closePrinterFile();
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

void CMagiC::PutKeyToBuffer(uint8_t key)
{
    *m_pKbWrite++ = key;
    if (m_pKbWrite >= m_cKeyboardOrMouseData + KEYBOARDBUFLEN)
    {
        m_pKbWrite = m_cKeyboardOrMouseData;    // wrap
    }
}


/**********************************************************************
*
* Busfehler melden (wird im Emulator-Thread aufgerufen).
*
* <addr> ist im Host-Format, d.h. "little endian" auf x86
*
**********************************************************************/

void CMagiC::sendBusError(uint32_t addr, const char *AccessMode)
{
    m68k_StopExecution();
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

void CMagiC::sendShutdown(void)
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
int CMagiC::sendSdlKeyboard(int sdlScanCode, bool keyUp)
{
    unsigned char val;


#ifdef _DEBUG_NO_ATARI_KB_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        //    CDebug::DebugInfo2("() --- message == %08x, keyUp == %d", message, (int) keyUp);

        OS_EnterCriticalRegion(&m_KbCriticalRegionId);
        if (GetKbBufferFree() < 1)
        {
            OS_ExitCriticalRegion(&m_KbCriticalRegionId);
            DebugError2("() -- keyboard buffer full. Ignore key press");
            return 1;
        }

        // Convert from SDL to Atari scancode

        val = CMagiCKeyboard::SdlScanCode2AtariScanCode(sdlScanCode);
        if (!val)
        {
            OS_ExitCriticalRegion(&m_KbCriticalRegionId);
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
        m68k_StopExecution();

        OS_SetEvent(            // wake up if idle
                    &m_InterruptEventsId,
                    EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    }

    return 0;    // OK
}


/** **********************************************************************************************
 *
 * @brief Get Atari kbshift system variable
 *
 * @return Atari kbshift value
 *
 ************************************************************************************************/
unsigned CMagiC::getKbshift(void)
{
    if (m_bEmulatorIsRunning)
    {
        return m_AtariKbData[0];
    }
    else
    {
        return 0;
    }
}


/** **********************************************************************************************
 *
 * @brief Update Atari kbshift system variable from host
 *
 * @param[in]  atari_kbshift    desired state of the Atari system variable kbshift
 *
 * @note Called from main event loop, after window focus has been regained, to make sure
 *       that modifier keys and mouse button states do not diverge between host and emulator,
 *       especially CapsLock state of host shall match that of emulated system.
 *
 ************************************************************************************************/
void CMagiC::sendKbshift(uint8_t atari_kbshift)
{
    if (m_bEmulatorIsRunning && (m_AtariKbData[0] != atari_kbshift))
    {
        DebugInfo2("() -- re-synchronise Atari kbshift 0x%02x -> 0x%02x", m_AtariKbData[0], atari_kbshift);
        m_AtariKbData[0] = atari_kbshift;
    }
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
                DebugError2("() -- keyboard buffer is full");
                return(1);
            }

    //        CDebug::DebugInfo2("() --- val == 0x%04x", (int) val);
            PutKeyToBuffer(val);
            // Bei CapsLock wird der break code automatisch mitgeschickt
            if (bAutoBreak)
                PutKeyToBuffer((unsigned char) (val | 0x80));

            done = true;
        }

        if (done)
        {
            m_bInterruptMouseKeyboardPending = true;
            m68k_StopExecution();
        }

        OS_SetEvent(            // aufwecken, wenn in "idle task"
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(m_KbCriticalRegionId);
    }

    return 0;    // OK
}
#endif


/**********************************************************************
*
* Mausposition schicken (Atari-Bildschirm-relativ).
*
* Rückgabe != 0, wenn die letzte Nachricht noch aussteht.
*
**********************************************************************/

int CMagiC::sendMousePosition(int x, int y)
{
#ifdef _DEBUG_NO_ATARI_MOUSE_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;

        OS_EnterCriticalRegion(&m_KbCriticalRegionId);
        m_InterruptMouseWhereX = x;
        m_InterruptMouseWhereY = y;
        m_bInterruptMouseKeyboardPending = true;
        m68k_StopExecution();

        // wake up emulator, if in "idle task"
        OS_SetEvent(
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    }

    return 0;    // OK
}


/**********************************************************************
*
* Mausbewegung schicken
*
* Rückgabe != 0, wenn die letzte Nachricht noch aussteht.
*
**********************************************************************/
int CMagiC::sendMouseMovement(double xrel, double yrel)
{
    if (m_bEmulatorIsRunning)
    {
        OS_EnterCriticalRegion(&m_KbCriticalRegionId);
        m_InterruptMouseMoveRelX = xrel;
        m_InterruptMouseMoveRelY = yrel;
        m_bInterruptMouseKeyboardPending = true;
        m68k_StopExecution();

        // wake up emulator, if in "idle task"
        OS_SetEvent(
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
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

int CMagiC::sendMouseButton(unsigned int NumOfButton, bool bIsDown)
{
#ifdef _DEBUG_NO_ATARI_MOUSE_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        if (NumOfButton > 1)
        {
            DebugWarning2("() -- mouse button %d is not supported", NumOfButton + 1);
            return 1;
        }

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
        m68k_StopExecution();

        // wake up emulator, if in "idle task"
        OS_SetEvent(
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    }

    return 0;    // OK
}


/**********************************************************************
*
* Joystick-Status schicken.
*
* Wird von der "main event loop" aufgerufen.
*
**********************************************************************/

int CMagiC::sendJoystickState(uint8_t header, uint8_t state0, uint8_t state1)
{
    if (m_bEmulatorIsRunning)
    {
        OS_EnterCriticalRegion(&pTheMagiC->m_KbCriticalRegionId);

        pTheMagiC->PutKeyToBuffer(header);    // 0xfd
        pTheMagiC->PutKeyToBuffer(state0);    // joystick 0: bits 0..3: directions, bit 7: fire
        pTheMagiC->PutKeyToBuffer(state1);    // joystick 1: bits 0..3: directions, bit 7: fire

        m_bInterruptJoystickButtons = true;
        m_bInterruptMouseKeyboardPending = true;

        m68k_StopExecution();

        // wake up emulator, if in "idle task"
        OS_SetEvent(
                &m_InterruptEventsId,
                EMU_INTPENDING_KBMOUSE);

        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
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

int CMagiC::sendHz200(void)
{
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
                DebugInfo2("() -- execute delayed shutdown");

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
        m68k_StopExecution();

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

int CMagiC::sendVBL(void)
{
#ifdef _DEBUG_NO_ATARI_VBL_INTERRUPTS
    return 0;
#endif

    if (m_bEmulatorIsRunning)
    {
        m_bInterruptVBLPending = true;
        m68k_StopExecution();

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
#if defined(M68K_BREAKPOINTS)
    m68k_breakpoints[0][0] = 0x7c9bf8 + 0x1F5E;   // VsetRGB
    m68k_breakpoints[0][1] = m68k_breakpoints[0][0] + 16;   // range
#endif
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
#if defined(M68K_BREAKPOINTS)
    //breakpoint = 0x007c9c68 + 0x5cf2;   // Pdkill TODO: remove, if done
    //breakpoint2 = 0x007c9c68 + 0x651c;   // Pexec
    //breakpoint3 = 0x007c9c68 + 0x1e4ee;   // pgml_term
    //breakpoint = 0x007c9c68 + 0x19c96;   // bsr pgm_loader
    //breakpoint2 = 0x007c9c68 + 0x17ee;   // print_bombs10
    //breakpoint2_range = 0x180a - 0x17ee;   // print_bombs10
    //breakpoint3 = 0x007c9c68 + 0x1e2f2;   // pgm_loader
    //m68k_breakpoints[0][0] = 0x007c9bf8 /* system TEXT segment */ + 0x1F5E;   // VsetRGB
#endif
//    (void) params;
//    (void) addrOffset68k;

    m_LineAVars = addrOffset68k + params;
    // Aktuelle Mausposition: Bildschirmmitte
    int AtariMousePosX = ((CMagiCScreen::m_PixMap.bounds_right - CMagiCScreen::m_PixMap.bounds_left) >> 1);
    int AtariMousePosY = ((CMagiCScreen::m_PixMap.bounds_bottom - CMagiCScreen::m_PixMap.bounds_top) >> 1);
    CMagiCKeyboard::init();
    CMagiCMouse::init(m_LineAVars, AtariMousePosX, AtariMousePosY);

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
        m68k_StopExecution();
        return 0;
    }

    if (m68k_context_size() > 1024)
    {
        DebugError2("() -- Context too large");
        return(0xffffffff);
    }

    // alten 68k-Kontext retten
    (void) m68k_get_context(Old68kContext);
    // PC und sp setzen
    m68k_pulse_reset();
    NFReset();
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

    (void) params;
    (void) addrOffset68k;
#if defined(_DEBUG)
    AtariDOSFnParm *theAtariDOSFnParm = (AtariDOSFnParm *) (addrOffset68k + params);
    DebugInfo2("(fn = 0x%x)", be16toh(theAtariDOSFnParm->dos_fnr));
#endif
    return (uint32_t) EINVFN;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Gettime
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return bits [0:4]=two-seconds [5:10]=min [11:15]=h [16:20]=day [21:24]=month [25-31]=y-1980
 *
 * @note For Atari the month value is 1..12, while in Unix it is 0..11.
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariGettime(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    time_t t = time(nullptr);
    uint16_t time;
    uint16_t date;
    CConversion::hostDateToDosDate(t, &time, &date);
    return ((uint32_t) time) | (((uint32_t) date) << 16);
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Settime
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return not supported, always zero
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariSettime(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;
    DebugWarning2("() -- setting of host clock is not supported, yet");
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Setcreen
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return not supported, always zero
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariSetscreen(uint32_t params, uint8_t *addrOffset68k)
{
    struct SetscreenParm
    {
        PTR32_BE log;       // logical screen address
        PTR32_BE phys;      // physical screen address
        UINT16_BE res;      // resolution: 0=ST low, 1=ST mid, 2=ST high
    } __attribute__((packed));

    const SetscreenParm *theParm = (SetscreenParm *) (addrOffset68k + params);
    uint32_t log = be32toh(theParm->log);
    uint32_t phys = be32toh(theParm->phys);
    uint16_t res = be16toh(theParm->res);
    //DebugError2("(0x%08x, 0x%08x, %u) -- not supported", log, phys, res);
    if (log != 0xffffffff)
    {
        if (log == addr68kVideo)
        {
            DebugWarning2("() -- logical screen address reset to 0x%08x", log);
            CMagiCScreen::m_logAddr = 0;
        }
        else
        if (log > mem68kSize - 32000)
        {
            DebugError2("() -- invalid 68k address 0x%08x for logical screen", log);
        }
        else
        {
            CMagiCScreen::m_logAddr = log;
            setAtariBE32(mem68k + _v_bas_ad, log);
            DebugWarning2("() -- changing of logical screen to 0x%08x is experimental", log);
        }
    }
    if (phys != 0xffffffff)
    {
        if (phys == addr68kVideo)
        {
            DebugWarning2("() -- physical screen address reset to 0x%08x", phys);
            CMagiCScreen::m_physAddr = 0;
        }
        else
        if (phys > mem68kSize - 32000)
        {
            DebugError2("() -- invalid 68k address 0x%08x for physical screen", phys);
        }
        else
        {
            CMagiCScreen::m_physAddr = phys;
            DebugWarning2("() -- changing of physical screen to 0x%08x is experimental", phys);
        }
    }
    if (res != 0xffff)
    {
        CMagiCScreen::m_res = res;
        const char *resname = (res == 0) ? "ST LOW" : ((res == 1) ? "ST MID" : ((res == 2) ? "ST HIGH" : "(unknown)"));
        DebugError2("() -- changing of screen resolution to %u (%s) not supported, yet", res, resname);
        (void)resname;
    }
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Setpalette
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return not supported, always zero
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariSetpalette(uint32_t params, uint8_t *addrOffset68k)
{
    DebugWarning2("() -- partially supported");

    struct SetpaletteParm
    {
        PTR32_BE ptr;       // 68k pointer to 16 16-bit values (big-endian)
    } __attribute__((packed));

    const SetpaletteParm *theParm = (SetpaletteParm *) (addrOffset68k + params);
    uint32_t table = be32toh(theParm->ptr);
    if (table > mem68kSize - 16 * 2)
    {
        DebugError2("() -- invalid 68k address 0x%08x", table);
        return 0;
    }

    //
    // loop over all 16 colours in the palette that shall be changed
    //

    const uint8_t *newColours = addrOffset68k + table;
    for (unsigned i = 0; i < 16; i++)
    {
        // Atari: 16'bxxxxrrrrggggbbbb
        // 0xf000        ignored
        // 0x0f00        red
        // 0x00f0        green
        // 0x000f        blue
        uint16_t ac = *newColours++;     // read 16-bit big-endian
        ac <<= 8;
        ac |= *newColours++;
        CMagiCScreen::setColourPaletteEntry(i, ac);
    }

    // tell GUI thread to update the screen
    atomic_store(&gbAtariVideoBufChanged, true);
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Setcolor
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return previous value
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariSetcolor(uint32_t params, uint8_t *addrOffset68k)
{
    (void) addrOffset68k;
    struct SetcolorParm
    {
        UINT16_BE index;    // colour to change (0..15)
        UINT16_BE val;      // new value
    } __attribute__((packed));

    const SetcolorParm *theParm = (SetcolorParm *) (addrOffset68k + params);
    uint16_t index = be16toh(theParm->index);
    uint16_t val = be16toh(theParm->val);

    if (index > 15)
    {
        DebugError2("() -- invalid colour number %u", index);
        return 0;
    }

    uint16_t prev_val = CMagiCScreen::getColourPaletteEntry(index);
    #if !defined(STE_COLOUR_PALETTE)
    prev_val &= 0x777;  // ST has 512 colours, STe has 4096
    #endif
    if (val != 0xffff)
    {
        CMagiCScreen::setColourPaletteEntry(index, val);
    }

    DebugWarning2("(%2d, 0x%04x) -> 0x%08x", index, val, prev_val);
    return prev_val;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS VsetRGB (Falcon TOS)
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return always zero
 *
 * @note Changes subsequent colour palette entries at once, up to 256.
 *       Also called by VDI during intialisation.
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariVsetRGB(uint32_t params, uint8_t *addrOffset68k)
{
    struct VsetRGBParm
    {
        uint16_t index;     // palette table index 0..255
        uint16_t cnt;       // number of entries to change
        uint32_t pValues;   // new colour values, 32-bit big-endian each
    } __attribute__((packed));

    const VsetRGBParm *theVsetRGBParm = (VsetRGBParm *) (addrOffset68k + params);
    const uint8_t *pValues = (const uint8_t *) (addrOffset68k + be32toh(theVsetRGBParm->pValues));
    // index of first entry to change
    uint16_t index = be16toh(theVsetRGBParm->index);
    // number of entries to change
    uint16_t cnt = be16toh(theVsetRGBParm->cnt);
    DebugInfo2("(index=%u, cnt=%u, 0x%02x%02x%02x%02x)",
              (unsigned) index, (unsigned) cnt,
              (unsigned) pValues[0], (unsigned) pValues[1], (unsigned) pValues[2], (unsigned) pValues[3]);

    //
    // loop over all colours in the palette that shall be changed
    //

    uint32_t *pColourTable = CMagiCScreen::m_pColourTable + index;
    unsigned j = MIN(MAGIC_COLOR_TABLE_LEN, index + cnt);
    for (unsigned i = index; i < j; i++, pValues += 4,pColourTable++)
    {
        // Atari: 00rrggbb
        // 0xff000000        black
        // 0xffff0000        red
        // 0xff00ff00        green
        // 0xff0000ff        blue
        uint32_t c = (pValues[1] << 16) | (pValues[2] << 8) | (pValues[3] << 0);

        #if 0
        //
        // Hack for NVDI in mode "four colours interleaved":
        //  changing colour #3 from black to yellow must be blocked twice
        //

        static unsigned bHacked = 2;
        if (bHacked && (i == 3) && (c == 0x00ffff00) && (Preferences::atariScreenColourMode == atariScreenMode4ip))
        {
            DebugWarning2("() -- suppressed setting colour #3 to yellow (NVDI bug workaround, round %u/2)", 2 - bHacked + 1);
            c = *pColourTable;  // value unchanged
            bHacked--;
            #if defined(M68K_TRACE)
            static int done = 0;
            if (!done)
            {
                m68k_trace_print();
                done = 1;
            }
            #endif
        }
        #endif
        *pColourTable++ = c | (0xff000000);
    }

    // tell GUI thread to update the screen
    atomic_store(&gbAtariVideoBufChanged, true);

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS VgetRGB (Falcon TOS)
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return always zero
 *
 * @note Retrieves subsequent colour palette entries at once, up to 256.
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariVgetRGB(uint32_t params, uint8_t *addrOffset68k)
{
    struct VgetRGBParm
    {
        uint16_t index;     // palette table index 0..255
        uint16_t cnt;       // number of entries to read
        uint32_t pValues;   // read buffer for colour values, 32-bit big-endian each
    } __attribute__((packed));

    const VgetRGBParm *theVgetRGBParm = (VgetRGBParm *) (addrOffset68k + params);
    uint8_t *pValues = (uint8_t *) (addrOffset68k + be32toh(theVgetRGBParm->pValues));
    // index of first entry to read
    uint16_t index = be16toh(theVgetRGBParm->index);
    uint16_t cnt = be16toh(theVgetRGBParm->cnt);
     // number of entries to read
    DebugInfo2("(index=%u, cnt=%u)", index, cnt);

    //
    // loop over all colours in the palette that shall be read
    //

    const uint32_t *pColourTable = CMagiCScreen::m_pColourTable + index;
    unsigned j = MIN(MAGIC_COLOR_TABLE_LEN, index + cnt);
    for (unsigned i = index; i < j; i++, pValues++, pColourTable++)
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
 * @brief Emulator callback: BIOS Bconin
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return read character in bits 0..7
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariBconin(uint32_t params, uint8_t *addrOffset68k)
{
    struct BconinParm
    {
        uint16_t devno;             // 3: MIDI, 4: IKDB, TODO: more to come
    } __attribute__((packed));

    const BconinParm *theParm = (BconinParm *) (addrOffset68k + params);
    uint16_t devno = be16toh(theParm->devno);
    DebugWarning2("(devno = %u)", devno);

    if (devno == 0)
    {
        unsigned char c;
        uint32_t n;

        n = CMagiCPrint::read(&c, 1);
        if (!n)
            return 0;
        else
            return(c);
    }
    else
    if (devno == 1)
    {
        // open serial port, if necessary
        if (!CMagiCSerial::OpenSerialBIOS())
        {
            return 0;
        }

        char c;
        uint32_t ret = CMagiCSerial::Read(&c, 1);
        if (ret > 0)
        {
            return (uint32_t) c & 0x000000ff;
        }
        else
            return 0xffffffff;
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: BIOS Bconout
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return read character in bits 0..7
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariBconout(uint32_t params, uint8_t *addrOffset68k)
{
    struct BconoutParm
    {
        uint16_t devno;             // 3: MIDI, 4: IKDB, TODO: more to come
        PTR32_BE data;            // 68k-pointer to character, as 16-bit word
    } __attribute__((packed));

    const BconoutParm *theParm = (BconoutParm *) (addrOffset68k + params);
    uint16_t devno = be16toh(theParm->devno);
    const uint16_t *pData = (uint16_t *) (addrOffset68k + be32toh(theParm->data));
    uint16_t datum = be16toh(*pData); // 16-bit big endian
    DebugWarning2("(devno = %u, c = 0x%04x)", devno, datum);

    if (devno == 0)
    {
        uint32_t ret = CMagiCPrint::write(addrOffset68k + params + 1, 1);
        if (ret == 1)
            return(0xffffffff);        // OK
        else
            return 0;                // Fehler
    }
    else
    if (devno == 1)
    {
        // open serial port, if necessary
        if (!CMagiCSerial::OpenSerialBIOS())
        {
            return 0;
        }

        return CMagiCSerial::Write((char *) addrOffset68k + params + 1, 1);
    }
    else
    if ((devno == 4) && (datum == 0x16))
    {
        // Joystick interrogation (IKBD $16)
        SDL_Event event;

        event.type = SDL_USEREVENT;
        event.user.code = USEREVENT_POLL_JOYSTICK_STATE;
        event.user.data1 = 0;
        event.user.data2 = 0;
        SDL_PushEvent(&event);
    }
    else
    if ((devno == 4) && (datum == 8))
    {
        // Set relative mouse position reporting (IKBD $08)
        DebugWarning2("() -- Set relative mouse position reporting (IKBD $08)");
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: BIOS Bconstat
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return read character in bits 0..7
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariBconstat(uint32_t params, uint8_t *addrOffset68k)
{
    struct BconstatParm
    {
        uint16_t devno;             // 3: MIDI, 4: IKDB, TODO: more to come
    } __attribute__((packed));

    const BconstatParm *theParm = (BconstatParm *) (addrOffset68k + params);
    uint16_t devno = be16toh(theParm->devno);
    DebugWarning2("(devno = %u)", devno);

    if (devno == 0)
    {
        return CMagiCPrint::getInputStatus();
    }
    else
    if (devno == 1)
    {
        // open serial port, if necessary
        if (!CMagiCSerial::OpenSerialBIOS())
        {
            return 0;
        }

        return CMagiCSerial::ReadStatus() ? 0xffffffff : 0;
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: BIOS Bcostat
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return read character in bits 0..7
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariBcostat(uint32_t params, uint8_t *addrOffset68k)
{
    struct BcostatParm
    {
        uint16_t devno;             // 3: MIDI, 4: IKDB, 0: PRN, TODO: more to come
    } __attribute__((packed));

    const BcostatParm *theParm = (BcostatParm *) (addrOffset68k + params);
    uint16_t devno = be16toh(theParm->devno);
    DebugWarning2("(devno = %u)", devno);

    if (devno == 0)
    {
        return CMagiCPrint::getOutputStatus();
    }
    else
    if (devno == 1)
    {
        // open serial port, if necessary
        if (!CMagiCSerial::OpenSerialBIOS())
        {
            return 0;
        }

        return CMagiCSerial::WriteStatus() ? 0xffffffff : 0;
    }

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Ikbdws
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return nothing, return values is ignored
 *
 * @note The special case Ikbdws(0, "\x1c") for asking the IKBD clock is already handled
 *       inside the kernel.
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariIkbdws(uint32_t params, uint8_t *addrOffset68k)
{
    struct IkbdwsParm
    {
        uint16_t len_minus_one;         // 0: one byte, 1: two bytes, et cetera
        uint32_t data;                  // 68k pointer to data
    } __attribute__((packed));

    const IkbdwsParm *theParm = (IkbdwsParm *) (addrOffset68k + params);
    uint32_t len = be16toh(theParm->len_minus_one) + 1;
    //const uint8_t *pData = (uint8_t *) (addrOffset68k + be32toh(theParm->data));
    DebugError2("(len = %u, data = 0x%08x) -- not implemented", len, be32toh(theParm->data));
    (void)len;
    return EINVFN;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: XBIOS Dosound
 *
 * @param[in] param             68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return previous content of Atari sound_data variable:
 *              sound_data:     DS.L 1
 *              sound_delay:    DS.B 1
 *              sound_byte:     DS.B 1
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariDosound(uint32_t params, uint8_t *addrOffset68k)
{
    struct DosoundParm
    {
        uint32_t fn;                // 0:XBIOS Dosound() / 1:click / 2:pling
        uint32_t new_sound;         // 68k pointer to new sound data
        uint32_t sound_vars;        // 68k address of sound_data, sound_delay und sound_byte
    } __attribute__((packed));

    const DosoundParm *theParm = (DosoundParm *) (addrOffset68k + params);
    uint32_t fn = be32toh(theParm->fn);
    const uint8_t *pSoundData = (uint8_t *) (addrOffset68k + be32toh(theParm->new_sound));
    uint8_t *pSoundVariables = (uint8_t *) (addrOffset68k + be32toh(theParm->sound_vars));
    DebugInfo2("(fn = %u, new_sound=0x%08x, sound_vars=0x%08x)", be32toh(theParm->fn), be32toh(theParm->new_sound), be32toh(theParm->sound_vars));
    if (fn == 1)
    {
        CAudio::play_click();
    }
    else
    if (fn == 2)
    {
        CAudio::play_pling();
    }
    (void) pSoundData;
    (void) pSoundVariables;
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Convert Atari drive to device code
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
 * @brief Update _drvbits and _nflops in Atari memory
 *
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @note Needed during boot and whenever a volume is added or removed
 *
 ************************************************************************************************/
void CMagiC::updateDriveBits(uint8_t *addrOffset68k)
{
    uint32_t drvbits = m_HostXFS.getDrvBits() | CVolumeImages::getDrvBits();
    setAtariBE32(addrOffset68k + _drvbits, drvbits);
    uint16_t nflops = 0;
    if (drvbits & 1)
    {
        nflops = 1;
    }
    if (drvbits & 2)
    {
        if (nflops == 0)
        {
            DebugWarning2("() -- Have B: without A:, must set _nflops to 2");
        }
        nflops = 2;
    }
    setAtariBE16(addrOffset68k + _nflops, nflops);
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: Device operations
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
            updateDriveBits(addrOffset68k);
            return E_OK;
        }
        else
        if (type == 2)
        {
            //
            // Volume image
            //

            CVolumeImages::eject(drv);
            updateDriveBits(addrOffset68k);
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
    // note that colons in buttons must be quoted
    static const char *mount_buttons_a = "A\\:, A\\: Read-Only, Cancel";
    static const char *mount_buttons_b = "B\\:, B\\: Read-Only, Cancel";
    static const char *mount_buttons_ab = "A\\:, B\\:, A\\: Read-Only, B\\: Read-Only, Cancel";

    // do some tidy up to free logical drives that failed to mount
    CVolumeImages::remove_failed_volumes();

    bool availA = (!m_HostXFS.isDrvValid('A' - 'A') && !CVolumeImages::isDrvValid('A' - 'A'));
    bool availB = (!m_HostXFS.isDrvValid('B' - 'A') && !CVolumeImages::isDrvValid('B' - 'A'));

    if (availA || availB)
    {
        uint16_t drv = 0xffff;
        const char *mount_buttons;
        int answerA = 1000;         // initialise as invalid
        int answerB = 1000;
        int answerA_ro = 1000;
        int answerB_ro = 1000;

        if (availA && availB)
        {
            mount_buttons = mount_buttons_ab;
            answerA = 101;
            answerB = 102;
            answerA_ro = 103;
            answerB_ro = 104;
        }
        else
        if (availA)
        {
            mount_buttons = mount_buttons_a;
            answerA = 101;
            answerA_ro = 102;
        }
        else
        if (availB)
        {
            mount_buttons = mount_buttons_b;
            answerB = 101;
            answerB_ro = 102;
        }

        // drive A: or B: currently unused. Mount image or path.
        struct stat statbuf;
        if (stat(allocated_path, &statbuf) == 0)
        {
            mode_t ftype = (statbuf.st_mode & S_IFMT);
            if (ftype == S_IFDIR)
            {
                int answer = showDialogue("Mount directory?", allocated_path, mount_buttons);
                if ((answer == answerA) || (answer == answerA_ro))
                {
                    drv = 'A' - 'A';
                }
                else
                if ((answer == answerB) || (answer == answerB_ro))
                {
                    drv = 'B' - 'A';
                }
                bool bReadOnly = ((answer == answerA_ro) || (answer == answerB_ro));

                if (drv < NDRIVES)
                {
                    m_HostXFS.setNewDrv(drv, allocated_path, true, bReadOnly);
                    updateDriveBits(mem68k);
                    return true;
                }
            }
            else
            if (ftype == S_IFREG)
            {
                int volume_type = CVolumeImages::checkFatVolume(allocated_path);
                if (volume_type >= 0)
                {
                    const char *question = "Mount partition of disk image?";
                    if (volume_type == 12)
                    {
                        question = "Mount FAT12 image?";
                    }
                    else
                    if (volume_type == 16)
                    {
                        question = "Mount FAT16 image?";
                    }
                    else
                    if (volume_type == 32)
                    {
                        question = "Mount FAT32 image?";
                    }
                    int answer = showDialogue(question, allocated_path, mount_buttons);
                    if ((answer == answerA) || (answer == answerA_ro))
                    {
                        drv = 'A' - 'A';
                    }
                    else
                    if ((answer == answerB) || (answer == answerB_ro))
                    {
                        drv = 'B' - 'A';
                    }
                    bool bReadOnly = ((answer == answerA_ro) || (answer == answerB_ro));

                    if (drv < NDRIVES)
                    {
                        CVolumeImages::setNewDrv(drv, allocated_path, true, bReadOnly, statbuf.st_size);
                        updateDriveBits(mem68k);
                        return true;
                    }
                }
                else
                {
                    showAlert("This file seems not to be a FAT12/16/32 volume:", allocated_path);
                }
            }
        }
        else
        {
            DebugError2("() -- No diskimage: %s", allocated_path);
        }
    }
    else
    {
        (void) showDialogue("Eject A: or B: before reassign them!", allocated_path, "Cancel");
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

    showAlert("The emulator was halted", errMsg);
    pTheMagiC->stopExec();
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

    DebugError2("()");

    // FÜR DIE FEHLERSUCHE: GIB DIE LETZTEN TRACE-INFORMATIONEN AUS.
    #if DEBUG_68K_EMU
    m68k_trace_print("68k-trace-dump-syserr.txt");
    _DumpAtariMem("atarimem.bin");
    #endif

    GetActAtariPrg(&AtariPrgFname, &act_pd);
    m68k_pc = be32toh(*((uint32_t *) (addrOffset68k + proc_stk + 2)));

    DebugInfo2("() -- act_pd = 0x%08lx", act_pd);
    DebugInfo2("() -- Prozeßpfad = %s", (AtariPrgFname) ? AtariPrgFname : "<unknown>");
#if defined(_DEBUG)
    if (m68k_pc < mem68kSize - 8)
    {
        uint16_t opcode1 = be16toh(*((uint16_t *) (addrOffset68k + m68k_pc)));
        uint16_t opcode2 = be16toh(*((uint16_t *) (addrOffset68k + m68k_pc + 2)));
        uint16_t opcode3 = be16toh(*((uint16_t *) (addrOffset68k + m68k_pc + 4)));
        DebugInfo2("() -- opcode = 0x%04x 0x%04x 0x%04x", (unsigned) opcode1, (unsigned) opcode2, (unsigned) opcode3);
    }
#endif

    // Note that the first byte (in big-endian: the uppermost byte) of proc_pc holds the exception vector number
    send68kExceptionData(
                addrOffset68k[proc_pc /*0x3c4*/],           // exception vector number
                pTheMagiC->m_BusErrorAddress,
                pTheMagiC->m_BusErrorAccessMode,
                m68k_pc,                                                    // pc
                be16toh(*((uint16_t *) (addrOffset68k + proc_stk))),        // sr
                be32toh(*((uint32_t *) (addrOffset68k + proc_usp))),        // usp
                (uint32_t *) (addrOffset68k + proc_regs /*0x384*/),         // Dx (big-endian)
                (uint32_t *) (addrOffset68k + proc_regs + 32),              // Ax (big-endian)
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


/** **********************************************************************************************
 *
 * @brief Emulator callback: Atari has shut down itself
 *
 * @param[in] params            68k address of parameter structure
 * @param[in] addrOffset68k     Host address of 68k memory
 *
 * @return (irrelevant)
 *
 * @note MagiC's SHUTDOWN.PRG more or less gracefully stops all running applications and then
 *       calls [xbios(39, 'AnKr', 0)]. This is ignored on the Atari, but the emulator XBIOS
 *       calls [v_clswk()] to close the VDI. Then it calls the emulator. Without VDI the
 *       emulated system may not continue, so we make sure that the 68k instruction
 *       loop is left, and then emulator thread can end.
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariExit(uint32_t params, uint8_t *addrOffset68k)
{
    (void) params;
    (void) addrOffset68k;

    DebugInfo2("()");

    sExitImmediately = true;        // make sure the inner instruction loop in m68kcpu ends
    pTheMagiC->m_bCanRun = false;   // also leave the outer loop

    // Tell the main thread to also leave
    pTheMagiC->OS_SetEvent(
            &pTheMagiC->m_EventId,
            EMU_EVNT_TERM);
    return 0;
}


/**********************************************************************
*
* Callback des Emulators: Debug-Ausgaben
*
**********************************************************************/

uint32_t CMagiC::AtariDebugOut(uint32_t params, uint8_t *addrOffset68k)
{
    DebugInfo2("(%s)", CConversion::textAtari2Host(addrOffset68k + params));
    (void) params;
    (void) addrOffset68k;

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
    (void) errorCode;
    (void) params;
    (void) addrOffset68k;
    /*
     Das System kann keinen passenden Grafiktreiber finden.

     Installieren Sie einen Treiber, oder wechseln Sie die Bildschirmauflösung unter MacOS, und starten Sie MagiCMacX neu.
     [MagiCMacX beenden]

     The system could not find an appropriate graphics driver.

     Install a driver, or change the monitor resolution resp. colour depth using the system's control panel. Finally, restart  MagiCMacX.
     [Quit MagiCMacX]
     */
    showAlert("The emulated system could not find a suitable video driver", "Review configuration file!");
    pTheMagiC->stopExec();    // fatal error for execution thread
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
        DebugInfo2("() -- normaler Abbruch");
        break;    // normaler Abbruch, Thread-Ende
    }
*/
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: get keyboard and mouse data
 *
 * @param[in] params            0: about to handle interrupt, 1: leaving interrupt
 * @param[in] addrOffset68k     Host address of 68k memory (unused)
 *
 * @return scancode or mouse code. Zero, if none is available
 *
 * @note params = 0: called from ikbdsys(),
 *       params = 1: called from midikey_int() in MagiC kernel (MFP interrupt 6)
 * @note The function ikbdsys() must ignore NUL bytes if they do not belong to
 *       a mouse packet.
 *
 ************************************************************************************************/
uint32_t CMagiC::AtariGetKeyboardOrMouseData(uint32_t params, uint8_t *addrOffset68k)
{
    (void) addrOffset68k;

    OS_EnterCriticalRegion(&m_KbCriticalRegionId);
    uint32_t ret = m_pKbRead != m_pKbWrite;        // any data in buffer?

    // If no data in buffer, query mouse buttons and position
    if (!ret)
    {
        // No mouse handling before VDI initialisation
        int8_t buf[3];
        if (m_LineAVars != nullptr)
        {
            ret = CMagiCMouse::getNewPositionAndButtonState(buf);
        }

        if (ret)
        {
            PutKeyToBuffer((uint8_t) buf[0]);
            PutKeyToBuffer((uint8_t) buf[1]);
            PutKeyToBuffer((uint8_t) buf[2]);
        }
    }

    if (params)
    {
        // key was processed by kernel, end of interrupt handler
        OS_ExitCriticalRegion(&m_KbCriticalRegionId);

        // Clear "interrupt service bit", if any
        //  if (!ret)
        //      Asgard68000SetIRQLine(k68000IRQLineIRQ6, k68000IRQStateClear);
        return ret;        // maybe more interrupts
    }

    if (!ret)
    {
        OS_ExitCriticalRegion(&m_KbCriticalRegionId);
        // no data to process
        return 0;  // no mouse or keyboard events to process, note that caller must explicitly ignore zeros
    }

    // read byte from mouse/keyboard ringbuffer
    ret = *m_pKbRead++;
    if (m_pKbRead >= m_cKeyboardOrMouseData + KEYBOARDBUFLEN)
    {
        m_pKbRead = m_cKeyboardOrMouseData;
    }
    OS_ExitCriticalRegion(&m_KbCriticalRegionId);
    return ret;
}


/** **********************************************************************************************
 *
 * @brief Emulator callback: MMXDAEMON polling
 *
 * @param[in] params            0: about to handle interrupt, 1: leaving interrupt
 * @param[in] addrOffset68k     Host address of 68k memory (unused)
 *
 * @return zero or error code
 *
 * @note The MMXDAEMON provides an interface between the host and the Atari shell.
 *       It regularly polls for host shutdown and for starting Atari applications from
 *       the host's file manager or command line.
 *
 ************************************************************************************************/
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
        //
        // run Atari programs, initiated by host
        //

        case 1:
        {
            // mounting drive seems to fail if done during Atari boot phase
            if (Preferences::mountDriveParameter != nullptr)
            {
                // Send user event to event loop
                SDL_Event event;

                event.type = SDL_USEREVENT;
                event.user.code = USEREVENT_POLL_MOUNT;
                event.user.data1 = 0;
                event.user.data2 = 0;
                SDL_PushEvent(&event);
            }

            // TODO: OS_EnterCriticalRegion(&m_AECriticalRegionId);
            const char *path = Preferences::AtariStartApplications[0];
            if (path != nullptr)
            {
                uint8_t *pBuf = addrOffset68k + be32toh(theMmxDaemonParm->parm);
                if (!pTheMagiC->m_HostXFS.isAtariPath(path))
                {
                    //
                    // If the path is a host path, directly convert it to Atari path in the caller's buffer
                    //

                    if (pTheMagiC->m_HostXFS.hostPath2AtariPath(path, 'C' - 'A', pBuf, 256) == 0)
                    {
                        ret = E_OK;
                    }
                }
                else
                {
                    //
                    // If the path is an Atari path, check for overflow and copy it to the caller's buffer.
                    // TODO: convert non-ASCII characters from host to Atari
                    //

                    if (strlen(Preferences::AtariStartApplications[0]) < 255)
                    {
                        // Copy path of application to start to MMXDAEMN
                        strcpy((char *) pBuf, Preferences::AtariStartApplications[0]);
                        ret = E_OK;
                    }
                    else
                    {
                        DebugError2("() -- path length overflow");
                    }
                }

                // update queue
                free((void *) Preferences::AtariStartApplications[0]);
                for (unsigned i = 0; i < MAX_START_APPS - 1; i++)
                {
                    Preferences::AtariStartApplications[i] = Preferences::AtariStartApplications[i + 1];
                }
                Preferences::AtariStartApplications[MAX_START_APPS - 1] = nullptr;
            }
            else
            {
                ret = (uint32_t) EFILNF;
            }
            // TODO: OS_ExitCriticalRegion(&m_AECriticalRegionId);
            break;
        }

        //
        // Atari queries shutdown status
        //

        case 2:
            ret = (uint32_t) pTheMagiC->m_bShutdown;
            break;

        default:
            ret = (uint32_t) EUNCMD;
            break;
    }

    return ret;
}
