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
* Character conversions between Atari and host
*
*/

#include <assert.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include "Globals.h"
#include "Debug.h"
#include "conversion.h"

// Atari character codes:
// *H* = hebrew
//         0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
// 0x80 =  Ç   ü   é   â   ä   à   å   ç   ê   ë   è   ï   î   ì   Ä   Å
// 0x90 =  É   æ   Æ   ô   ö   ò   û   ù   ÿ   Ö   Ü   ¢   £   ¥   ß   ƒ
// 0xa0 =  á   í   ó   ú   ñ   Ñ   ª   º   ¿   ⌐   ¬   ½   ¼   ¡   «   »
// 0xb0 =  ã   õ   Ø   ø   œ   Œ   À   Ã   Õ   ¨   ´   †   ¶   ©   ®   ™
// 0xc0 =  ĳ   Ĳ  *H* *H* *H* *H* *H* *H* *H* *H* *H* *H* *H* *H* *H* *H*
// 0xd0 = *H* *H* *H* *H* *H* *H* *H* *H* *H* *H* *H* *H*  *H* §   ∧   ∞
// 0xe0 =  α   β   Γ   π   Σ   σ   µ   τ   Φ   Θ   Ω   δ   ∮   φ   ∈   ∩
// 0xf0 =  ≡   ±   ≥   ≤   ⌠   ⌡   ÷   ≈   °   •   ·   √   ⁿ   ²   ³   ¯

// Atari characters from 0x80 to 0xff, separated by a zero byte. As some of these
// characters are encoded in one byte, others in two, this string is converted to
// a table on startup.
// Hewbrew characters are translated as '_'. TODO: Find a better solution.
static const char *atari2utf8 =
    /* 0x80 */  "Ç ü é â ä à å ç ê ë è ï î ì Ä Å "
    /* 0x90 */  "É æ Æ ô ö ò û ù ÿ Ö Ü ¢ £ ¥ ß ƒ "
    /* 0xa0 */  "á í ó ú ñ Ñ ª º ¿ ⌐ ¬ ½ ¼ ¡ « » "
    /* 0xb0 */  "ã õ Ø ø œ Œ À Ã Õ ¨ ´ † ¶ © ® ™ "
    /* 0xc0 */  "ĳ Ĳ _ _ _ _ _ _ _ _ _ _ _ _ _ _ "
    /* 0xd0 */  "_ _ _ _ _ _ _ _ _ _ _ _ _ § ∧ ∞ "
    /* 0xe0 */  "α β Γ π Σ σ µ τ Φ Θ Ω δ ∮ φ ∈ ∩ "
    /* 0xf0 */  "≡ ± ≥ ≤ ⌠ ⌡ ÷ ≈ ° • · √ ⁿ ² ³ ¯";
static unsigned atari2utf8Index[0x80];


/** **********************************************************************************************
 *
 * @brief [static] Get byte length of utf-8 character
 *
 * @param[in]  c    first byte of utf-8 character
 *
 * @return length of detected utf-8 character in bytes, zero for continuation byte
 *
 * If a byte starts with a 0 bit, it's a single byte value less than 128.
 * If it starts with 11, it's the first byte of a multi-byte sequence,
 * and the number of 1 bits at the start indicates how many bytes there
 * are in total (110xxxxx has two bytes, 1110xxxx has three and 11110xxx has four).
 * If it starts with 10, it's a continuation byte.
 *
 ************************************************************************************************/
static unsigned utf8Len(unsigned char c)
{
    if ((c & 0x80) == 0)
    {
        return 1;       // ASCII starts with %01 or %00
    }
    if ((c & 0xc0) == 0x80)
    {
        return 0;       // continuation starts with %10
    }
    c <<= 2;            // starts with %11
    unsigned len = 2;
    while(c & 0x80)
    {
        c <<= 1;
        len++;
    }
    return len;
}


/** **********************************************************************************************
 *
 * @brief [static] Initialise conversion tables
 *
 ************************************************************************************************/
void CConversion::init(void)
{
    static const char *tab = atari2utf8;
    static const char *tab_e = atari2utf8 + strlen(atari2utf8);

    unsigned index = 0;
    while ((tab < tab_e) && (index < 128))
    {
        atari2utf8Index[index++] = tab - atari2utf8;
        unsigned len = utf8Len(*tab);
        assert(len > 0);

        // skip utf8 character
        tab += len;

        // skip spaces
        while (*tab == ' ')
        {
            tab++;
        }
    }
    assert(index == 128);
}


/** **********************************************************************************************
 *
 * @brief [static] Convert utf-8 byte array to Atari single-byte character
 *
 * @param[in]  utf8     utf-8 character
 * @param[out] dst      Atari single-byte character
 *
 * @return length of detected utf-8 character in bytes
 *
 ************************************************************************************************/
unsigned CConversion::charHost2Atari(const char *utf8, unsigned char *dst)
{
    unsigned len;

    if ((*utf8 & 0x80) == 0)
    {
        // ASCII, no conversion
        *dst = *utf8;
        len = 1;
    }
    else
    {
        // linear search in table. TODO: find better solution
        len = utf8Len(*utf8);
        *dst = '_';     // in case we do not find it
        for (unsigned c = 128; c < 256; c++)
        {
            unsigned index = atari2utf8Index[c - 128];
            const char *putf8 = atari2utf8 + index;
            if (!memcmp(utf8, putf8, len))
            {
                // match
                *dst = c;
                break;
            }
        }
    }

    return len;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert Atari single-byte character to utf-8 byte array
 *
 * @param[in]  c        Atari character
 * @param[out] dst      utf-8 character
 *
 * @return length of generated utf-8 character in bytes
 *
 ************************************************************************************************/
unsigned CConversion::charAtari2Host(unsigned char c, char *dst)
{
    unsigned len;

    if (c < 128)
    {
        *dst = c;
        len = 1;
    }
    else
    {
        unsigned index = atari2utf8Index[c - 128];
        const char *utf8 = atari2utf8 + index;
        len = utf8Len(*utf8);
        assert(len < 5);
        memcpy(dst, utf8, len);
    }

    return len;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert Atari text to host text and return it
 *
 * @param[in]  atari_text   text in Atari character set
 *
 * @return text in utf-8 coding
 *
 ************************************************************************************************/
const char *CConversion::textAtari2Host(const unsigned char *atari_text)
{
    static char buf[1024];

    char *p = buf;
    char *pe = buf + 1019;  // leave space for four UTF8 bytes and end-of-string!
    unsigned char c;
    while ((c = *atari_text++) && (p < pe))
    {
        unsigned len = charAtari2Host(c, p);
        p += len;
    }
    *p = '\0';
    return buf;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert host error code to Atari error code
 *
 * @param[in]  error   host error code
 *
 * @return Atari error code, ERROR if not convertible or unknown
 *
 ************************************************************************************************/
int CConversion::host2AtariError(int error)
{
    switch(error)
    {
        case EROFS:
        case EPERM:
        case EACCES: return EACCDN;
        case EBADF: return EIHNDL;
        case ENOENT: return EFILNF;    // could also be EPTHNF
        case ENOTDIR: return EPTHNF;
        case EINVAL: return EINVFN;
    }

    return(ERROR);
}


/** **********************************************************************************************
 *
 * @brief [static] Convert host time code to Atari time and date
 *
 * @param[in]  host_time   host time
 * @param[out] time        Atari time (host-endian format), bits hhhhhmmmmmmsssss
 * @param[out] date        Atari date (host-endian format), bits ddddd
 *
 * @note GEMDOS time has a two-seconds resolution.
 * @note tm_sec is 0..60, not 59
 *
 ************************************************************************************************/
void CConversion::hostDateToDosDate(time_t host_time, uint16_t *time, uint16_t *date)
{
    struct tm dt;
    localtime_r(&host_time, &dt);

    if (time != nullptr)
    {
        if (dt.tm_sec == 60)
        {
            dt.tm_sec--;
        }
        *time = (uint16_t) ((dt.tm_sec >> 1) | (dt.tm_min << 5) | (dt.tm_hour << 11));
    }
    if (date != nullptr)
    {
        // day is 1..31
        // month is 1..12 for GEMDOS, 0..11 for Linux
        // year is starting at 1980 for GEMDOS, at 1900 for Linux
        *date = (uint16_t) ((dt.tm_mday) | ((dt.tm_mon + 1)  << 5) | ((dt.tm_year - 80) << 9));
    }
}


/** **********************************************************************************************
 *
 * @brief [static] Convert Atari time and date to host time
 *
 * @param[in]  time        Atari time (host-endian format)
 * @param[in]  date        Atari date (host-endian format)
 * @param[out] host_time   host time
 *
 ************************************************************************************************/
void CConversion::dosDateToHostDate(uint16_t time, uint16_t date, time_t *host_time)
{
    struct tm dt;
    dt.tm_sec   = ((time & 0x1f) << 1);
    dt.tm_min   = ((time >> 5 ) & 0x3f);
    dt.tm_hour  = ((time >> 11) & 0x1f);
    dt.tm_mday  = (date & 0x1f);
    dt.tm_mon   = ((date >> 5 ) & 0x0f) - 1;
    dt.tm_year  = (((date >> 9 ) & 0x7f) + 80);
    dt.tm_wday  = 0;    // ignored by mktime()
    dt.tm_yday  = 0;    // ignored by mktime()
    dt.tm_isdst = -1;   // auto

    *host_time = mktime(&dt);
}
