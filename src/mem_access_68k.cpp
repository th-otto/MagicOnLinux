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
* User provided memory access functions for the 68k emulator
*
*/

#include "config.h"
// system headers
#include <cassert>
#include <cerrno>
#include <sys/param.h>
#include <time.h>
// programm headers
#include "Debug.h"
#include "emulation_globals.h"
#include "Globals.h"
#include "Atari.h"
#include "gui.h"
#include "register_model.h"

// compile time switches

#ifdef _DEBUG
#define _DEBUG_WRITEPROTECT_ATARI_OS
#endif

// Stuff for the 68k emulator
extern "C" {

typedef unsigned int m68k_data_type;
typedef unsigned int m68k_addr_type;

extern m68k_data_type m68k_read_memory_8(m68k_addr_type address);
extern m68k_data_type m68k_read_memory_16(m68k_addr_type address);
extern m68k_data_type m68k_read_memory_32(m68k_addr_type address);
extern void m68k_write_memory_8(m68k_addr_type address, m68k_data_type value);
extern void m68k_write_memory_16(m68k_addr_type address, m68k_data_type value);
extern void m68k_write_memory_32(m68k_addr_type address, m68k_data_type value);


#ifndef NDEBUG
/** **********************************************************************************************
 *
 * @brief Get the name of the addressed Atari chip from 68k address
 *
 * @param[in] addr        68k address
 *
 * @return name or "?", if unknown
 *
 ************************************************************************************************/
#ifndef NDEBUG
static const char *AtariAddr2Description(uint32_t addr)
{
    // Convert ST address to TT address

    if ((addr >= 0xff0000) && (addr < 0xffffff))
    {
        addr |= 0xff000000;
    }

    switch(addr)
    {
        case 0xffff8006:
            return("Falcon 030 Monitor type, RAM size, etc.");
            break;

        case 0xffff8201:
            return("Video Controller: Video base register high");
            break;

        case 0xffff8203:
            return("Video Controller: Video base register mid");
            break;

        case 0xffff820a:
            return "Video Sync Mode";
            break;

        case 0xffff820d:
            return("Video Controller: Video base register low (STE)");
            break;

        case 0xffff8260:
        case 0xffff8261:
            return("Video Controller: ST shift mode register (0: 4 Planes, 1: 2 Planes, 2: 1 Plane)");
            break;

        case 0xffff8262:
            return("Video Controller: TT shift mode register");
            break;

        // Interrupts A: Timer B/XMIT Err/XMIT Buffer Empty/RCV Err/RCV Buffer full/Timer A/Port I6/Port I7 (Bits 0..7)
        case 0xfffffa0f:
            return("MFP: ISRA (Interrupt-in-service A)");
            break;

        // Interrupts B: Port I0/Port I1/Port I2/Port I3/Timer D/Timer C/Port I4/Port I5 (Bits 0..7)
        case 0xfffffa11:
            return("MFP: ISRB (Interrupt-in-service B)");
            break;
    }

    if ((addr >= 0xffff8240) && (addr < 0xffff8260))
    {
        return("ST Video palette register 0..15");
    }

    if ((addr >= 0xffff8400) && (addr < 0xffff8600))
    {
        return("TT Video palette register 0..255");
    }

    if ((addr >= 0xfffffa40) && (addr < 0xfffffa54))
    {
        return("MC68881");
    }

    if ((addr >= 0xfffffc00) && (addr < 0xfffffc07))
    {
        return("6850 ACIA I/O Chips");
    }

    if ((addr >= 0xffff0000) && (addr < 0xffff8000))
    {
        return("Reserved I/O Space");
    }

    if ((addr >= 0xfffc0000) && (addr < 0xffff0000))
    {
        return("ST 192k ROM");
    }

    // if we do not know details ...
    if (addr >= 0xffff8000)
    {
        return("ST/TT I/O");
    }

    return("?");
}
#endif


/** **********************************************************************************************
 *
 * @brief Convert 68k exception to its name
 *
 * @param[in]  exception_no     68k exception number
 *
 * @return  human readable description
 *
 ************************************************************************************************/
const char *exception68k_to_name(uint32_t addr)
{
    static char buf[64];

    if ((addr >= INTV_32_TRAP_0) && (addr <= INTV_47_TRAP_15))
    {
        sprintf(buf, "Trap %u", (addr - INTV_32_TRAP_0) >> 2);
    }
    else
    switch (addr)
    {
        case INTV_2_BUS_ERROR:      strcpy(buf, "bus error"); break;
        case INTV_3_ADDRESS_ERROR:  strcpy(buf, "address error"); break;
        case INTV_4_ILLEGAL:        strcpy(buf, "illegal instruction"); break;
        case INTV_5_DIV_BY_ZERO:    strcpy(buf, "division by zero"); break;
        case INTV_6_CHK:            strcpy(buf, "CHK"); break;
        case INTV_7_TRAPV:          strcpy(buf, "TRAPV"); break;
        case INTV_8_PRIV_VIOL:      strcpy(buf, "privilege violation"); break;
        case INTV_9_TRACE:          strcpy(buf, "TRACE"); break;
        case INTV_10_LINE_A:        strcpy(buf, "Line A"); break;
        case INTV_11_LINE_F:        strcpy(buf, "Line F"); break;
        case INTV_13:               strcpy(buf, "co-proc protocol (68030)"); break;
        case INTV_14:               strcpy(buf, "format (68030)"); break;
        case INTV_26_AUTV_2:        strcpy(buf, "ST: HBlank"); break;
        case INTV_28_AUTV_4:        strcpy(buf, "ST: VBlank"); break;
        case INTV_56:               strcpy(buf, "MMU configuration"); break;
        case INTV_MFP5_HZ200:       strcpy(buf, "MFP5: 200 Hz timer"); break;
        case INTV_MFP6_IKBD_MIDI:   strcpy(buf, "MFP6: IKBD/MIDI"); break;
        case INTV_MFP7_FDC_ACSI:    strcpy(buf, "MFP7: FDC/ACSI"); break;
        case INTV_MFP8:             strcpy(buf, "MFP8: display enable (?)"); break;
        case INTV_MFP9_TX_ERR:      strcpy(buf, "MFP9: transmission fault RS232"); break;
        case INTV_MFP10_SND_EMPT:   strcpy(buf, "MFP10: RS232 transmission buffer empty"); break;
        case INTV_MFP11_RX_ERR:     strcpy(buf, "MFP11: receive fault RS232"); break;
        case INTV_MFP12_RCV_FULL:   strcpy(buf, "MFP12: RS232 receive buffer full"); break;
        case INTV_MFP13:            strcpy(buf, "MFP13: unused"); break;
        case INTV_MFP14_RING_IND:   strcpy(buf, "MFP14: RS232: incoming call"); break;
        case INTV_MFP15_MNCHR:      strcpy(buf, "MFP15: monochrome monitor detect"); break;

        case etv_timer:             strcpy(buf, "etv_timer"); break;
        case etv_critic:            strcpy(buf, "etv_critic"); break;
        case resvector:             strcpy(buf, "resvector"); break;
        case phystop:               strcpy(buf, "phystop"); break;
        case _membot:               strcpy(buf, "_membot"); break;
        case _memtop:               strcpy(buf, "_memtop"); break;
        case _v_bas_ad:             strcpy(buf, "_v_bas_ad: logical VRAM base"); break;
        case _vblqueue:             strcpy(buf, "_vblqueue"); break;
        case colorptr:              strcpy(buf, "colorptr"); break;
        case screenpt:              strcpy(buf, "screenpt"); break;

        default:                    strcpy(buf, "other"); break;
    }

    return buf;
}



/** **********************************************************************************************
 *
 * @brief Helper to get the Atari process name, including "unknown"
 *
 * @param[in] address        68k address
 *
 * @return byte read, extended to 32-bit unsigned
 *
 ************************************************************************************************/
static void getAtariPrg(const char **pprocName, uint32_t *pact_pd)
{
    getActAtariPrg(pprocName, pact_pd);
    if (*pprocName == nullptr)
    {
        *pprocName = "<unknown>";
    }
}


#if defined(M68K_WRITE_WATCHES)
/** **********************************************************************************************
 *
 * @brief Debug helper to find a write-watch
 *
 * @param[in] address        68k address
 *
 * @return true, if there is a watch
 *
 ************************************************************************************************/
static bool is_write_watch(uint32_t address)
{
    for (unsigned i = 0; i < M68K_WRITE_WATCHES; i++)
    {
        if (address == m68k_write_watches[i])
        {
            return true;
        }
    }
    return false;
}
#endif


#if defined(EMULATE_NULLPTR_BUSERR)

#define HANDLE_LOWMEM_READ_BUSERR(address, text) \
    if ((address < 8) && !m68k_get_super()) \
    { \
        const char *procName; \
        uint32_t act_pd; \
        getAtariPrg(&procName, &act_pd); \
        DebugError2("() -- User mode read access by process %s at address 0x%08x", \
                        procName, address); \
        sendBusError(address, text); \
        return 0; \
    }

#define HANDLE_LOWMEM_WRITE_BUSERR(address, text) \
    if ((address < 8) && !m68k_get_super()) \
    { \
        const char *procName; \
        uint32_t act_pd; \
        getAtariPrg(&procName, &act_pd); \
        DebugError2("() -- User mode write access by process %s at address 0x%08x", \
                        procName, address); \
        sendBusError(address, text); \
        return; \
    }

#else

#define HANDLE_LOWMEM_READ_BUSERR(address, text)
#define HANDLE_LOWMEM_WRITE_BUSERR(address, text)

#endif

#if defined(_DEBUG_WRITEPROTECT_ATARI_OS)

#define WRITE_PROTECT_OS(address, text) \
    if ((address >= addrOsRomStart) && (address < addrOsRomEnd)) \
    { \
        const char *procName; \
        uint32_t act_pd; \
        getAtariPrg(&procName, &act_pd); \
        DebugError2("() -- Blocked attempt to overwrite MagiC OS by process %s at address 0x%08x", \
                        procName, address); \
        sendBusError(address, text); \
        return; \
    }
#else

#define WRITE_PROTECT_OS(address, text)

#endif

#if defined(_DEBUG_WATCH_68K_VECTOR_CHANGE)

#define WATCH_68K_VECTOR_CHANGE(address, value) \
    if ((address < 0x140) || \
       (address == _v_bas_ad) || \
       /*(address = etv_timer) ||*/ \
       (address == etv_critic) || \
       (address == resvector) || \
       (address == phystop) || \
       (address == _membot) || \
       (address == _memtop) || \
       (address == _vblqueue) || \
       (address == colorptr) || \
       (address == screenpt)) \
    { \
        const char *vecname = exception68k_to_name(address); \
        const char *procName; \
        uint32_t act_pd; \
        getAtariPrg(&procName, &act_pd); \
        DebugWarning2("() -- 68k vec 0x%08x := 0x%08x (%s) by process %s", \
                        address, value, vecname, procName); \
    }
#else

#define WATCH_68K_VECTOR_CHANGE(address, value)

#endif


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: read a single byte
 *
 * @param[in] address        68k address
 *
 * @return byte read, extended to 32-bit unsigned
 *
 ************************************************************************************************/
m68k_data_type m68k_read_memory_8(m68k_addr_type address)
{
    HANDLE_LOWMEM_READ_BUSERR(address, "read byte")
    if (address < addr68kVideo)
    {
        // read regular memory
        return(*((uint8_t *) (addrOpcodeROM + address)));
    }

    if (address < addr68kVideoEnd)
    {
        // read from host's video memory
        return(*((uint8_t *) (hostVideoAddr + (address - addr68kVideo))));
    }

    bool b_success;
    uint32_t datum = CRegisterModel::read_reg(address, 1, &b_success);
    if (b_success)
    {
        return datum;
    }

    // handle access error
    const char *procName;
    uint32_t act_pd;
    getAtariPrg(&procName, &act_pd);

    DebugError("m68k_read_memory_8(addr = 0x%08lx) --- bus error (%s) by process %s",
                    address, AtariAddr2Description(address), procName);
        /*
        if (address == 0xfffffa11)
        {
            DebugWarning("CMagiC::ReadByte() --- Access to \"MFP ISRB\" is ignored to make ZBENCH.PRG working!");
        }
        else
        */
    sendBusError(address, "read byte");
    return 0xff;
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: read a single 16-bit halfword in big-endian mode
 *
 * @param[in] address        68k address
 *
 * @return 16-bit halfword read, extended to 32-bit unsigned
 *
 ************************************************************************************************/
m68k_data_type m68k_read_memory_16(m68k_addr_type address)
{
    HANDLE_LOWMEM_READ_BUSERR(address, "read 16-bit")
    if (address < addr68kVideo)
    {
        // read regular memory and convert to big-endian
        return getAtariBE16(addrOpcodeROM + address);
    }

    if (address < addr68kVideoEnd)
    {
        // read from host's video memory
        if (gbAtariVideoRamHostEndian)
        {
            // x86 has bgr instead of rgb
            return *((uint16_t *) (hostVideoAddr + (address - addr68kVideo)));
        }
        else
        {
            return getAtariBE16(hostVideoAddr + (address - addr68kVideo));
        }
    }

    // Overflow region for graphics driver (MVDI and NVDI)
    // 4 for mode 2ip
    // 8 for mode 4ip
    if (address < addr68kVideoEnd + 8)
    {
        return 0;
    }

    bool b_success;
    uint32_t datum = CRegisterModel::read_reg(address, 2, &b_success);
    if (b_success)
    {
        return datum;
    }

    const char *procName;
    uint32_t act_pd;
    getAtariPrg(&procName, &act_pd);

    DebugError("m68k_read_memory_16(addr = 0x%08lx) --- bus error (%s) by process %s",
                    address, AtariAddr2Description(address), procName);
    sendBusError(address, "read word");
    return(0xffff);        // in fact this was a bus error
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: read a single 32-bit word in big-endian mode
 *
 * @param[in] address        68k address
 *
 * @return 32-bit word read
 *
 ************************************************************************************************/
m68k_data_type m68k_read_memory_32(m68k_addr_type address)
{
    HANDLE_LOWMEM_READ_BUSERR(address, "read 32-bit")
    if (address < addr68kVideo)
    {
        // read regular memory and convert to big-endian
        return getAtariBE32(addrOpcodeROM + address);
    }

    if (address < addr68kVideoEnd)
    {
        // read from host's video memory
        if (gbAtariVideoRamHostEndian)
        {
            // x86 has bgr instead of rgb
            return *((uint32_t *) (hostVideoAddr + (address - addr68kVideo)));
        }
        else
        {
            return getAtariBE32(hostVideoAddr + (address - addr68kVideo));
        }
    }

    // Overflow region for graphics driver (MVDI and NVDI)
    // 4 for mode 8
    // 6 for mode 4ip
    if (address < addr68kVideoEnd + 6)
    {
        return 0;
    }

    bool b_success;
    uint32_t datum = CRegisterModel::read_reg(address, 4, &b_success);
    if (b_success)
    {
        return datum;
    }

    // handle access error
    const char *procName;
    uint32_t act_pd;
    getAtariPrg(&procName, &act_pd);

    DebugError("m68k_read_memory_32(addr = 0x%08lx) --- suppressed bus error by process %s", address, procName);
    return(0xffffffff);        // in fact this was a bus error
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: write a single byte
 *
 * @param[in] address       68k address
 * @param[in] value         datum to write
 *
 ************************************************************************************************/
void m68k_write_memory_8(m68k_addr_type address, m68k_data_type value)
{
#if defined(M68K_WRITE_WATCHES)
    if (is_write_watch(address))
    {
        printf("write.8 value 0x%02x to 68k addr 0x%08x\n", value, address);
    }
#endif

    HANDLE_LOWMEM_WRITE_BUSERR(address, "write byte")
    WATCH_68K_VECTOR_CHANGE(address, value);       // although non-32-bit access is improbable...
    WRITE_PROTECT_OS(address, "write byte")

    if (address < addr68kVideo)
    {
        // write regular memory
        *((uint8_t *) (addrOpcodeROM + address)) = (uint8_t) value;
        return;
    }

    if (address < addr68kVideoEnd)
    {
        address -= addr68kVideo;
        *((uint8_t *) (hostVideoAddr + address)) = (uint8_t) value;
        atomic_store(&gbAtariVideoBufChanged, true);
        return;
    }

    bool b_success;
    CRegisterModel::write_reg(address, 1, value, &b_success);
    if (b_success)
    {
        return;
    }

    const char *procName;
    uint32_t act_pd;
    getAtariPrg(&procName, &act_pd);

    DebugError2("(adr = 0x%08lx, dat = 0x%02hx) --- access error (%s) by process %s",
                    address, (uint8_t) value, AtariAddr2Description(address), procName);

    if ((address == 0xffff8201) || (address == 0xffff8203) || (address == 0xffff820d))
    {
        DebugWarning2("() --- Access to \"Video Base Register\" ignored to make PD.PRG working!");
    }
    else
    if (address == 0xfffffa11)
    {
        DebugWarning2("() --- Access to \"MFP ISRB\" ignored to make ZBENCH.APP working!");
    }
    else
    if (address == 0xffff820a)
    {
        DebugWarning2("() --- Access to \"Video Sync Mode\" ignored!");
    }
    else
    {
        DebugError2("(adr = 0x%08lx, dat = 0x%02hx) --- bus error (%s) by process %s",
                    address, (uint8_t) value, AtariAddr2Description(address), procName);
        sendBusError(address, "write byte");
    }
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: write a single 16-bit halfword in big-endian order
 *
 * @param[in] address       68k address
 * @param[in] value         datum to write
 *
 ************************************************************************************************/
void m68k_write_memory_16(m68k_addr_type address, m68k_data_type value)
{
#if defined(M68K_WRITE_WATCHES)
    if (is_write_watch(address))
    {
        printf("write.8 value 0x%02x to 68k addr 0x%08x\n", value, address);
    }
#endif

    HANDLE_LOWMEM_WRITE_BUSERR(address, "write 16-bit")
    WATCH_68K_VECTOR_CHANGE(address, value);       // although non-32-bit access is improbable...
    WRITE_PROTECT_OS(address, "write 16-bit")

    if (address < addr68kVideo)
    {
        // write regular 68k memory (big endian)
        setAtariBE16(addrOpcodeROM + address, value);
        return;
    }

    if (address < addr68kVideoEnd)
    {
        address -= addr68kVideo;
        if (gbAtariVideoRamHostEndian)
        {
            // x86 has bgr instead of rgb
            *((uint16_t *) (hostVideoAddr + address)) = (uint16_t) value;
        }
        else
        {
            setAtariBE16(hostVideoAddr + address, value);
        }

        atomic_store(&gbAtariVideoBufChanged, true);
        return;
    }

    // Overflow region for graphics driver (MVDI and NVDI)
    // 4 for mode 2ip
    // 8 for mode 4ip
    if (address < addr68kVideoEnd + 8)
    {
        return;
    }

    bool b_success;
    CRegisterModel::write_reg(address, 2, value, &b_success);
    if (b_success)
    {
        return;
    }

    const char *procName;
    uint32_t act_pd;
    getAtariPrg(&procName, &act_pd);

    DebugError("m68k_write_memory_16(addr = 0x%08lx, dat = 0x%04hx) --- bus error (%s) by process %s",
                    address, (uint16_t) value, AtariAddr2Description(address), procName);
    sendBusError(address, "write word");
}


/** **********************************************************************************************
 *
 * @brief 68k-Emu function: write a single 32-bit word in big-endian order
 *
 * @param[in] address       68k address
 * @param[in] value         datum to write
 *
 ************************************************************************************************/
void m68k_write_memory_32(m68k_addr_type address, m68k_data_type value)
{
#if defined(M68K_WRITE_WATCHES)
    if (is_write_watch(address))
    {
        printf("write.8 value 0x%02x to 68k addr 0x%08x\n", value, address);
    }
#endif

    HANDLE_LOWMEM_WRITE_BUSERR(address, "write 32-bit")
    WATCH_68K_VECTOR_CHANGE(address, value);       // process changed 68k exception vector
    WRITE_PROTECT_OS(address, "write 32-bit")

    if (address < addr68kVideo)
    {
        // write regular 68k memory (big endian)
        setAtariBE32(addrOpcodeROM + address, value);
        return;
    }

    if (address < addr68kVideoEnd)
    {
        address -= addr68kVideo;
        if (gbAtariVideoRamHostEndian)
        {
            // x86 has brg instead of rgb
            *((uint32_t *) (hostVideoAddr + address)) = value;
        }
        else
        {
            setAtariBE32(hostVideoAddr + address, value);
        }

        atomic_store(&gbAtariVideoBufChanged, true);
        return;
    }

    // Overflow region for graphics driver (MVDI and NVDI)
    // 4 for mode 8
    // 6 for mode 4ip
    if (address < addr68kVideoEnd + 6)
    {
        return;
    }

    bool b_success;
    CRegisterModel::write_reg(address, 4, value, &b_success);
    if (b_success)
    {
        return;
    }

    const char *procName;
    uint32_t act_pd;
    getAtariPrg(&procName, &act_pd);

    DebugError("m68k_write_memory_32(addr = 0x%08lx, dat = 0x%08lx) --- bus error (%s) by process %s",
                    address, value, AtariAddr2Description(address), procName);
    sendBusError(address, "write long");
}

} // end extern "C"
