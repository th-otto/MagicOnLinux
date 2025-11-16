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
* Character conversions between Atari and host
*
*/

#include "Atari.h"

// endian conversion helpers

#define getAtariBE16(addr) \
    be16toh(*((uint16_t *) (addr)));
#define getAtariBE32(addr) \
    be32toh(*((uint32_t *) (addr)));
#define setAtariBE16(addr, val) \
    *((uint16_t *) (addr)) = htobe16(val);
#define setAtariBE32(addr, val) \
    *((uint32_t *) (addr)) = htobe32(val);
#define setAtari32(addr, val) \
    *((uint32_t *) (addr)) = val;

class CConversion
{
   public:
	static void init(void);
    static unsigned charHost2Atari(const char *utf8, unsigned char *dst);
    static unsigned charAtari2Host(unsigned char c, char *dst);
    static unsigned char charAtari2UpperCase(unsigned char c);
    static unsigned hostStringLength(const char *utf8, bool crlf_conv);
    static unsigned strHost2Atari(const char *utf8, uint8_t *buf, unsigned buflen, bool crlf_conv);

    static const char *textAtari2Host(const unsigned char *atari_text);
	static int host2AtariError(int error);
	static void hostDateToDosDate(time_t host_time, uint16_t *time, uint16_t *date);
	static void dosDateToHostDate(uint16_t time, uint16_t date, time_t *host_time);
    static char *copyString(const char *s);
};
