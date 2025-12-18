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
* Manages the MagiC screen
*
*/

#include "config.h"
#include <string.h>
#include "MagiCScreen.h"


MXVDI_PIXMAP CMagiCScreen::m_PixMap;
uint32_t CMagiCScreen::m_pColourTable[MAGIC_COLOR_TABLE_LEN];

int CMagiCScreen::init(void)
{
    memset(&m_PixMap, 0, sizeof(m_PixMap));
    memset(&m_pColourTable, 0, sizeof(m_pColourTable));
    return 0;
}
