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
#include "register_model.h"

#define MAX_REG_MODELS  32

static CRegisterModel *models[MAX_REG_MODELS];
static unsigned num_models = 0;


class CTosRom512k : public CRegisterModel
{
  public:
	CTosRom512k() : CRegisterModel("512k TOS ROMs",0x00E00000,0x00F00000) { }

    /*
    virtual bool write(uint32_t addr, unsigned len, const uint8_t *data)
    {
        (void) addr;
        (void) len;
        (void) data;
        return false;
    }
    virtual bool read(uint32_t addr, unsigned len, uint8_t *data)
    {
        (void) addr;
        (void) len;
        (void) data;
        return false;
    }*/
};

class CReservedIO_1 : public CRegisterModel
{
  public:
    CReservedIO_1() : CRegisterModel("Reserved I/O Space #1", 0x00f00000, 0x00fa0000) { }
};

class CRomCartridge : public CRegisterModel
{
  public:
    CRomCartridge() : CRegisterModel("128k ROM cartridge expansion port", 0x00fa0000, 0x00fc0000) { }
};

class CTosRom192k : public CRegisterModel
{
  public:
    CTosRom192k() : CRegisterModel("192k System ROM", 0x00fc0000, 0x00ff0000) { }
};

class CReservedIO_2 : public CRegisterModel
{
  public:
    CReservedIO_2() : CRegisterModel("Reserved I/O Space #2", 0x00ff0000, 0x00ff8000) { }
};

class CST_TT_IO : public CRegisterModel
{
  public:
    CST_TT_IO() : CRegisterModel("ST/TT I/O", 0x00ff8000, 0x01000000) { }
};

class CTTFastRam : public CRegisterModel
{
  public:
    CTTFastRam() : CRegisterModel("TT Fast RAM", 0x01000000, 0x01400000) { }
};

class CReserved : public CRegisterModel
{
  public:
    CReserved() : CRegisterModel("Reserved", 0x01400000, 0xfe000000) { }
};

class CVme : public CRegisterModel
{
  public:
    CVme() : CRegisterModel("VME", 0xfe000000, 0xff000000) { }
};

class CStShadow : public CRegisterModel
{
  public:
    CStShadow() : CRegisterModel("ST 24 bit compatible shadow", 0xff000000, 0xffffffff) { }
    // TODO: one-off
    virtual bool read(uint32_t addr, unsigned len, uint8_t *data)
    {
        (void) addr;
        (void) len;
        (void) data;
        return false;   // bus error
    }
    virtual bool write(uint32_t addr, unsigned len, const uint8_t *data)
    {
        (void) addr;
        (void) len;
        (void) data;
        return false;   // bus error
    }
};


class CTtMmuConf : public CRegisterModel
{
  public:
    CTtMmuConf() : CRegisterModel("TT MMU memory configuration", 0xffff8000, 0xffff8001) { }
};

class CStMmuConf : public CRegisterModel
{
  public:
    CStMmuConf() : CRegisterModel("ST MMU memory configuration", 0xffff8001, 0xffff8002) { }
};

class CVideoScreenMemoryPositionHigh : public CRegisterModel
{
  public:
    CVideoScreenMemoryPositionHigh() : CRegisterModel("Video screen memory position (High byte)", 0xffff8201, 0xffff8202) { }
    virtual bool read(uint32_t addr, unsigned len, uint8_t *data)
    {
        (void) addr;
        (void) len;
        *data = (uint8_t) (addr68kVideo >> 16);
        return true;
    }
};

class CVideoScreenMemoryPositionMid : public CRegisterModel
{
  public:
    CVideoScreenMemoryPositionMid() : CRegisterModel("Video screen memory position (Mid byte)", 0xffff8203, 0xffff8204) { }
    virtual bool read(uint32_t addr, unsigned len, uint8_t *data)
    {
        (void) addr;
        (void) len;
        *data = (uint8_t) (addr68kVideo >> 8);
        return true;
    }
};

class CVideoAddressPointer : public CRegisterModel
{
  public:
    CVideoAddressPointer() : CRegisterModel("Video address pointer", 0xffff8205, 0xffff820a) { }
    virtual bool read(uint32_t addr, unsigned len, uint8_t *data)
    {
        (void) len;
        addr -= start_addr;
        if (addr == 0)
            *data = (uint8_t) (addr68kVideo >> 16);
        else
        if (addr == 2)
            *data = (uint8_t) (addr68kVideo >> 8);
        else
        if (addr == 0)
            *data = (uint8_t) (addr68kVideo);
        return true;
    }
};

class CVideoSyncMode : public CRegisterModel
{
  public:
    CVideoSyncMode() : CRegisterModel("Video Synchronisation mode", 0xffff820a, 0xffff820b) { }
};

class CVideoScreenMemoryPositionLow : public CRegisterModel
{
  public:
    CVideoScreenMemoryPositionLow() : CRegisterModel("Video screen memory position (Low byte)", 0xffff820d, 0xffff820e) { }
    virtual bool read(uint32_t addr, unsigned len, uint8_t *data)
    {
        (void) addr;
        (void) len;
        *data = (uint8_t) addr68kVideo;
        return true;
    }
};

class CYM2149Read : public CRegisterModel
{
  public:
    CYM2149Read() : CRegisterModel("YM2149 Read data/Register select", 0xffff8800, 0xffff8801) { }
};

class CYM2149Write : public CRegisterModel
{
  public:
    CYM2149Write() : CRegisterModel("YM2149 Write data", 0xffff8802, 0xffff8803) { }
};

class CMfp : public CRegisterModel
{
  public:
    CMfp() : CRegisterModel("MFP 68901 - Multi Function Peripheral Chip", 0xfffffa01, 0xfffffa30) { }
};

class CFpu : public CRegisterModel
{
  public:
    CFpu() : CRegisterModel("Floating Point Coprocessor", 0xfffffa, 0xfffffa60) { }
};

class CMfp2 : public CRegisterModel
{
  public:
    CMfp2() : CRegisterModel("MFP 68901 #2 - Multi Function Peripheral Chip #2 (TT)", 0xfffffa81, 0xfffffab0) { }
};

class CAcia : public CRegisterModel
{
  public:
    CAcia() : CRegisterModel("6850 ACIA I/O Chips", 0xfffffc00, 0xfffffc07) { }
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
    models[num_models++] = new CMfp;
    models[num_models++] = new CFpu;
    models[num_models++] = new CMfp2;
    models[num_models++] = new CAcia;
    // ranges
    models[num_models++] = new CTosRom512k;
    models[num_models++] = new CReservedIO_1;
    models[num_models++] = new CRomCartridge;
    models[num_models++] = new CTosRom192k;
    models[num_models++] = new CReservedIO_2;
    models[num_models++] = new CST_TT_IO;
    models[num_models++] = new CTTFastRam;
    models[num_models++] = new CReserved;
    models[num_models++] = new CVme;
    models[num_models++] = new CStShadow;

    return 0;
}


// return false for bus error
bool CRegisterModel::read_byte(uint32_t addr, uint8_t *datum)
{
    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr < model->end_addr))
        {
            bool ret = model->read(addr, 1, datum);
            if (ret)
            {
                DebugInfo("m68k (0x%08x) -> 0x%02x (%s)", addr, *datum, model->name);
            } else
            {
                DebugInfo("m68k (0x%08x) -> BUSERR (%s)", addr, model->name);
            }
            return ret;
        }
    }

    return false;
}


bool CRegisterModel::read_halfword(uint32_t addr, uint16_t *datum)
{
    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr < model->end_addr))
        {
            DebugInfo("m68k (0x%08x) -> 0x???? (%s)", addr, model->name);
            return model->read(addr, 2, (uint8_t *) datum);
        }
    }

    return false;
}


bool CRegisterModel::read_word(uint32_t addr, uint32_t *datum)
{
    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr < model->end_addr))
        {
            DebugInfo("m68k (0x%08x) -> 0x???????? (%s)", addr, model->name);
            return model->read(addr, 4, (uint8_t *) datum);
        }
    }

    return false;
}


// return false for bus error
bool CRegisterModel::write_byte(uint32_t addr, uint8_t datum)
{
    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr < model->end_addr))
        {
            DebugInfo("m68k (0x%08x) := 0x%02x (%s)", addr, datum, model->name);
            return model->write(addr, 1, &datum);
        }
    }

    return false;
}


bool CRegisterModel::write_halfword(uint32_t addr, uint16_t datum)
{
    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr < model->end_addr))
        {
            DebugInfo("m68k (0x%08x) := 0x%04x (%s)", addr, datum, model->name);
            return model->write(addr, 2, (uint8_t *) &datum);
        }
    }

    return false;
}


bool CRegisterModel::write_word(uint32_t addr, uint32_t datum)
{
    for (unsigned n = 0; n < num_models; n++)
    {
        CRegisterModel *model = models[n];
        if ((model != nullptr) && (addr >= model->start_addr) && (addr < model->end_addr))
        {
            DebugInfo("m68k (0x%08x) := 0x%08x (%s)", addr, datum, model->name);
            return model->write(addr, 4, (uint8_t *) &datum);
        }
    }

    return false;
}
