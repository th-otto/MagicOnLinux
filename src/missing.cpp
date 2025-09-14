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

//
//  missing.c
//  SDLOpenGLIntro
//
//  Created by Andreas Kromke on 07.10.11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include "config.h"
// System-Header
// Programm-Header
#include "Globals.h"
#include "Debug.h"
#include "missing.h"

// TODO: make a GUI from this
static int GuiMyAlert(const char *msg_text, const char *info_txt, int nButtons)
{
	fprintf(stderr, "Alert (%d buttons):\n", nButtons);
	fprintf(stderr, "  MSG  %s\n", msg_text);
	fprintf(stderr, "  INFO %s\n", info_txt);
	return 0;
}

// TODO: make a GUI from this
static void GuiAtariCrash
(
	uint16_t exc,
	uint32_t ErrAddr,
	const char *AccessMode,
	uint32_t pc,
	uint16_t sr,
	uint32_t usp,
	const uint32_t *pDx,		// TODO: Is this big endian or host endian?
	const uint32_t *pAx,
	const char *ProcPath,
	uint32_t pd
)
{
	fprintf(stderr, "Atari crash:\n");
	fprintf(stderr, "    exc = %u\n", exc);
	fprintf(stderr, "    ErrAddr = 0x%08x\n", ErrAddr);
	fprintf(stderr, "    AccessMode = %s\n", AccessMode);
	fprintf(stderr, "    pc = 0x%08x\n", pc);
	fprintf(stderr, "    sr = 0x%04x\n", sr);
	fprintf(stderr, "    usp = 0x%08x\n", usp);
	for (int i = 0; i < 8; i++)
	{
		fprintf(stderr, "     d%i = 0x%08x\n", i, pDx[i]);
	}
	for (int i = 0; i < 8; i++)
	{
		fprintf(stderr, "     a%i = 0x%08x\n", i, pAx[i]);
	}
	fprintf(stderr, "    ProcPath = %s\n", ProcPath);
	fprintf(stderr, "    pd = 0x%08x\n", pd);
}



#if 0
// TODO: implement
int MyAlert(int alertID, int alertType)
{
	const char *msgtxt = "unknown message";
	const char *inftxt = "unknown information";
	int nButtons;

	switch(alertID)
	{
		case ALRT_NO_VIDEO_DRIVER:
			msgtxt = "Keinen passenden Bildschirmtreiber gefunden.";
			inftxt = "Das Atari-Dateisystem ist möglicherweise unvollständig, und die Treiberdatei fehlt.";
			break;

		case ALRT_NO_MAGICMAC_OS:
			msgtxt = "Die Datei mit dem Atari-Kernel fehlt.";
			inftxt = "Keine Datei mit dem Namen \"MagicMacX.OS\" im Programmpaket gefunden.";
			break;

		case ALRT_NOT_ENOUGH_MEM:
			msgtxt = "Zuwenig Speicher.";
			break;

		case ALRT_INVALID_MAGICMAC_OS:
			msgtxt = "Die Atari-Kerneldatei ist beschädigt.";
			inftxt = "Die Datei \"MagicMacX.OS\" wurde verändert.";
			break;

		case ALRT_RESTART_APPLICATION:
			msgtxt = "Der Atari-Emulator muß neu gestartet werden.";
			inftxt = "Die Änderungen an den Einstellungen können nicht im Betrieb übernommen werden.";
			break;

		case ALRT_QUIT_INSTANTLY:
			msgtxt = "Der Atari-Emulator wird sofort beendet.";
			break;

		case ALRT_INVALID_MONITOR:
			break;

		case ALRT_DEMO:
			msgtxt = "Der Atari-Emulator ist eine Demo-Version.";
			inftxt = "Schreibzugriffe sind in der Demo-Version deaktiviert.";
			break;

		case ALRT_MIN_ATARI_SCREEN_SIZE:
			msgtxt = "Die Bildschirmgröße wurde automatisch korrigiert.";
			inftxt = "Der Atari-Emulator benötigt mindestens 320 mal 200 Bildpunkte.";
			break;

		case ALRT_DISPLAY_CHANGED:
			break;

		default:
			break;
	}

	if (alertType == kAlertStopAlert)
		nButtons = 1;
	else
		nButtons = 2;

	GuiMyAlert(msgtxt, inftxt, nButtons);
	return 0;
}
#endif

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
void SendSysHaltReason(const char *Reason)
{
	GuiMyAlert("Der Emulator wurde angehalten", Reason, 1);
}
void MMX_BeginDialog(void)
{
	//GuiShowMouse();
}
void MMX_EndDialog(void)
{
}

CMagiCScreen::CMagiCScreen(void)
{
	memset(this, 0, sizeof(*this));
}
CMagiCScreen::~CMagiCScreen(void)
{
}
