/*
 * Copyright (C) 1990-2025 Andreas Kromke, andreas.kromke@gmail.com
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
* Manages all about the Atari keyboard
*
*/

#ifndef _MAGICKEYBOARD_INCLUDED_
#define _MAGICKEYBOARD_INCLUDED_

// System-Header
// Programm-Header

// Schalter

class CMagiCKeyboard
{
   public:
	// Konstruktor
	CMagiCKeyboard();
	// Destruktor
	~CMagiCKeyboard();
	// Initialisieren
	static int Init(void);
	// Scancode umrechnen
	unsigned char GetScanCode(uint32_t message);
	static unsigned char SdlScanCode2AtariScanCode(int s);
	#if 0
	unsigned char GetModifierScanCode(uint32_t modifiers, bool *bAutoBreak);
	#endif
   private:
   	uint32_t m_modifiers;
	static const unsigned char *s_tabScancodeMac2Atari;
	static const unsigned char s_convtab[128];
	static const uint32_t s_modconvtab[16];
#ifdef PATCH_DEAD_KEYS
	char *m_OldKbTable;
	size_t m_OldKbTableLen;
#endif
};

#endif

