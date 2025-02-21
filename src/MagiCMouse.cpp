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
* Enth�lt alles, was mit der Atari-Maus zu tun hat
*
*/

#include "config.h"
// System-Header
// Programm-Header
#include "Globals.h"
#include "osd_cpu.h"
#include "Debug.h"
#include "MagiCMouse.h"

// Schalter

#define	GCURX	-0x25a
#define	GCURY	-0x258
#define	M_HID_CT	-0x256
#define	CURX		-0x158
#define	CURY		-0x156


/**********************************************************************
*
* Konstruktor
*
**********************************************************************/

CMagiCMouse::CMagiCMouse()
{
	m_bActAtariMouseButton[0] = m_bActAtariMouseButton[1] = false;
	m_bActMacMouseButton[0] = m_bActMacMouseButton[1] = false;
	m_pLineAVars = NULL;
}


/**********************************************************************
*
* Destruktor
*
**********************************************************************/

CMagiCMouse::~CMagiCMouse()
{
}


/**********************************************************************
*
* Initialisierung
*
**********************************************************************/

void CMagiCMouse::Init(unsigned char *pLineAVars, Point PtPos)
{
	m_pLineAVars = pLineAVars;
	m_PtActAtariPos = PtPos;
}


/**********************************************************************
*
* Neue Mausposition �bergeben
*
* R�ckgabe: true = Mausbewegung notwendig false = keine Mausbewegung
*
**********************************************************************/

bool CMagiCMouse::SetNewPosition(Point PtPos)
{
	if	(m_pLineAVars)
	{
		m_PtActMacPos = PtPos;
		// get current Atari mouse position from Atari memory (big endian)
		m_PtActAtariPos.y = getAtariBE16(m_pLineAVars + CURY);
		m_PtActAtariPos.x = getAtariBE16(m_pLineAVars + CURX);
		return((m_PtActMacPos.y != m_PtActAtariPos.y) || (m_PtActMacPos.x != m_PtActAtariPos.x));
	}
	else
		return false;
}


/**********************************************************************
*
* Neuen Maustastenstatus �bergeben
*
* R�ckgabe: true = Maustasten-Aktualisierung notwendig / false = Maustasten unver�ndert
*
**********************************************************************/

bool CMagiCMouse::SetNewButtonState(unsigned int NumOfButton, bool bIsDown)
{
	if	(NumOfButton < 2)
	m_bActMacMouseButton[NumOfButton] = bIsDown;
	return(m_bActMacMouseButton[NumOfButton] != m_bActAtariMouseButton[NumOfButton]);
}


/**********************************************************************
*
* Mauspaket liefern
*
* R�ckgabe: true = Mausbewegung notwendig false = keine Mausbewegung
*
**********************************************************************/

bool CMagiCMouse::GetNewPositionAndButtonState(char packet[3])
{
	int xdiff,ydiff;
	char packetcode;


	xdiff = m_PtActMacPos.x - m_PtActAtariPos.x;
	ydiff = m_PtActMacPos.y - m_PtActAtariPos.y;

	if	((!xdiff) && (!ydiff) &&
		 (m_bActAtariMouseButton[0] == m_bActMacMouseButton[0]) &&
		 (m_bActAtariMouseButton[1] == m_bActMacMouseButton[1]))
		return(false);	// keine Bewegung/Taste notwendig

	if	(packet)
		{
		packetcode = '\xf8';
		if	(m_bActMacMouseButton[0])
			packetcode += 2;
		if	(m_bActMacMouseButton[1])
			packetcode += 1;
		m_bActAtariMouseButton[0] = m_bActMacMouseButton[0];
		m_bActAtariMouseButton[1] = m_bActMacMouseButton[1];
		*packet++ = packetcode;

		if	(abs(xdiff) < 128)
			*packet = (char) xdiff;
		else	*packet = (xdiff > 0) ? (char) 127 : (char) -127;
		m_PtActAtariPos.x += *packet++;

		if	(abs(ydiff) < 128)
			*packet = (char) ydiff;
		else	*packet = (ydiff > 0) ? (char) 127 : (char) -127;
		m_PtActAtariPos.y += *packet++;
		}

	return(true);
}
