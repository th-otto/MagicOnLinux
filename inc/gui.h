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

#ifndef _GUI_H_
#define _GUI_H_

#include <stdint.h>


extern int showAlert(const char *msg_text, const char *info_txt, int nButtons);
extern int showDialogue(const char *msg_text, const char *info_txt, const char *buttons);

// global function used by CMagiC to report 68k exceptions
extern void Send68kExceptionData(
                uint16_t exc,
                uint32_t ErrAddr,
                char *AccessMode,
                uint32_t pc,
                uint16_t sr,
                uint32_t usp,
                uint32_t *pDx,
                uint32_t *pAx,
                const char *ProcPath,
                uint32_t pd);

#endif
