/*
 * Copyright (C) 1990-2018 Andreas Kromke, andreas.kromke@gmail.com
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

#include "Debug.h"
#include "gui.h"

// We need a temporary file, because otherwise we get a syntax
// error for the long exception error message.
// To make sure that the message window will not get too small horizontally,
// send some space characters.
int showAlert(const char *msg_text, const char *info_txt, int nButtons)
{
    const char *title = "MagicOnLinux";
    // TODO: find some reasonable unicode long space
    const char *spaces = "                                                                                                    ";
    char text[512];
    char buttons[64] = "";

    fprintf(stderr, "Alert (%d buttons):\n", nButtons);
    fprintf(stderr, "  MSG  %s\n", msg_text);
    fprintf(stderr, "  INFO %s\n", info_txt);

    FILE *f = nullptr;
    char *fname = tempnam(nullptr, "magic-on-linux");
    if (fname != nullptr)
    {
        f = fopen(fname, "wt");
    }
    if (f != nullptr)
    {
        fprintf(f, "%s\n%s\n\n%s", spaces, msg_text, info_txt);
        sprintf(text, "-file \"%s\"", fname);
        fclose(f);
    }
    else
    {
        sprintf(text, "%s\n\n%s", msg_text, info_txt);
    }

    switch(nButtons)
    {
        case 1:
            sprintf(buttons, "-buttons \"OK\"");
            break;
        case 2:
            sprintf(buttons, "-buttons \"OK,CANCEL\"");
            break;
        case 3:
            sprintf(buttons, "-buttons \"OK,CANCEL,IGNORE\"");
            break;
    }

    char command[1024];
    sprintf(command, "gxmessage -nearmouse -wrap -title \"%s\" %s %s", title, buttons, text);
    // Contrary to man page, we do not get 101, 102, 103 for the buttons, but the value is
    // shifted by 8, so that we receive 0x6500, 0x6600 and 0x6700 for buttons and 0x0100 for window close.
    int result = system(command);
    fprintf(stderr, "-> %d (0x%08x)\n", result, result);

    return result >> 8;
}

static void GuiAtariCrash
(
    uint16_t exc,
    uint32_t ErrAddr,
    const char *AccessMode,
    uint32_t pc,
    uint16_t sr,
    uint32_t usp,
    const uint32_t *pDx,        // TODO: Is this big endian or host endian?
    const uint32_t *pAx,
    const char *ProcPath,
    uint32_t pd
)
{
    char text[1024] = "";
    sprintf(text + strlen(text), "    exc = %u\n", exc);
    sprintf(text + strlen(text), "    ErrAddr = 0x%08x\n", ErrAddr);
    sprintf(text + strlen(text), "    AccessMode = %s\n", AccessMode);
    sprintf(text + strlen(text), "    pc = 0x%08x\n", pc);
    sprintf(text + strlen(text), "    sr = 0x%04x\n", sr);
    sprintf(text + strlen(text), "    usp = 0x%08x\n", usp);
    for (int i = 0; i < 8; i++)
    {
        sprintf(text + strlen(text), "     d%i = 0x%08x\n", i, pDx[i]);
    }
    for (int i = 0; i < 8; i++)
    {
        sprintf(text + strlen(text), "     a%i = 0x%08x\n", i, pAx[i]);
    }
    sprintf(text + strlen(text), "    ProcPath = %s\n", ProcPath);
    sprintf(text + strlen(text), "    pd = 0x%08x\n", pd);
    (void) showAlert("Atari crash", text, 1);
}


void Send68kExceptionData(
                         uint16_t exc,
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
    GuiAtariCrash(exc, ErrAddr, AccessMode, pc, sr, usp, pDx, pAx, ProcPath, pd);
}
