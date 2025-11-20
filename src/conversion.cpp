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
* Character conversions between Atari and host
*
*/

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
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
 * @note Utf-8 characters with no Atari pendant are stored as '_'.
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
 * @brief [static] Get number of characters in an utf-8 string
 *
 * @param[in]  utf8         utf-8 string
 * @param[in]  crlf_conv    count CR and LF as two (!) characters, for conversion to CR/LF
 *
 * @return number of characters
 *
 ************************************************************************************************/
unsigned CConversion::hostStringLength(const char *utf8, bool crlf_conv)
{
    if (utf8 == nullptr)
    {
        return 0;
    }

    unsigned slen = 0;
    const char *s = utf8;
    while(*s)
    {
        if (crlf_conv)
        {
            if (s[0] == '\n')
            {
                s++;
                slen += 2;      // LF -> CR/LF
                continue;
            }

            if ((s[0] == '\r') && (s[1] != '\n'))
            {
                s++;
                slen += 2;      // CR -> CR/LF
                continue;
            }

            if ((s[0] == '\r') && (s[1] == '\n'))
            {
                s += 2;
                slen += 2;      // CR/LF -> CR/LF
                continue;
            }
        }

        slen++;
        unsigned clen = utf8Len(*s);
        s += clen;
    }

    return slen;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert utf-8 string to Atari string
 *
 * @param[in]  utf8         utf-8 string
 * @param[out] buf          destination buffer
 * @param[in]  buflen       destination buffer length, including end-of-string
 * @param[in]  crlf_conv    convert CR and LF to CR/LF
 *
 * @return number of written bytes
 *
 ************************************************************************************************/
unsigned CConversion::strHost2Atari(const char *utf8, uint8_t *buf, unsigned buflen, bool crlf_conv)
{
    if (utf8 == nullptr)
    {
        return 0;
    }

    unsigned slen = 0;
    const char *s = utf8;
    while((*s) && (slen < buflen))
    {
        if (crlf_conv)
        {
            if (s[0] == '\n')
            {
                s++;
                if (slen < buflen - 1)
                {
                    *buf++ = '\r';      // LF -> CR/LF
                    *buf++ = '\n';
                }
                else
                {
                    // overflow, suppress
                }
                slen += 2;
                continue;
            }

            if ((s[0] == '\r') && (s[1] != '\n'))
            {
                s++;
                if (slen < buflen - 1)
                {
                    *buf++ = '\r';      // CR -> CR/LF
                    *buf++ = '\n';
                }
                else
                {
                    // overflow, suppress
                }
                slen += 2;
                continue;
            }

            if ((s[0] == '\r') && (s[1] == '\n'))
            {
                s += 2;
                if (slen < buflen - 1)
                {
                    *buf++ = '\r';      // CR/LF -> CR/LF
                    *buf++ = '\n';
                }
                else
                {
                    // overflow, suppress
                }
                slen += 2;
                continue;
            }
        }

        unsigned clen = charHost2Atari(s, buf);
        s += clen;
        if (clen > 0)
        {
            buf++;
            slen++;
        }
    }

    *buf = '\0';

    return slen;
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
 * @brief [static] Get number of necessary utf8-bytes in an Atari string
 *
 * @param[in]  src          Atari string
 * @param[in]  crlf_conv    count CR and LF as one (!) character, for conversion to LF
 *
 * @return number of needed bytes
 *
 ************************************************************************************************/
unsigned CConversion::atariStringHostLength(const unsigned char *src, bool crlf_conv)
{
    if (src == nullptr)
    {
        return 0;
    }

    char dummy_buf[16];
    unsigned dlen = 0;
    const unsigned char *s = src;
    while(*s)
    {
        if (crlf_conv)
        {
            if (s[0] == '\n')
            {
                s++;
                dlen += 1;      // LF -> LF
                continue;
            }

            if ((s[0] == '\r') && (s[1] != '\n'))
            {
                s++;
                dlen += 1;      // CR -> LF
                continue;
            }

            if ((s[0] == '\r') && (s[1] == '\n'))
            {
                s += 2;
                dlen += 1;      // CR/LF -> LF
                continue;
            }
        }

        unsigned clen = charAtari2Host(*s, dummy_buf);
        dlen += clen;
        s++;
    }

    return dlen;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert Atari string to to utf8 host string
 *
 * @param[in]  src          Atari string
 * @param[in]  crlf_conv    count CR and LF as one (!) character, for conversion to LF
 *
 * @return number of needed bytes
 *
 ************************************************************************************************/

unsigned CConversion::strAtari2Host(const unsigned char *src, char *buf, unsigned buflen, bool crlf_conv)
{
    if (src == nullptr)
    {
        return 0;
    }

    unsigned dlen = 0;
    const unsigned char *s = src;
    while((*s) && (dlen < buflen))
    {
        if (crlf_conv)
        {
            if (s[0] == '\n')
            {
                *buf++ = '\n';
                s++;
                dlen += 1;      // LF -> LF
                continue;
            }

            if ((s[0] == '\r') && (s[1] != '\n'))
            {
                *buf++ = '\n';
                s++;
                dlen += 1;      // CR -> LF
                continue;
            }

            if ((s[0] == '\r') && (s[1] == '\n'))
            {
                *buf++ = '\n';
                s += 2;
                dlen += 1;      // CR/LF -> LF
                continue;
            }
        }

        unsigned clen = charAtari2Host(*s, buf);
        dlen += clen;
        buf += clen;
        s++;
    }

    *buf = '\0';

    return dlen;
}


/** **********************************************************************************************
 *
 * @brief [static] Convert Atari lowercase character to uppercase
 *
 * @param[in]   c     Atari lowercase character
 *
 * @return Atari uppercase character, if convertible, otherwise c
 *
 ************************************************************************************************/
unsigned char CConversion::charAtari2UpperCase(unsigned char c)
{
    /* äöüçé(a°)(ae)(oe)à(ij)(n˜)(a˜)(o/)(o˜) */
    static const char lowers[] = {'\x84','\x94','\x81','\x87','\x82','\x86','\x91','\xb4','\x85','\xc0','\xa4','\xb0','\xb3','\xb1',0};
    static const char uppers[] = {'\x8e','\x99','\x9a','\x80','\x90','\x8f','\x92','\xb5','\xb6','\xc1','\xa5','\xb7','\xb2','\xb8',0};

    if (c >= 'a' && c <= 'z')
    {
        return((char) (c & '\x5f'));
    }

    const char *found;
    if (((unsigned char) c >= 128) && ((found = strchr(lowers, c)) != nullptr))
    {
        return uppers[found - lowers];
    }

    return(c);
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
 * @brief [static] Read file to a buffer
 *
 * @param[in]  filename     file to read
 * @param[in]  add_bufsiz   make buffer larger than file size
 * @param[out] bufsiz       resulting buffer size
 *
 * @return buffer or nullptr
 *
 ************************************************************************************************/
void *CConversion::readFileToBuffer(const char *filename, unsigned add_bufsiz, unsigned *pread_cont)
{
    int fd = open(filename, O_RDONLY, 0);
    if (fd < 0)
    {
        DebugError2("() -- cannot open file \"%s\" -> %s", filename, strerror(errno));
        return nullptr;
    }

    // get file size
    off_t file_length = lseek(fd, 0, SEEK_END);
    (void) lseek(fd, 0, SEEK_SET);
    if (file_length < 1)
    {
        close(fd);      // ignore non-readable and empty files
        return nullptr;
    }

    // optionally allocate more bytes, for end-of-string
    uint8_t *src_buf = (uint8_t *) malloc(file_length + add_bufsiz);
    assert(src_buf != nullptr);

    // read entire file and close it
    ssize_t read_count = read(fd, src_buf, file_length);
    close(fd);
    if (read_count == -1)
    {
        free(src_buf);
        DebugError2("() -- cannot read file \"%s\" -> %s", filename, strerror(errno));
        return nullptr;
    }
    assert(read_count <= file_length);
    *pread_cont = read_count;

    return src_buf;
}


/** **********************************************************************************************
 *
 * @brief [static] Read Atari text file and convert to buffer in host text format
 *
 * @param[in]   filename   Atari text file, e.g. SCRAP.TXT
 * @param[out]  pBuffer    utf-8 buffer
 *
 * @note This converts both characters and line endings (to LF)
 *
 * @note We use a two-pass conversion, first determine space, then convert.
 *
 ************************************************************************************************/
void CConversion::convTextFileAtari2Host(const char *filename, char **pBuffer)
{
    unsigned read_count;
    uint8_t *src_buf = (uint8_t *) readFileToBuffer(filename, 1, &read_count);
    if (src_buf == nullptr)
    {
        *pBuffer = nullptr;
        return;
    }

    src_buf[read_count] = '\0';    // add end-of-string

    unsigned dst_len = CConversion::atariStringHostLength(src_buf, true);
    if (dst_len == 0)
    {
        DebugWarning2("() -- ignore empty file \"%s\"", filename);
        *pBuffer = nullptr;     // ignore empty file
        free(src_buf);
        return;
    }

    char *dst_buf = (char *) malloc(dst_len + 1);
    unsigned done = CConversion::strAtari2Host(src_buf, dst_buf, dst_len + 1, true);
    assert(done == dst_len);

    free(src_buf);
    *pBuffer = dst_buf;
}


/** **********************************************************************************************
 *
 * @brief [static] Read host text file and convert to buffer in Atari text format
 *
 * @param[in]   filename   host text file, utf-8
 * @param[out]  pBuffer    Atari text buffer
 *
 * @note This converts both characters and line endings (to CR/LF)
 *
 * @note We use a two-pass conversion, first determine space, then convert.
 *
 ************************************************************************************************/
void CConversion::convTextFileHost2Atari(const char *filename, uint8_t **pBuffer)
{
    unsigned read_count;
    char *src_buf = (char *) readFileToBuffer(filename, 1, &read_count);
    if (src_buf == nullptr)
    {
        *pBuffer = nullptr;
        return;
    }

    src_buf[read_count] = '\0';    // add end-of-string

    unsigned stringlen = CConversion::hostStringLength(src_buf, true);
    if (stringlen == 0)
    {
        DebugWarning2("() -- ignore empty file \"%s\"", filename);
        *pBuffer = nullptr;     // ignore empty file
        free(src_buf);
        return;        // no data
    }

    uint8_t *dst_buf = (uint8_t *) malloc(stringlen + 1);
    assert(dst_buf != nullptr);
    unsigned done = CConversion::strHost2Atari(src_buf, dst_buf, stringlen + 1, true);
    assert(done == stringlen);

    free(src_buf);
    *pBuffer = dst_buf;
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
        case EEXIST: return EACCDN;
    }

    return ERROR;
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


/** **********************************************************************************************
 *
 * @brief [static] Create an allocated copy of a string
 *
 * @param[in]  s        string
 *
 * @return allocated copy of the string
 *
 ************************************************************************************************/
char *CConversion::copyString(const char *s)
{
    unsigned len = strlen(s);
    char *out = (char *) malloc(len + 1);
    memcpy(out, s, len + 1);
    return out;
}
