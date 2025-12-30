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
* Manages the Atari mouse
*
*/

#include <stdlib.h>
#include "config.h"
#include "Globals.h"
#include "osd_cpu.h"
#include "Debug.h"
#include "preferences.h"
#include "MagiCMouse.h"


#define GCURX    -0x25a
#define GCURY    -0x258
#define M_HID_CT -0x256
#define CURX     -0x158
#define CURY     -0x156

uint8_t *CMagiCMouse::m_pLineAVars;
int CMagiCMouse::m_actAtariPosX;             // current position
int CMagiCMouse::m_actAtariPosY;
int CMagiCMouse::m_actHostPosX;              // desired position
int CMagiCMouse::m_actHostPosY;              // desired position
double CMagiCMouse::m_actHostMovPosX;
double CMagiCMouse::m_actHostMovPosY;
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
int CMagiCMouse::init(uint8_t *pLineAVars, int posX, int posY)
{
    m_bActAtariMouseButton[0] = m_bActAtariMouseButton[1] = false;
    m_bActHostMouseButton[0] = m_bActHostMouseButton[1] = false;

    m_pLineAVars = pLineAVars;
    m_actAtariPosX = posX;
    m_actAtariPosY = posY;

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Pass new mouse pointer movement from host to emulated system
 *
 * @param[in]  PtPos    new relative mouse pointer movement vector
 *
 * @return true: Atari mouse must be moved, false: no movement necessary
 *
 ************************************************************************************************/
bool CMagiCMouse::setNewMovement(double vx, double vy)
{
    if (m_pLineAVars != nullptr)
    {
        m_actHostMovPosX += vx;        // accumulate fractions
        m_actHostMovPosY += vy;
        return ((int) m_actHostMovPosX != 0) || ((int) m_actHostMovPosY != 0);
    }
    else
        return false;
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
bool CMagiCMouse::setNewPosition(int posX, int posY)
{
    if (m_pLineAVars != nullptr)
    {
        m_actHostPosY = posY;
        m_actHostPosX = posX;
        // get current Atari mouse position from Atari memory (big endian)
        m_actAtariPosY = getAtariBE16(m_pLineAVars + CURY);
        m_actAtariPosX = getAtariBE16(m_pLineAVars + CURX);
        return (m_actHostPosY != m_actAtariPosY) || (m_actHostPosX != m_actAtariPosX);
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
bool CMagiCMouse::getNewPositionAndButtonState(int8_t packet[3])
{
    int xdiff,ydiff;
    int8_t packetcode;

    // Determine the way to go to the desired mouse pointer position
    if (Preferences::bRelativeMouse)
    {
        xdiff = (int) m_actHostMovPosX;
        ydiff = (int) m_actHostMovPosY;
    }
    else
    {
        xdiff = m_actHostPosX - m_actAtariPosX;
        ydiff = m_actHostPosY - m_actAtariPosY;
    }

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
            *packet = (int8_t) xdiff;
        else
            *packet = (xdiff > 0) ? (int8_t) 127 : (int8_t) -127;
        if (Preferences::bRelativeMouse)
        {
           m_actHostMovPosX -= *packet++;
        }
        else
        {
            m_actAtariPosX += *packet++;
        }

        // The mouse packet allows vertical movements up to 127 pixels.
        if (abs(ydiff) < 128)
            *packet = (int8_t) ydiff;
        else
            *packet = (ydiff > 0) ? (int8_t) 127 : (int8_t) -127;
        if (Preferences::bRelativeMouse)
        {
            m_actHostMovPosY -= *packet++;
        }
        else
        {
            m_actAtariPosY += *packet++;
        }
    }

    return true;
}
