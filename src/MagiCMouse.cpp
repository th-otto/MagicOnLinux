/*
 * Copyright (C) 1990-2018/25 Andreas Kromke, andreas.kromke@gmail.com
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
* Manages the Atari mouse
*
*/

#include "config.h"
#include "Globals.h"
#include "osd_cpu.h"
#include "Debug.h"
#include "MagiCMouse.h"


#define GCURX    -0x25a
#define GCURY    -0x258
#define M_HID_CT -0x256
#define CURX     -0x158
#define CURY     -0x156

unsigned char *CMagiCMouse::m_pLineAVars;
Point CMagiCMouse::m_PtActAtariPos;             // current position
Point CMagiCMouse::m_PtActHostPos;              // desired position
bool CMagiCMouse::m_bActAtariMouseButton[2];    // current state
bool CMagiCMouse::m_bActHostMouseButton[2];     // desired state


/** **********************************************************************************************
 *
 * @brief Initialisation
 *
 * @param[in]  pLineAVars       host pointer to emulated LineA variables
 * @param[in]  PtPos            initial host mouse postion
 *
 * @return currently returns always zero
 *
 ************************************************************************************************/
int CMagiCMouse::init(unsigned char *pLineAVars, Point PtPos)
{
    m_bActAtariMouseButton[0] = m_bActAtariMouseButton[1] = false;
    m_bActHostMouseButton[0] = m_bActHostMouseButton[1] = false;

    m_pLineAVars = pLineAVars;
    m_PtActAtariPos = PtPos;

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Pass new mouse pointer position from host to emulated system
 *
 * @param[in]  PtPos    new host mouse postion
 *
 * @return true: Atari mouse must be moved, false: no movement necessary
 *
 ************************************************************************************************/
bool CMagiCMouse::setNewPosition(Point PtPos)
{
    if (m_pLineAVars != nullptr)
    {
        m_PtActHostPos = PtPos;
        // get current Atari mouse position from Atari memory (big endian)
        m_PtActAtariPos.y = getAtariBE16(m_pLineAVars + CURY);
        m_PtActAtariPos.x = getAtariBE16(m_pLineAVars + CURX);
        return (m_PtActHostPos.y != m_PtActAtariPos.y) || (m_PtActHostPos.x != m_PtActAtariPos.x);
    }
    else
        return false;
}


/** **********************************************************************************************
 *
 * @brief Pass new mouse button state from host to emulated system
 *
 * @param[in]  NumOfButton      number of mouse button (0 or 1)
 * @param[in]  bIsDown          true: button pressed, false: button released
 *
 * @return true: Atari mouse button state must be updated, false: no update necessary
 *
 ************************************************************************************************/
bool CMagiCMouse::setNewButtonState(unsigned int NumOfButton, bool bIsDown)
{
    if (NumOfButton < 2)
    {
        m_bActHostMouseButton[NumOfButton] = bIsDown;
    }

    return m_bActHostMouseButton[NumOfButton] != m_bActAtariMouseButton[NumOfButton];
}


/** **********************************************************************************************
 *
 * @brief Generate a mouse packet for the Atari
 *
 * @param[out]  packet      buffer to get the mouse packet
 *
 * @return true: Atari mouse button or position state must be updated, false: no update necessary
 *
 * @note The Atari gets mouse packets like keyboard presses from its hardware interface,
 *       mouse packets contain position and button information.
 * @note Atari mouse packets allow relative mouse pointer movements of up to 127 pixels in
 *       both directions. For farther distances more than one packet is needed.
 *
 ************************************************************************************************/
bool CMagiCMouse::getNewPositionAndButtonState(char packet[3])
{
    int xdiff,ydiff;
    char packetcode;

    // Determine the way to go to the desired mouse pointer position
    xdiff = m_PtActHostPos.x - m_PtActAtariPos.x;
    ydiff = m_PtActHostPos.y - m_PtActAtariPos.y;

    // Check if we already reached the desired position and if
    // the Atari has also been informed about the current mouse
    // button states.
    if ((!xdiff) && (!ydiff) &&
         (m_bActAtariMouseButton[0] == m_bActHostMouseButton[0]) &&
         (m_bActAtariMouseButton[1] == m_bActHostMouseButton[1]))
        return false;    // neither movement nor button update necessary

    // compose a mouse packet
    if (packet != nullptr)
    {
        packetcode = '\xf8';
        if (m_bActHostMouseButton[0])
            packetcode += 2;
        if (m_bActHostMouseButton[1])
            packetcode += 1;
        m_bActAtariMouseButton[0] = m_bActHostMouseButton[0];
        m_bActAtariMouseButton[1] = m_bActHostMouseButton[1];
        *packet++ = packetcode;

        // The mouse packet allows horizontal movements up to 127 pixels.
        if (abs(xdiff) < 128)
            *packet = (char) xdiff;
        else
            *packet = (xdiff > 0) ? (char) 127 : (char) -127;
        m_PtActAtariPos.x += *packet++;

        // The mouse packet allows vertical movements up to 127 pixels.
        if (abs(ydiff) < 128)
            *packet = (char) ydiff;
        else
            *packet = (ydiff > 0) ? (char) 127 : (char) -127;
        m_PtActAtariPos.y += *packet++;
    }

    return true;
}
