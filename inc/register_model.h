/*
 * Copyright (C) 2025 Andreas Kromke, andreas.kromke@gmail.com
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
* Manages register access (I/O)
*
*/

#ifndef _REGISTER_MODEL_H
#define _REGISTER_MODEL_H

#include <string.h>
#include "Atari.h"


class CRegisterModel
{
  public:
    static int init();
    static uint32_t read_reg(uint32_t addr, unsigned len, bool *p_success);
    static void write_reg(uint32_t addr, unsigned len, uint32_t datum, bool *p_success);

    const char *name = "base";
    const uint32_t start_addr = 0;
    const uint32_t last_addr = 0;
    unsigned logcnt = 100;     // maximum debug messages for this model

	CRegisterModel(const char *my_name, uint32_t my_start_addr, uint32_t my_last_addr) :
        name(my_name),
        start_addr(my_start_addr),
        last_addr(my_last_addr)
    {
    }

    /*
    * default method implementations
    */

	virtual ~CRegisterModel()
    {
    }

    virtual const char *regname(uint32_t addr, unsigned len)
    {
        (void) addr;
        (void) len;
        return "";
    }

    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        // default: ignore write, no bus error
        (void) addr;
        (void) len;
        (void) datum;
        *p_success = true;
    }

    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        // default: read zeros, no bus error
        (void) addr;
        (void) len;
        *p_success = true;
        return 0;
    }
};

#endif
