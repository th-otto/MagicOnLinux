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
* Manages user interaction (GUI), mainly message dialogues
*
*/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "Debug.h"
#include "Atari.h"
#include "emulation_globals.h"
#include "gui.h"


// We need a temporary file, because otherwise we get a syntax
// error for the long exception error message.
// To make sure that the message window will not get too small horizontally,
// send some space characters.
int showDialogue(const char *msg_text, const char *info_txt, const char *buttons)
{
    const char *title = "MagicOnLinux";
    char fname[] = "/tmp/magic-on-linux_XXXXXX";
    // TODO: find some reasonable unicode long space
    const char *spaces = "                                                                                                    ";
    char text[512];

    FILE *f = nullptr;
    int fd = mkstemp(fname);
    if (fd >= 0)
    {
        f = fdopen(fd, "wt");
    }
    if (f != nullptr)
    {
        fprintf(f, "%s\n\n%s\n%s\n",msg_text, info_txt, spaces);
        sprintf(text, "-file \"%s\"", fname);
        fclose(f);
    }
    else
    {
        sprintf(text, "%s\n\n%s\n", msg_text, info_txt);
    }

    char command[1024];
    sprintf(command, "gxmessage -nearmouse -wrap -title \"%s\" -buttons \"%s\" %s", title, buttons, text);
    // Contrary to man page, we do not get 101, 102, 103 for the buttons, but the value is
    // shifted by 8, so that we receive 0x6500, 0x6600 and 0x6700 for buttons and 0x0100 for window close.
    //fprintf(stderr, "%s\n", command);
    int result = system(command);
    unlink(fname);
    //fprintf(stderr, "-> %d (0x%08x)\n", result, result);

    return result >> 8;
}


// We need a temporary file, because otherwise we get a syntax
// error for the long exception error message.
// To make sure that the message window will not get too small horizontally,
// send some space characters.
int showAlert(const char *msg_text, const char *info_txt)
{
    return showDialogue(msg_text, info_txt, "OK");
}


const char *exception68kToName(unsigned exception_no)
{
    static char buf[32];

    exception_no <<= 2;     // convert from exception number to exception vector

    if ((exception_no >= INTV_32_TRAP_0) && (exception_no <= INTV_47_TRAP_15))
    {
        sprintf(buf, "Trap %u", (exception_no - INTV_32_TRAP_0) >> 2);
    }
    else
    switch (exception_no)
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
        case INTV_56:               strcpy(buf, "MMU configuration"); break;
        default:                    strcpy(buf, "(other)"); break;
    }

    return buf;
}


static void GuiAtariCrash
(
    unsigned exception_no,
    uint32_t err_addr,
    const char *access_mode,
    uint32_t pc,                // host-endian
    uint16_t sr,                // host-endian
    uint32_t usp,               // host-endian
    const uint32_t *pDx,        // big-endian
    const uint32_t *pAx,        // big-endian
    const char *proc_path,
    uint32_t pd                 // host-endian
)
{
    char text[1024] = "";
    char srbits[32] = "";
    if (sr & 0x8000)
    {
        strcat(srbits, "TRC ");
    }
    if (sr & 0x2000)
    {
        strcat(srbits, "SUP ");
    }
    if (sr & 0x0010)
    {
        strcat(srbits, "EXT");
    }
    if (sr & 0x0008)
    {
        strcat(srbits, "NEG ");
    }
    if (sr & 0x0004)
    {
        strcat(srbits, "ZER ");
    }
    if (sr & 0x0002)
    {
        strcat(srbits, "OVF ");
    }
    if (sr & 0x0001)
    {
        strcat(srbits, "CRY ");
    }
    sprintf(srbits + strlen(srbits), "INT=%u", (sr >> 8) & 7);

    bool pc_is_rom = (pc >= addrOsRomStart) && (pc < addrOsRomEnd);
    sprintf(text + strlen(text), "    exc = %s (%u)\n", exception68kToName(exception_no), exception_no);
    sprintf(text + strlen(text), "    exception address = 0x%08x (%s)\n", err_addr, AtariAddr2Description(err_addr));
    sprintf(text + strlen(text), "    AccessMode = %s\n", access_mode);
    if (pc_is_rom)
    {
        sprintf(text + strlen(text), "    pc = 0x%08x (OS ROM 0x%06x)\n", pc, pc - addrOsRomStart);
        sprintf(text + strlen(text), "         code = 0x%02x%02x [0x%02x%02x] 0x%02x%02x\n",
                                        mem68k[pc - 2], mem68k[pc - 1], mem68k[pc], mem68k[pc + 1], mem68k[pc + 2], mem68k[pc + 3]);
    }
    else
    {
        sprintf(text + strlen(text), "    pc = 0x%08x\n", pc);
    }
    sprintf(text + strlen(text), "    sr = 0x%04x (%s)\n", sr, srbits);
    sprintf(text + strlen(text), "    usp = 0x%08x\n", usp);
    for (int i = 0; i < 8; i++)
    {
        sprintf(text + strlen(text), "     d%i = 0x%08x\n", i, be32toh(pDx[i]));
    }
    for (int i = 0; i < 8; i++)
    {
        sprintf(text + strlen(text), "     a%i = 0x%08x\n", i, be32toh(pAx[i]));
    }
    sprintf(text + strlen(text), "    ProcPath = %s\n", proc_path);
    sprintf(text + strlen(text), "    pd = 0x%08x\n", pd);
    (void) showAlert("Atari crash", text);
}


void send68kExceptionData(
                         unsigned exception_no,
                         uint32_t ErrAddr,
                         char *AccessMode,
                         uint32_t pc,
                         uint16_t sr,
                         uint32_t usp,
                         uint32_t *pDx,
                         uint32_t *pAx,
                         const char *ProcPath,
                         uint32_t pd)
{
    GuiAtariCrash(exception_no, ErrAddr, AccessMode, pc, sr, usp, pDx, pAx, ProcPath, pd);
}
