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

#include "config.h"
#include "Debug.h"
#include "emulation_globals.h"
#include "MagiCScreen.h"
#include "register_model.h"
#include <assert.h>

#define MAX_REG_MODELS  32

static CRegisterModel *models[MAX_REG_MODELS];
static unsigned num_models = 0;


class CTosRom512k : public CRegisterModel
{
  public:
	CTosRom512k() : CRegisterModel("512k TOS ROMs",0x00E00000,0x00F00000) { }

    /*
    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        (void) addr;
        (void) len;
        (void) data;
        *p_success = false;
        return;
    }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        (void) addr;
        (void) len;
        (void) data;
        *p_success = false;
        return 0;
    }*/
};

class CReservedIO_1 : public CRegisterModel
{
  public:
    CReservedIO_1() : CRegisterModel("Reserved I/O Space #1", 0x00f00000, 0x00fa0000 - 1) { }
};

class CRomCartridge : public CRegisterModel
{
  public:
    CRomCartridge() : CRegisterModel("128k ROM cartridge expansion port", 0x00fa0000, 0x00fc0000 - 1) { }
};

class CTosRom192k : public CRegisterModel
{
  public:
    CTosRom192k() : CRegisterModel("192k System ROM", 0x00fc0000, 0x00ff0000 - 1) { }
};

class CReservedIO_2 : public CRegisterModel
{
  public:
    CReservedIO_2() : CRegisterModel("Reserved I/O Space #2", 0x00ff0000, 0x00ff8000 - 1) { }
};

class CST_TT_IO : public CRegisterModel
{
  public:
    CST_TT_IO() : CRegisterModel("ST/TT I/O", 0x00ff8000, 0x01000000 - 1) { }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        // kind of recursion
        return read_reg(addr | 0xff000000, len, p_success);
    }
    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        // kind of recursion
        write_reg(addr | 0xff000000, len, datum, p_success);
    }
};

class CTTFastRam : public CRegisterModel
{
  public:
    CTTFastRam() : CRegisterModel("TT Fast RAM", 0x01000000, 0x01400000 - 1) { }
};

class CReserved : public CRegisterModel
{
  public:
    CReserved() : CRegisterModel("Reserved", 0x01400000, 0xfe000000 - 1) { }
};

class CVme : public CRegisterModel
{
  public:
    CVme() : CRegisterModel("VME", 0xfe000000, 0xff000000 - 1) { }
};

class CStShadow : public CRegisterModel
{
  public:
    CStShadow() : CRegisterModel("ST 24 bit compatible shadow", 0xff000000, 0xffffffff) { }
};


class CTtMmuConf : public CRegisterModel
{
  public:
    CTtMmuConf() : CRegisterModel("TT MMU memory configuration", 0xffff8000, 0xffff8000) { }
};

class CStMmuConf : public CRegisterModel
{
  public:
    CStMmuConf() : CRegisterModel("ST MMU memory configuration", 0xffff8001, 0xffff8001) { }
};

class CVideoScreenMemoryPositionHigh : public CRegisterModel
{
  public:
    CVideoScreenMemoryPositionHigh() : CRegisterModel("Video screen memory position (High byte)", 0xffff8201, 0xffff8201)
    {
        logcnt = 20;
    }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        (void) addr;
        (void) len;
        *p_success = true;
        return (uint8_t) (addr68kVideo >> 16);
    }
};

class CVideoScreenMemoryPositionMid : public CRegisterModel
{
  public:
    CVideoScreenMemoryPositionMid() : CRegisterModel("Video screen memory position (Mid byte)", 0xffff8203, 0xffff8203)
    {
        logcnt = 20;
    }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        (void) addr;
        (void) len;
        *p_success = true;
        return (uint8_t) (addr68kVideo >> 8);
    }
};

class CVideoAddressPointer : public CRegisterModel
{
  public:
    CVideoAddressPointer() : CRegisterModel("Video address pointer", 0xffff8204, 0xffff8209)
    {
        logcnt = 20;
    }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        if (((addr & 1) == 0) || (len != 1))
        {
            // only support 8-bit read on odd address
            *p_success = false;
            return 0;
        }

        addr -= start_addr;
        *p_success = true;
        if (addr == 1)
            return (uint8_t) (addr68kVideo >> 16);
        else
        if (addr == 3)
            return (uint8_t) (addr68kVideo >> 8);
        else
        if (addr == 5)
            return (uint8_t) (addr68kVideo);
        assert(0);
        return 0;
    }
};

class CVideoSyncMode : public CRegisterModel
{
  public:
    CVideoSyncMode() : CRegisterModel("Video Synchronisation mode", 0xffff820a, 0xffff820a) { }
};

class CVideoScreenMemoryPositionLow : public CRegisterModel
{
  public:
    CVideoScreenMemoryPositionLow() : CRegisterModel("Video screen memory position (Low byte)", 0xffff820d, 0xffff820d) { }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        (void) addr;
        (void) len;
        *p_success = true;
        return (uint8_t) addr68kVideo;
    }
};

class CVideoPalette : public CRegisterModel
{
  public:
    CVideoPalette() : CRegisterModel("Video colour palette (ST(e))", 0xffff8240, 0xffff825f)
    {
        logcnt = 20;
    }
    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        if ((addr & 1) || (len == 1))
        {
            // only support 16-bit and 32-bit read on even address
            *p_success = false;
            return 0;
        }
        addr -= start_addr;
        unsigned index = addr >> 1;
        assert(index < 16);

        if (len == 2)
        {
            // read one entry

            *p_success = true;
            return CMagiCScreen::getColourPaletteEntry(index);
        }
        else
        {
            // read two entries

            *p_success = true;
            uint32_t val0 = CMagiCScreen::getColourPaletteEntry(index);
            uint32_t val1 = CMagiCScreen::getColourPaletteEntry(index + 1);
            return (val0 << 16) | val1;
        }
    }
    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        if ((addr & 1) || (len == 1))
        {
            // only support 16-bit and 32-bit write on even address
            *p_success = false;
            return;
        }
        addr -= start_addr;
        unsigned index = addr >> 1;
        assert(index < 16);

        if (len == 2)
        {
            // write one entry

            *p_success = true;
            CMagiCScreen::setColourPaletteEntry(index, datum);
        }
        else
        {
            // write two entries

            *p_success = true;
            CMagiCScreen::setColourPaletteEntry(index, (uint16_t) (datum >> 16));
            CMagiCScreen::setColourPaletteEntry(index + 1, (uint16_t) datum);
        }
    }
};

class CYM2149Read : public CRegisterModel
{
  public:
    CYM2149Read() : CRegisterModel("YM2149 Read data/Register select", 0xffff8800, 0xffff8800) { }
};

class CYM2149Write : public CRegisterModel
{
  public:
    CYM2149Write() : CRegisterModel("YM2149 Write data", 0xffff8802, 0xffff8802) { }
};

class CMfp : public CRegisterModel
{
  public:
    CMfp() : CRegisterModel("MFP 68901 - Multi Function Peripheral Chip", 0xfffffa01, 0xfffffa2f) { }
};

class CFpu : public CRegisterModel
{
  public:
    CFpu() : CRegisterModel("Floating Point Coprocessor", 0xfffffa40, 0xfffffa5f) { }
};

class CMfp2 : public CRegisterModel
{
  public:
    CMfp2() : CRegisterModel("MFP 68901 #2 - Multi Function Peripheral Chip #2 (TT)", 0xfffffa81, 0xfffffaaf) { }
};

class CAcia : public CRegisterModel
{
  public:
    CAcia() : CRegisterModel("6850 ACIA I/O Chips", 0xfffffc00, 0xfffffc06) { }
};

int CRegisterModel::init()
{
    // modules
    models[num_models++] = new CTtMmuConf;
    models[num_models++] = new CStMmuConf;
    models[num_models++] = new CYM2149Read;
    models[num_models++] = new CYM2149Write;
    models[num_models++] = new CVideoScreenMemoryPositionHigh;
    models[num_models++] = new CVideoScreenMemoryPositionMid;
    models[num_models++] = new CVideoSyncMode;
    models[num_models++] = new CVideoAddressPointer;
    models[num_models++] = new CVideoScreenMemoryPositionLow;
    models[num_models++] = new CVideoPalette;
    models[num_models++] = new CMfp;
    models[num_models++] = new CFpu;
    models[num_models++] = new CMfp2;
    models[num_models++] = new CAcia;
    // ranges in 24-bit address range may interfere with large emulated RAM and VRAM
    if (addr68kVideoEnd < 0x01000000)
    {
        models[num_models++] = new CTosRom512k;
        models[num_models++] = new CReservedIO_1;
        models[num_models++] = new CRomCartridge;
        models[num_models++] = new CTosRom192k;
        models[num_models++] = new CReservedIO_2;
        models[num_models++] = new CST_TT_IO;
    }
    else
    {
        DebugWarning2("() -- 68k RAM + VRAM overlaps 24-bit ST I/O address range");
    }
    // ranges for 32-bit addresses
    models[num_models++] = new CTTFastRam;
    models[num_models++] = new CReserved;
    models[num_models++] = new CVme;
    models[num_models++] = new CStShadow;

    return 0;
}


/** **********************************************************************************************
 *
 * @brief Read 8-bit or 16-bit or 32-bit value from hardware register address range
 *
 * @param[in]  addr         absolute 68k address, 32-bit or 24-bit
 * @param[in]  len          1, 2 or 4 for 8-bit, 16-bit or 32-bit
 * @param[out] p_success    true: access granted, false: illegal access
 *
 * @return read value, extended to 32-bit
 *
 ************************************************************************************************/
uint32_t CRegisterModel::read_reg(uint32_t addr, unsigned len, bool *p_success)
{
    uint32_t ret = 0;
    *p_success = false;

    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr <= model->last_addr))
        {
            ret = model->read(addr, len, p_success);
            if (*p_success)
            {
                if (model->logcnt)
                {
                    if (len == 1)
                    {
                        DebugInfo("m68k (0x%08x) -> 0x%02x (%s)", addr, ret, model->name);
                    }
                    else
                    if (len == 2)
                    {
                        DebugInfo("m68k (0x%08x) -> 0x%04x (%s)", addr, ret, model->name);
                    }
                    else
                    {
                        DebugInfo("m68k (0x%08x) -> 0x%08x (%s)", addr, ret, model->name);
                    }

                    model->logcnt--;
                    if (model->logcnt == 0)
                    {
                        DebugWarning("     suppress further messages for this model");
                    }
                }
            }
            else
            {
                DebugInfo("m68k (0x%08x) -> BUSERR (%s)", addr, model->name);
            }
            break;
        }
    }

    return ret;
}


/** **********************************************************************************************
 *
 * @brief Read 8-bit or 16-bit or 32-bit value from hardware register address range
 *
 * @param[in]  addr         absolute 68k address, 32-bit or 24-bit
 * @param[in]  len          1, 2 or 4 for 8-bit, 16-bit or 32-bit
 * @param[in]  datum        datum to write
 * @param[out] p_success    true: access granted, false: illegal access
 *
 * @return read value, extended to 32-bit
 *
 ************************************************************************************************/
void CRegisterModel::write_reg(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
{
    *p_success = false;

    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr <= model->last_addr))
        {
            model->write(addr, len, datum, p_success);
            if (*p_success)
            {
                if (model->logcnt)
                {
                    if (len == 1)
                    {
                        DebugInfo("m68k (0x%08x) := 0x%02x (%s)", addr, datum, model->name);
                    }
                    else
                    if (len == 2)
                    {
                        DebugInfo("m68k (0x%08x) := 0x%04x (%s)", addr, datum, model->name);
                    }
                    else
                    {
                        DebugInfo("m68k (0x%08x) := 0x%08x (%s)", addr, datum, model->name);
                    }
                    model->logcnt--;
                    if (model->logcnt == 0)
                    {
                        DebugWarning("     suppress further messages for this model");
                    }
                }
            }
            else
            {
                if (len == 1)
                {
                    DebugInfo("m68k (0x%08x) := 0x%02x -> BUSERR (%s)", addr, datum, model->name);
                }
                else
                if (len == 2)
                {
                    DebugInfo("m68k (0x%08x) := 0x%04x -> BUSERR (%s)", addr, datum, model->name);
                }
                else
                {
                    DebugInfo("m68k (0x%08x) := 0x%08x -> BUSERR (%s)", addr, datum, model->name);
                }
            }
            break;
        }
    }
}

