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


/** **********************************************************************************************
 *
 * @brief SHIFTER Video Controller: Video addresses (ST, STe, F030)
 *
 ************************************************************************************************/
class CVideoAddress : public CRegisterModel
{
  public:
    CVideoAddress() : CRegisterModel("Video screen address", 0xffff8200, 0xffff8211)
    {
        logcnt = 20;
    }

    virtual const char *regname(uint32_t addr, unsigned len)
    {
        addr -= start_addr;
        if ((addr == 0) && (len == 4))
        {
            return "pos high and mid";
        }
        switch(addr)
        {
            case    1: return "pos high";
            case    3: return "pos mid";
            case    5: return "ptr high";
            case    7: return "ptr mid";
            case    9: return "ptr low";
            case 0x0a: return "sync mode";
            case 0x0d: return "pos low (STe)";
        }
        return "";
    }

    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        addr -= start_addr;
        *p_success = true;

        uint32_t physaddr = addr68kVideo;   // default screen
        if (CMagiCScreen::m_physAddr != 0)
        {
            physaddr = CMagiCScreen::m_physAddr;    // changed address
        }

        if ((addr == 0) && (len == 4))
        {
            uint32_t high = (physaddr >> 16) & 0xff;
            uint32_t mid  = (physaddr >>  8) & 0xff;
            return (high << 16) | mid;   // high and mid bytes
        }

        if (len == 1)
        {
            // only support 8-bit reads
            switch(addr)
            {
                case    1: return (physaddr >> 16) & 0xff;  // high byte
                case    3: return (physaddr >> 8) & 0xff;   // mid byte
                case    5: return (physaddr >> 16) & 0xff;  // high byte
                case    7: return (physaddr >> 8) & 0xff;   // mid byte
                case    9: return (physaddr) & 0xff;        // low byte
                case 0x0a: return 0;                        // sync mode
                case 0x0d: return (physaddr) & 0xff;        // low byte
            }
        }
        *p_success = false;
        return 0;
    }

    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        addr -= start_addr;

        uint32_t physaddr = addr68kVideo;   // default screen
        if (CMagiCScreen::m_physAddr != 0)
        {
            physaddr = CMagiCScreen::m_physAddr;    // changed address
        }
        uint32_t prev_physaddr = physaddr;

        if ((addr == 0) && (len == 4))
        {
            physaddr &= 0xff0000ff;             // mask out high and mid byte
            uint32_t high = (datum >> 16) & 0xff;
            uint32_t mid  = datum  & 0xff;
            physaddr |= high << 16;     // replace high byte
            physaddr |= mid << 8;       // replace mid byte
        }
        else
        if ((addr == 1) && (len == 1))
        {
            physaddr &= 0xff00ffff;             // mask out high byte
            physaddr |= (datum & 0xff) << 16;   // replace high byte
        }
        else
        if ((addr == 3) && (len == 1))
        {
            physaddr &= 0xffff00ff;             // mask out mid byte
            physaddr |= (datum & 0xff) << 8;    // replace mid byte
        }
        if ((addr == 0x0d) && (len == 1))
        {
            physaddr &= 0xffffff00;             // mask out low byte
            physaddr |= (datum & 0xff);         // replace low byte
        }

        if (physaddr != prev_physaddr)
        {
            if (physaddr == addr68kVideo)
            {
                DebugWarning("    physical screen address reset to default");
                CMagiCScreen::m_physAddr = 0;   // go back to default screen
            }
            else
            {
                DebugWarning("    physical screen address set to 0x%08x", physaddr);
                CMagiCScreen::m_physAddr = physaddr;
            }
        }
        *p_success = true;  // never bus error, just ignore, if unhandled
    }
};


/** **********************************************************************************************
 *
 * @brief SHIFTER Video Controller: Video colour palette (ST(e))
 *
 ************************************************************************************************/
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


/** **********************************************************************************************
 *
 * @brief SHIFTER Video Controller: resolution (ST, STe, F030)
 *
 ************************************************************************************************/
class CVideoResolution : public CRegisterModel
{
  public:
    CVideoResolution() : CRegisterModel("Video resolution", 0xffff8260, 0xffff8263) { }

    virtual const char *regname(uint32_t addr, unsigned len)
    {
        addr -= start_addr;
        (void) len;
        switch(addr)
        {
            case    0: return "ST shift mode";
            case    1: return "ST shift mode, only shifter";
            case    2: return "TT shift mode";
            case    3: return "ST palette bank (TT)";
        }
        return "";
    }

    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        addr -= start_addr;

        if ((len != 1) || (addr > 2))
        {
            // only support 8-bit reads to the first three bytes
            *p_success = false;
            return 0;
        }

        uint8_t mode = CMagiCScreen::getAtariScreenMode();
        if ((addr == 0) || (addr == 1))
        {
            // ST register: return invalid value 3 for ST incompatible resolution
            if (mode > 2)
            {
                mode = 3;
            }
        }

        *p_success = true;
        return mode;
    }

    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        addr -= start_addr;

        if ((len != 1) || (addr > 2))
        {
            // only support 8-bit writes to the first three bytes
            *p_success = false;
            return;
        }

        uint8_t mode = CMagiCScreen::getAtariScreenMode();
        if ((addr == 0) || (addr == 1))
        {
            // ST register: return invalid value 3 for ST incompatible resolution
            if (mode != datum)
            {
                // we could show an alert here
                DebugError("Failed attempt to change ST resolution from %u to %u", mode, datum);
            }
        }

        *p_success = true;
    }
};


/** **********************************************************************************************
 *
 * @brief YM2149 - Sound Chip
 *
 ************************************************************************************************/
class CYM2149 : public CRegisterModel
{
  public:
    CYM2149() : CRegisterModel("YM2149", 0xffff8800, 0xffff8802) { }

    virtual const char *regname(uint32_t addr, unsigned len)
    {
        (void) len;
        addr -= start_addr;
        if (addr == 0)
            return "Read data/Register select";
        else
        if (addr == 2)
            return "Write data";
        else
            return "";
    }
};


/** **********************************************************************************************
 *
 * @brief MFP 68901 - Multi Function Peripheral Chip (ST and TT)
 *
 ************************************************************************************************/
class CMfp : public CRegisterModel
{
  public:
    CMfp() : CRegisterModel("MFP 68901 - Multi Function Peripheral Chip", 0xfffffa00, 0xfffffa2f) { }

    virtual const char *regname(uint32_t addr, unsigned len)
    {
        addr -= start_addr;
        if (len == 1)
        {
            switch(addr)
            {
                case 0x07: return "Int Enable A";
                case 0x09: return "Int Enable B";
                case 0x0b: return "Int Pending A";
                case 0x0d: return "Int Pending B";
                case 0x0f: return "Int In-Service A";
                case 0x11: return "Int In-Service B";
                case 0x13: return "Int Mask A";
                case 0x15: return "Int Mask B";
                case 0x17: return "Vector Register";
                case 0x19: return "Timer A control";
                case 0x1b: return "Timer B control";
                case 0x1d: return "Timer C/D control";
                case 0x1f: return "Timer A data";
                case 0x21: return "Timer B data";
                case 0x23: return "Timer C data";
                case 0x25: return "Timer D data";
            }
        }
        return "";
    }
};

/** **********************************************************************************************
 *
 * @brief Floating Point Coprocessor (Mega-STe)
 *
 ************************************************************************************************/
class CFpu : public CRegisterModel
{
  public:
    CFpu() : CRegisterModel("Floating Point Coprocessor", 0xfffffa40, 0xfffffa5f) { }

    virtual const char *regname(uint32_t addr, unsigned len)
    {
        addr -= start_addr;
        if (len == 2)
        {
            switch(addr)
            {
                case 0x00: return "FP_Stat";
                case 0x02: return "FP_Ctl";
                case 0x04: return "FP_Save";
                case 0x06: return "FP_Restore";
            }
        }
        return "";
    }

    virtual void write(uint32_t addr, unsigned len, uint32_t datum, bool *p_success)
    {
        // bus error
        (void) addr;
        (void) len;
        (void) datum;
        *p_success = false;
    }

    virtual uint32_t read(uint32_t addr, unsigned len, bool *p_success)
    {
        // bus error
        (void) addr;
        (void) len;
        *p_success = false;
        return 0;
    }
};


/** **********************************************************************************************
 *
 * @brief MFP 68901 #2 - Multi Function Peripheral Chip (TT)
 *
 ************************************************************************************************/
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
    models[num_models++] = new CYM2149;
    models[num_models++] = new CVideoAddress;
    models[num_models++] = new CVideoPalette;
    models[num_models++] = new CVideoResolution;
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
                        DebugInfo("m68k (0x%08x) -> 0x%02x (%s %s)", addr, ret, model->name, model->regname(addr, len));
                    }
                    else
                    if (len == 2)
                    {
                        DebugInfo("m68k (0x%08x) -> 0x%04x (%s %s)", addr, ret, model->name, model->regname(addr, len));
                    }
                    else
                    {
                        DebugInfo("m68k (0x%08x) -> 0x%08x (%s %s)", addr, ret, model->name, model->regname(addr, len));
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
                        //SDL_SetRelativeMouseMode(SDL_FALSE);
                        DebugInfo("m68k (0x%08x) := 0x%02x (%s %s)", addr, datum, model->name, model->regname(addr, len));
                    }
                    else
                    if (len == 2)
                    {
                        DebugInfo("m68k (0x%08x) := 0x%04x (%s %s)", addr, datum, model->name, model->regname(addr, len));
                    }
                    else
                    {
                        DebugInfo("m68k (0x%08x) := 0x%08x (%s %s)", addr, datum, model->name, model->regname(addr, len));
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

