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
#include "Globals.h"
#include "Debug.h"
#include "Atari.h"
#include "emulation_globals.h"
#include "gui.h"


#if !defined(__APPLE__)
/** **********************************************************************************************
 *
 * @brief Show a simple text dialogue with decision buttons in Linux, using gxmessage
 *
 * @param[in]  msg_text         main message text, may be shown as headline
 * @param[in]  info_txt         message details, may be shown in a smaller font
 * @param[in]  buttons          list of buttons, comma separated
 *
 * @return 101, 102, 103, ... for the buttons
 * @retval 1    message window was closed
 *
 * @note We need a temporary file, because otherwise we get a syntax error for the long
 *       exception error message. To make sure that the message window will not get too small
 *       horizontally, send some space characters.
 *
 ************************************************************************************************/
int showDialogue(const char *msg_text, const char *info_txt, const char *buttons)
{
    const char *title = "MagicOnLinux";
    char *command;

    /*
     * try gxmessage first
     */
    command = NULL;
    asprintf(&command, "gxmessage -nearmouse -wrap -title '%s' -buttons '%s' '%s\n\n%s'", title, buttons, msg_text, info_txt);
    // Contrary to man page, we do not get 101, 102, 103 for the buttons, but the value is
    // shifted by 8, so that we receive 0x6500, 0x6600 and 0x6700 for buttons and 0x0100 for window close.
    //fprintf(stderr, "%s\n", command);
    int result = system(command);
    free(command);
    if (result == 127)
    {
        /*
         * gxmessage not found, try alternatives (TODO)
         */
        result = -1;
    } else
    {
        result = result >> 8;
    }
    //fprintf(stderr, "-> %d (0x%08x)\n", result, result);
    return result;
}
#endif


#if defined(__APPLE__)
/** **********************************************************************************************
 *
 * @brief Show a simple text dialogue with decision buttons in macOS, using osascript with AppleScript
 *
 * @param[in]  msg_text         main message text, may be shown as headline
 * @param[in]  info_txt         message details, may be shown in a smaller font
 * @param[in]  buttons          list of buttons, comma separated
 *
 * @return 101, 102, 103, ... for the buttons
 * @retval 1    message window was closed
 *
 ************************************************************************************************/
int showDialogue(const char *msg_text, const char *info_txt, const char *buttons)
{
    const char *title = "MagicOnLinux";
    char *command;
    char escaped_msg[512];
    char escaped_info[512];

    (void)buttons;
    // Simple escape for AppleScript strings - replace quotes with escaped quotes
    const char *src = msg_text;
    char *dst = escaped_msg;
    while (*src && (size_t)(dst - escaped_msg) < sizeof(escaped_msg) - 2)
    {
        if (*src == '"' || *src == '\\')
        {
            *dst++ = '\\';
        }
        *dst++ = *src++;
    }
    *dst = '\0';

    src = info_txt;
    dst = escaped_info;
    while (*src && (size_t)(dst - escaped_info) < sizeof(escaped_info) - 2)
    {
        if (*src == '"' || *src == '\\')
        {
            *dst++ = '\\';
        }
        *dst++ = *src++;
    }
    *dst = '\0';

    // Build AppleScript command - for now, ignore custom buttons and just use OK
    command = NULL;
    asprintf(&command,
             "osascript -e 'tell app \"System Events\" to display dialog \"%s\\n\\n%s\" with title \"%s\" buttons {\"OK\"} default button \"OK\"'",
             escaped_msg, escaped_info, title);

    int result = system(command);
    free(command);
    // osascript returns 0 on success (button clicked)
    return (result == 0) ? 0 : 1;
}
#endif


/** **********************************************************************************************
 *
 * @brief Show a simple alert text dialogue with OK button
 *
 * @param[in]  msg_text         main message text, may be shown as headline
 * @param[in]  info_txt         message details, may be shown in a smaller font
 *
 * @retval 101  OK pressed
 * @retval 1    message window was closed
 *
 ************************************************************************************************/
int showAlert(const char *msg_text, const char *info_txt)
{
    return showDialogue(msg_text, info_txt, "OK");
}


/** **********************************************************************************************
 *
 * @brief Show dialogue describing the 68k CPU state when the exception occurred
 *
 * @param[in]  exception_no     68k exception number
 * @param[in]  err_addr         68k address that triggered the exception
 * @param[in]  access_mode      "read byte", "write long" etc.
 * @param[in]  pc               68k program counter (host-endian)
 * @param[in]  sr               68k status register (host-endian)
 * @param[in]  usp              68k user stack pointer (host-endian)
 * @param[in]  pDx              68k data registers (big-endian)
 * @param[in]  pAx              68k address registers (big-endian)
 * @param[in]  proc_path        path of the running process (host-endian)
 * @param[in]  pd               process descriptor (host-endian)
 *
 ************************************************************************************************/
static void GuiAtariCrash
(
    unsigned exception_no,
    uint32_t err_addr,
    const char *access_mode,
    uint32_t pc,
    uint16_t sr,
    uint32_t usp,
    const uint32_t *pDx,
    const uint32_t *pAx,
    const char *proc_path,
    uint32_t pd
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
    sprintf(text + strlen(text), "    exc = %s (%u)\n", exception68k_to_name(exception_no << 2), exception_no);
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


/** **********************************************************************************************
 *
 * @brief Handle 68k exception
 *
 * @param[in]  exception_no     68k exception number
 * @param[in]  err_addr         68k address that triggered the exception
 * @param[in]  access_mode      "read byte", "write long" etc.
 * @param[in]  pc               68k program counter (host-endian)
 * @param[in]  sr               68k status register (host-endian)
 * @param[in]  usp              68k user stack pointer (host-endian)
 * @param[in]  pDx              68k data registers (big-endian)
 * @param[in]  pAx              68k address registers (big-endian)
 * @param[in]  proc_path        path of the running process (host-endian)
 * @param[in]  pd               process descriptor (host-endian)
 *
 ************************************************************************************************/
void send68kExceptionData(
                         unsigned exception_no,
                         uint32_t ErrAddr,
                         const char *AccessMode,
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
