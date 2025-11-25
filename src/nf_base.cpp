/*
 * This file was taken from the ARAnyM project, and adapted to MagicOnLinux:
 *
 * nf_base.c - NatFeat base
 *
 * Copyright (c) 2002-2004 Petr Stehlik of ARAnyM dev team (see AUTHORS)
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "Globals.h"
#include "m68kcpu.h"
#include "natfeat.h"
#include "nf_base.h"

void a2fstrcpy(char *dest, uint32_t source)
{
    while ((*dest++ = m68ki_read_8(source)) != 0)
        source++;
}


void f2astrcpy(uint32_t dest, const char *source)
{
    while (*source)
    {
        m68ki_write_8(dest, *source);
        dest++;
        source++;
    }
    m68ki_write_8(dest, 0);
}


void atari2HostSafeStrncpy(char *dest, uint32_t source, size_t count)
{
    while (count > 1 && (*dest = (char) m68ki_read_8(source)) != 0)
    {
        count--;
        dest++;
        source++;
    }
    if (count > 0)
        *dest = '\0';
}


void host2AtariSafeStrncpy(uint32_t dest, const char *source, size_t count)
{
    while (count > 1 && *source)
    {
        m68ki_write_8(dest, *source);
        dest++;
        source++;
        count--;
    }
    if (count > 0)
        m68ki_write_8(dest, 0);
}


size_t atari2HostSafeStrlen(uint32_t source)
{
    size_t count = 1;

    if (source == 0)
        return 0;
    while (m68ki_read_8(source) != 0)
    {
        count++;
        source++;
    }
    return count;
}
