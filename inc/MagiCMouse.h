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

#ifndef _MAGICMOUSE_INCLUDED_
#define _MAGICMOUSE_INCLUDED_


class CMagiCMouse
{
   public:
    static int init(uint8_t *pLineAVars, Point PtPos);
    static bool setNewPosition(Point PtPos);
    static bool setNewMovement(double vx, double vy);
    static bool setNewButtonState(unsigned int NumOfButton, bool bIsDown);
    static bool getNewPositionAndButtonState(int8_t packet[3]);

   private:
    static uint8_t *m_pLineAVars;
    static Point m_PtActAtariPos;           // current
    static Point m_PtActHostPos;            // goal
    static double mPtActHostMovPosX;        // accumulated movement vector
    static double mPtActHostMovPosY;
    static bool m_bActAtariMouseButton[2];  // current
    static bool m_bActHostMouseButton[2];   // goal
};

#endif
