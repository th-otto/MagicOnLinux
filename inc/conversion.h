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
#include "Globals.h"

#include <time.h>

class CConversion
{
   public:
	static void init(void);
    static unsigned charHost2Atari(const char *utf8, unsigned char *dst);
    static unsigned charAtari2Host(unsigned char c, char *dst);
    static unsigned char charAtari2UpperCase(unsigned char c);
    static unsigned char charAtari2LowerCase(unsigned char c);
    static unsigned hostStringLength(const char *utf8, bool crlf_conv);
    static unsigned strHost2Atari(const char *utf8, uint8_t *buf, unsigned buflen, bool crlf_conv);
    static unsigned atariStringHostLength(const unsigned char *src, bool crlf_conv);
    static unsigned strAtari2Host(const unsigned char *src, char *buf, unsigned buflen, bool crlf_conv);
    static void *readFileToBuffer(const char *filename, unsigned add_bufsiz, unsigned *pread_count);
    static void convTextFileAtari2Host(const char *filename, char **pBuffer);
    static void convTextFileHost2Atari(const char *filename, uint8_t **pBuffer);

    static const char *textAtari2Host(const unsigned char *atari_text);
	static int host2AtariError(int error);
	static void hostDateToDosDate(time_t host_time, uint16_t *time, uint16_t *date);
	static void dosDateToHostDate(uint16_t time, uint16_t date, time_t *host_time);
    static char *copyString(const char *s);
};
