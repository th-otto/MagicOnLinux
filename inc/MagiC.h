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
// system header files
#include <pthread.h>
// program header files
#include "Globals.h"
#include "osd_cpu.h"
#include "XCmd.h"
#include "HostXFS.h"
#include "MagiCKeyboard.h"
#include "MagiCMouse.h"
#include "MagiCScreen.h"

#define KEYBOARDBUFLEN  32

// SDL user events, messages from emulator thread to GUI thread
const int USEREVENT_RUN_EMULATOR_WINDOW_UPDATE = 1;
const int USEREVENT_OPEN_EMULATOR_WINDOW =2;
const int USEREVENT_RUN_EMULATOR = 3;
const int USEREVENT_POLL_MOUNT = 4;
const int USEREVENT_POLL_JOYSTICK_STATE = 5;

class CMagiC
{
   public:
    CMagiC();       // constructor
    ~CMagiC();      // destructor

    // initialisation
    int init(CXCmd *pXCmd);
    int createThread(void);         // create emulator thread
    void startExec(void);           // ... let it run
    void stopExec(void);            // ... pause it
    void terminateThread(void);     // terminate it

    int sendSdlKeyboard(int sdlScanCode, bool KeyUp);
    void sendKbshift(uint8_t atari_kbshift);
    unsigned getKbshift();
    #if 0
    int SendKeyboardShift(uint32_t modifiers);
    #endif
    int sendMousePosition(int x, int y);
    int sendMouseMovement(double xrel, double yrel);
    int sendMouseButton(unsigned int NumOfButton, bool bIsDown);
    int sendJoystickState(uint8_t header, uint8_t state);
    int sendHz200(void);
    int sendVBL(void);
    void sendBusError(uint32_t addr, const char *AccessMode);
    //void SendAtariFile(const char *pBuf); // remnant from MagicMac(X) and AtariX
    bool sendDragAndDropFile(const char *allocated_path);
    void sendShutdown(void);
    //void ChangeXFSDrive(short drvNr);
    static void GetActAtariPrg(const char **pName, uint32_t *pact_pd);
    bool m_bEmulatorIsRunning;
    bool m_bEmulatorHasEnded;
    bool m_bShutdown;

   private:
    struct Atari68kData
    {
        MXVDI_PIXMAP m_PixMap;    // the Atari screen, baseAddr is virtual 68k address
        MgMxCookieData m_CookieData;
    } __attribute__((packed));

    void initCookieData(MgMxCookieData *pCookieData);
    void initHostCallbacks(struct MacXSysHdr *pMacXSysHdr, CXCmd *pXCmd);
    static int relocate(FILE *f, uint32_t file_size, uint8_t *tbase, const ExeHeader *exehead);
    int LoadReloc(const char *path, uint32_t stackSize, int32_t  reladdr, BasePage **basePage);
    int GetKbBufferFree(void);
    void PutKeyToBuffer(uint8_t key);
    static void *_EmuThread(void *param);
    int EmuThread(void);
    static int IRQCallback(int IRQLine);
    static uint32_t AtariInit(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariBIOSInit(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariBconin(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariBconout(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariBconstat(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariBcostat(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariIkbdws(uint32_t params, uint8_t *addrOffset68k);

    uint32_t AtariVdiInit(uint32_t params, uint8_t *addrOffset68k);
    uint32_t AtariExec68k(uint32_t params, uint8_t *addrOffset68k);
    uint32_t OpenSerialBIOS(void);
    static void SendMessageToMainThread( bool bAsync, uint32_t command );
    static uint32_t AtariDOSFn(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariGettime(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariSettime(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariSysHalt(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariSetscreen(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariSetpalette(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariSetcolor(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariVsetRGB(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariVgetRGB(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariDosound(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariSysErr(uint32_t params, uint8_t *addrOffset68k);
    static void *_Remote_AtariSysErr( void *param );
    static uint32_t AtariColdBoot(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariExit(uint32_t params, uint8_t *addrOffset68k);
    static uint32_t AtariDebugOut(uint32_t params, uint8_t *addrOffset68k);
    static void *_Remote_AtariError( void *param );
    static uint32_t AtariError(uint32_t params, uint8_t *addrOffset68k);
    uint32_t AtariGetKeyboardOrMouseData(uint32_t params, uint8_t *addrOffset68k);
#if defined(MAGICLIN)
    static uint32_t MmxDaemon(uint32_t params, unsigned char *AdrOffset68k);
    #else
    uint32_t MmxDaemon(uint32_t params, unsigned char *AdrOffset68k);
#endif
    static uint32_t AtariYield(uint32_t params, uint8_t *addrOffset68k);

    uint32_t Drv2DevCode(uint32_t params, uint8_t *addrOffset68k);
    void updateDriveBits(uint8_t *addrOffset68k);
    uint32_t RawDrvr(uint32_t params, uint8_t *addrOffset68k);

    // private attributes
    uint8_t *m_AtariKbData;         // [0] = kbshift, [1] = kbrepeat
    uint32_t *m_pAtariActPd;        // active Atari process
    uint32_t *m_pAtariActAppl;      // active Atari application
    BasePage *m_BasePage;           // loaded MagiC Kernel

    pthread_t m_EmuTaskID;          // emulation thread
    pthread_mutex_t m_EventMutex     = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t m_ConditionMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t m_Cond  = PTHREAD_COND_INITIALIZER;
    void OS_SetEvent(uint32_t *event, uint32_t flags);
    uint32_t OS_AskEvent(const uint32_t *event, uint32_t flags);
    void OS_WaitForEvent(uint32_t *event, uint32_t *flags);

    CHostXFS m_HostXFS;              // XFS
    uint32_t m_CurrModifierKeys;     // current state of Shift/Cmd/Alt...
    bool m_bBusErrorPending;
    uint32_t m_BusErrorAddress;
    char m_BusErrorAccessMode[32];

    bool m_bInterrupt200HzPending;
    bool m_bInterruptVBLPending;
    bool m_bInterruptMouseKeyboardPending;
    int m_InterruptMouseWhereX;         // for absolute mouse mode
    int m_InterruptMouseWhereY;         // for absolute mouse mode
    double m_InterruptMouseMoveRelX;    // for relative mouse mode
    double m_InterruptMouseMoveRelY;
    bool m_bInterruptMouseButton[2];
    bool m_bInterruptJoystickButtons;
    bool m_bInterruptPending;
    bool m_bWaitEmulatorForIRQCallback;

    #define EMU_EVNT_RUN           0x00000001
    #define EMU_EVNT_TERM          0x00000002
    uint32_t m_EventId;
    bool m_bCanRun;
    #define EMU_INTPENDING_KBMOUSE 0x00000001
    #define EMU_INTPENDING_200HZ   0x00000002
    #define EMU_INTPENDING_VBL     0x00000004
    #define EMU_INTPENDING_OTHER   0x00000008
    uint32_t m_InterruptEventsId;

    bool m_bSpecialExec;
    uint8_t *m_LineAVars;

    // ring buffer for keyboard and mouse
    uint8_t m_cKeyboardOrMouseData[KEYBOARDBUFLEN];
    uint8_t *m_pKbRead;           // read pointer
    uint8_t *m_pKbWrite;          // write pointer
    pthread_mutex_t m_KbCriticalRegionId;   // mutex for keyboard events

    // Screen data
    bool m_bScreenBufferChanged;
    //unsigned char *m_pFgBuffer;                // foreground buffer
    //unsigned long m_FgBufferLineLenInBytes;
//    unsigned char *m_pBgBuffer;                // do we need a background buffer?
//    unsigned long m_BgBufferLineLenInBytes;
    pthread_mutex_t m_ScrCriticalRegionId;

    uint32_t m_AtariShutDownDelay;        // added for AtariX
};
