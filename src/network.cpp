/*
 *
 * Does everything that deals with Network emulation (Thorsten Otto)
 *
 */

#include "config.h"
#include <sys/stat.h>
#include <sys/param.h>
#include <time.h>
#include <assert.h>
#include "emulation_globals.h"
#include "Debug.h"
#include "Globals.h"
#include "MagiC.h"
#include "Atari.h"
#include "network.h"
#include "conversion.h"
#include "m68kcpu.h"
#include "natfeat.h"

#define MXETH_API_VERSION 1

enum
{
    MXETH_GET_VERSION = 0,  /* no parameters, return API_VERSION in d0 */
    MXETH_INTLEVEL,         /* no parameters, return Interrupt Level in d0 */
    MXETH_IRQ,              /* acknowledge interrupt from host */
    MXETH_START,            /* (ethX), called on 'ifup', start receiver thread */
    MXETH_STOP,             /* (ethX), called on 'ifdown', stop the thread */
    MXETH_READLENGTH,       /* (ethX), return size of network data block to read */
    MXETH_READBLOCK,        /* (ethX, buffer, size), read block of network data */
    MXETH_WRITEBLOCK,       /* (ethX, buffer, size), write block of network data */
    MXETH_GET_MAC,          /* (ethX, buffer, size), return MAC HW addr in buffer */
    MXETH_GET_IPHOST,       /* (ethX, buffer, size), return IP address of host */
    MXETH_GET_IPATARI,      /* (ethX, buffer, size), return IP address of atari */
    MXETH_GET_NETMASK,      /* (ethX, buffer, size), return IP netmask */
    MXETH_GET_GATEWAY       /* (ethX, buffer, size), return IP gateway */
};


CNetwork::Handler *CNetwork::handlers[MAX_ETH];
int CNetwork::pending_interrupts;

/*
 *  Initialization
 */
void CNetwork::init(void)
{
    pending_interrupts = 0;

    for (int i = 0; i < MAX_ETH; i++)
    {
        Handler *handler = new ETHERNET_HANDLER_CLASSNAME(i);
        DebugInfo2("ETH%d: open: %s", i, handler ? handler->type() : "<none>");
        if (handler->open())
        {
            handlers[i] = handler;
        } else
        {
            DebugWarning2("ETH%d: open failed", i);
            delete handler;
            handlers[i] = NULL;
        }
    }
}


/*
 *  Deinitialization
 */
void CNetwork::exit(void)
{
    DebugInfo2("Ethernet: exit");

    for (int i = 0; i < MAX_ETH; i++)
    {
        // Stop reception thread
        Handler *handler = handlers[i];
        if ( handler )
        {
            stopThread(handler);
            handler->close();
            delete handler;
            handlers[i] = NULL;
        }
    }
}


// reset, called upon OS reboot
void CNetwork::reset(void)
{
    DebugInfo2("Ethernet: reset");

    for (int i = 0; i < MAX_ETH; i++)
    {
        // Stop reception thread
        Handler *handler = handlers[i];
        if (handler)
        {
            stopThread(handler);
        }
    }
}


CNetwork::Handler *CNetwork::getHandler(unsigned int ethX, bool msg)
{
    if (ethX < MAX_ETH)
    {
        Handler *h = handlers[ethX];
        if (h != NULL)
        {
            assert(h->ethX == ethX);
            return h;
        }
    }

    if (msg)
    {
        DebugError2("Ethernet: handler for %d not found", ethX);
    }

    return NULL;
}

uint32_t CNetwork::AtariNetwork(m68k_addr_type params, uint8_t *addrOffset68k)
{
    uint32_t cmd;
    uint32_t ret = 0;

    (void)addrOffset68k;
    cmd = m68ki_read_data_32(params);
    params += 4;

    switch (cmd)
    {
    case MXETH_GET_VERSION:
        return MXETH_API_VERSION;

    case MXETH_INTLEVEL:
        {
            uint32_t unit;

            unit = m68ki_read_data_32(params);
            DebugInfo2("ETH%d: get_int_level()", unit);
            params += 4;
            if (unit >= MAX_ETH)
                return EUNDEV;
            return Preferences::eth[unit].intlevel;
        }

    case MXETH_GET_MAC:    // what is the MAC address?
        /* store MAC address to provided buffer */
        {
            uint32_t unit;

            unit = m68ki_read_data_32(params);
            params += 4;
            if (unit >= MAX_ETH || Preferences::eth[unit].type == ETH_TYPE_NONE)
                return EUNDEV;

            uint32_t buf_ptr = params;    // destination buffer
            DebugInfo2("ETH%d: getMAC(%x)", unit, buf_ptr);

            // default MAC Address is just made up
            unsigned char mac_addr[6] = {'\0','A','E','T','H', (unsigned char)('0' + unit) };

            // convert user-defined MAC Address from string to 6 bytes array
            char *ms = Preferences::eth[unit].mac_addr;
            bool format_OK = false;
            if (strlen(ms) == 2*6+5 && (ms[2] == ':' || ms[2] == '-'))
            {
                ms[2] = ms[5] = ms[8] = ms[11] = ms[14] = ':';
                unsigned int md[6] = {0, 0, 0, 0, 0, 0};
                int matched = sscanf(ms, "%02x:%02x:%02x:%02x:%02x:%02x",
                    &md[0], &md[1], &md[2], &md[3], &md[4], &md[5]);
                if (matched == 6)
                {
                    for (int i = 0; i < 6; i++)
                        mac_addr[i] = md[i];
                    format_OK = true;
                }
            }
            if (!format_OK)
            {
                DebugError2("ETH%d: MAC Address in incorrect format", unit);
                return TOS_EINVAL;
            }
            Host2Atari_memcpy(buf_ptr, mac_addr, 6);
        }
        break;

    case MXETH_IRQ: // interrupt raised by native side thread polling tap0 interface
        {
            int dev_bit;
            uint32_t unit;
            Handler *handler;

            dev_bit = m68ki_read_data_32(params);
            params += 4;
            if (dev_bit == 0)
            {
                // dev_bit = 0 means "tell me what devices want me to serve their interrupts"
                ret = pending_interrupts;
            } else
            {
                // otherwise the set bit means "I'm acknowledging this device's interrupt"
                unit = -1;
                switch (dev_bit)
                {
                    case 0x01: unit = 0; break;
                    case 0x02: unit = 1; break;
                    case 0x04: unit = 2; break;
                    case 0x08: unit = 3; break;
                    case 0x10: unit = 4; break;
                    case 0x20: unit = 5; break;
                    case 0x40: unit = 6; break;
                    case 0x80: unit = 7; break;
                    default: DebugError2("Ethernet: wrong XIF_IRQ(%d)", dev_bit); break;
                }

                handler = getHandler(unit, true);
                if (handler == NULL)
                    return 0;

                DebugInfo2("ETH%d: IRQ acknowledged", unit);
                // Acknowledge interrupt to reception thread
                pthread_cond_signal(&handler->intAck);
                ret = 0;
            }
        }
        break;

    case MXETH_START:
        {
            uint32_t unit;

            unit = m68ki_read_data_32(params);
            params += 4;
            if (startThread(unit) == false)
                ret = EUNDEV;
        }
        break;

    case MXETH_STOP:
        {
            uint32_t unit;

            unit = m68ki_read_data_32(params);
            params += 4;
            stopThread(getHandler(unit, false));
        }
        break;

    case MXETH_READLENGTH:
        {
            uint32_t unit;

            unit = m68ki_read_data_32(params);
            params += 4;
            ret = readPacketLength(unit);
        }
        break;
    case MXETH_READBLOCK:
        {
            uint32_t unit;
            m68k_addr_type buff;
            uint32_t len;

            unit = m68ki_read_data_32(params);
            params += 4;
            buff = m68ki_read_data_32(params);
            params += 4;
            len = m68ki_read_data_32(params);
            params += 4;
            readPacket(unit, buff, len);
        }
        break;
    case MXETH_WRITEBLOCK:
        {
            uint32_t unit;
            m68k_addr_type buff;
            uint32_t len;

            unit = m68ki_read_data_32(params);
            params += 4;
            buff = m68ki_read_data_32(params);
            params += 4;
            len = m68ki_read_data_32(params);
            params += 4;
            sendPacket(unit, buff, len);
        }
        break;

    case MXETH_GET_IPHOST:
        ret = get_params(params, HOST_IP);
        break;
    case MXETH_GET_IPATARI:
        ret = get_params(params, ATARI_IP);
        break;
    case MXETH_GET_NETMASK:
        ret = get_params(params, NETMASK);
        break;
    case MXETH_GET_GATEWAY:
        ret = get_params(params, GATEWAY);
        break;

    default:
        DebugWarning2("Ethernet: unsupported function %d", cmd);
        ret = EINVFN;
        break;
    }
    return ret;
}


int CNetwork::get_params(m68k_addr_type params, GET_PAR which)
{
    uint32_t unit;
    m68k_addr_type name_ptr;
    uint32 name_maxlen;
    const char *text;

    unit = m68ki_read_data_32(params);
    params += 4;
    name_maxlen = m68ki_read_data_32(params);
    params += 4;
    name_ptr = params;

    DebugInfo2("ETH%d: getPAR(%d) to buffer at %x of size %d",
            unit, which, name_ptr, name_maxlen);

    if (unit >= MAX_ETH)
    {
        DebugError2("Ethernet: handler for %d not found", unit);
        return 0;
    }
    switch (which)
    {
        case HOST_IP: text = Preferences::eth[unit].host_ip; break;
        case ATARI_IP:text = Preferences::eth[unit].atari_ip; break;
        case NETMASK: text = Preferences::eth[unit].netmask; break;
        case GATEWAY: text = Preferences::eth[unit].gateway; break;
        default: text = "";
    }

    Host2AtariSafeStrncpy(name_ptr, text, name_maxlen);
    return strlen(text);
}


int32_t CNetwork::readPacketLength(unsigned int ethX)
{
    Handler *handler = getHandler(ethX, true);
    if (handler == NULL)
        return 0;
    return handler->packet_length;
}


/*
 *  ETHERNETDriver ReadPacket routine
 */

void CNetwork::readPacket(unsigned int ethX, m68k_addr_type buffer, uint32_t len)
{
    Handler *handler = getHandler(ethX, true);
    if (handler == NULL)
        return;
    DebugInfo2("ETH%d: ReadPacket dest %08x, len %x", ethX, buffer, len);
    Host2Atari_memcpy(buffer, handler->packet, MIN(len, MAX_PACKET_SIZE));
    if (len > MAX_PACKET_SIZE)
    {
        DebugError2("ETH%d: readPacket() - length %d > %d", ethX, len, MAX_PACKET_SIZE);
    }
}


/*
 *  ETHERNETDriver writePacket routine
 */
void CNetwork::sendPacket(unsigned int ethX, m68k_addr_type buffer, uint32_t len)
{
    Handler *handler = getHandler(ethX, true);
    if (handler == NULL)
        return;
    uint8 packetToWrite[MAX_PACKET_SIZE+2];

    DebugInfo2("ETH%d: SendPacket src %08x, len %x", ethX, buffer, len);

    len = MIN(len, MAX_PACKET_SIZE);
    Atari2Host_memcpy(packetToWrite, buffer, len);

    if (handler->debug)
    {
        fprintf(stderr, "ETH%d: send %4d:", ethX, len);
        dump_ether_packet(packetToWrite, len);
    }

    // Transmit packet
    if (handler->send(packetToWrite, len) < 0)
    {
        DebugWarning2("ETH%d: Couldn't transmit packet", ethX);
    }
}


/*
 *  Start packet reception thread
 */
bool CNetwork::startThread(unsigned int ethX)
{
    pthread_attr_t attr;

    Handler *handler = getHandler(ethX, true);
    DebugInfo2("ETH%d: Start thread: %s", ethX, handler ? handler->type() : "<none>");
    if (handler == NULL)
        return false;
    if (handler->handlingThread == 0)
    {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        if (pthread_create(&handler->handlingThread, &attr, receiveFunc, handler) < 0)
        {
            DebugWarning2("ETH%d: Cannot start ethernet driver thread", ethX);
            return false;
        }
    }
    return true;
}


/*
 *  Stop packet reception thread
 */
void CNetwork::stopThread(Handler *handler)
{
    if (handler == NULL)
        return;
    if (handler->handlingThread)
    {
        DebugInfo2("ETH%d: Stop thread", handler->ethX);

        pthread_cancel(handler->handlingThread);
#ifndef __APPLE__ /* seems to hang :( */
        pthread_join(handler->handlingThread, NULL);
#endif
        handler->handlingThread = 0;
    }
}


/*
 *  Packet reception thread
 */
void *CNetwork::receiveFunc(void *arg)
{
    Handler *handler = (Handler*)arg;

    // Call protocol handler for received packets
    // ssize_t length;
    for (;;)
    {
        // Read packet device
        handler->packet_length = handler->recv(handler->packet, MAX_PACKET_SIZE);

        // Trigger ETHERNETDriver interrupt (call the m68k side)
        DebugInfo2("ETH%d: packet received (len %d), triggering ETHERNETDriver interrupt", handler->ethX, (int)handler->packet_length);
        if (handler->debug)
        {
            fprintf(stderr, "ETH%d: recv %4d:", handler->ethX, (int)handler->packet_length);
            dump_ether_packet(handler->packet, handler->packet_length);
        }

        /*
         * The Atari driver does not like to see negative values here
         */
        if (handler->packet_length < 0)
            handler->packet_length = 0;

        /* but needs to be triggered, anyway */
        pthread_mutex_lock(&handler->intLock);
        pending_interrupts |= (1 << handler->ethX);
        // Ethernet runs at interrupt level 3 by default but can be reconfigured
        switch (Preferences::eth[handler->ethX].intlevel)
        {
            case 0:
                break;
            case 3:
                /* TriggerInt3(); */
                break;
            case 4:
            default:
                sendVBL();
                break;
            case 5:
                /* TriggerInt5(); */
                break;
        }
        // Wait for interrupt acknowledge (m68k network driver read interrupt to finish)
        DebugInfo2("ETH%d: waiting for int acknowledge with pending irq mask %02x", handler->ethX, pending_interrupts);
        pthread_cond_wait(&handler->intAck, &handler->intLock);
        pending_interrupts &= ~(1 << handler->ethX);
        pthread_mutex_unlock(&handler->intLock);
        DebugInfo2("ETH%d: int acknowledged, pending irq mask now %02x", handler->ethX, pending_interrupts);
    }

    return 0;
}


/*
 *  Debug output
 */
void CNetwork::dump_ether_packet(const uint8_t *buf, int len)
{
    int i;
    int type = 0;
    const char *proto;

    /* ethernet header */
    if (len >= 14)
    {
        /* dest mac */
        fprintf(stderr, "eth: dst %02x:%02x:%02x:%02x:%02x:%02x ",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
        /* source mac */
        fprintf(stderr, "src %02x:%02x:%02x:%02x:%02x:%02x ",
            buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
        /* ether type */
        type = (buf[12] << 8) | buf[13];
        fprintf(stderr, "type %04x  ", type);
        len -= 14;
        buf += 14;
    }

    /* IP header */
    if (len >= 20 && type == 0x800 && (buf[0] & 0xf0) == 0x40) /* IPv4 */
    {
        fprintf(stderr, "ip: ");
        /* version/hd_len/tos */
        fprintf(stderr, "hl %02x tos %02x ", buf[0], buf[1]);
        /* length */
        fprintf(stderr, "length %5d ", (buf[2] << 8) | buf[3]);
        /* ident */
        fprintf(stderr, "ident %5d ", (buf[4] << 8) | buf[5]);
        /* fragment */
        fprintf(stderr, "frag %04x ", (buf[6] << 8) | buf[7]);
        /* ttl/protocol */
        switch (buf[9])
        {
            case 0: proto = "IP"; break;
            case 1: proto = "ICMP"; break;
            case 2: proto = "IGMP"; break;
            case 3: proto = "GGP"; break;
            case 4: proto = "IPinIP"; break;
            case 5: proto = "ST"; break;
            case 6: proto = "TCP"; break;
            case 7: proto = "CBT"; break;
            case 8: proto = "EGP"; break;
            case 9: proto = "IGP"; break;
            case 10: proto = "BBN"; break;
            case 11: proto = "NVP"; break;
            case 12: proto = "PUP"; break;
            case 13: proto = "ARGUS"; break;
            case 14: proto = "EMCON"; break;
            case 15: proto = "XNET"; break;
            case 16: proto = "CHAOS"; break;
            case 17: proto = "UDP"; break;
            case 18: proto = "MUX"; break;
            case 21: proto = "PRM"; break;
            case 22: proto = "IDP"; break;
            case 29: proto = "TP"; break;
            case 31: proto = "DCCP"; break;
            case 40: proto = "IL"; break;
            case 41: proto = "IPv6"; break;
            case 42: proto = "SDRP"; break;
            case 43: proto = "IPv6-Route"; break;
            case 44: proto = "IPv6-Frag"; break;
            case 46: proto = "RSVP"; break;
            case 47: proto = "GRE"; break;
            case 50: proto = "ESP"; break;
            case 51: proto = "AH"; break;
            case 56: proto = "TLSP"; break;
            case 92: proto = "MTP"; break;
            case 94: proto = "BEETPH"; break;
            case 97: proto = "ETHERIP"; break;
            case 98: proto = "ENCAP"; break;
            case 103: proto = "PIM"; break;
            case 108: proto = "COMP"; break;
            case 115: proto = "L2TP"; break;
            case 121: proto = "SMP"; break;
            case 122: proto = "SM"; break;
            case 132: proto = "SCTP"; break;
            case 136: proto = "UDPLITE"; break;
            case 137: proto = "MPLS"; break;
            case 143: proto = "ETHERNET"; break;
            case 255: proto = "RAW"; break;
            /* case 262: proto = "MPTCP"; break; 262 does not fit in uint8 */
            default: proto = "unknown"; break;
        }
        fprintf(stderr, "ttl %02x proto %02x(%s) ", buf[8], buf[9], proto);
        /* header checksum */
        fprintf(stderr, "csum %04x ", (buf[10] << 8) | buf[11]);
        /* source ip */
        fprintf(stderr, "src %d.%d.%d.%d ", buf[12], buf[13], buf[14], buf[15]);
        /* dest ip */
        fprintf(stderr, "dst %d.%d.%d.%d  ", buf[16], buf[17], buf[18], buf[19]);
        len -= 20;
        buf += 20;
    }
    else
    if ((len >= 28) && (type == 0x0806 || type == 0x8035)) /* ARP/RARP */
    {
        if (type == 0x0806)
            fprintf(stderr, "arp: ");
        else
            fprintf(stderr, "rarp: ");
        /* hardware space */
        fprintf(stderr, "hws %04x ", (buf[0] << 8) | buf[1]);
        /* protocol space */
        fprintf(stderr, "prs %04x ", (buf[2] << 8) | buf[3]);
        /* hardware_len/protcol_len */
        fprintf(stderr, "hwl %02x prl %02x ", buf[4], buf[5]);
        /* opcode */
        fprintf(stderr, "opcode %04x ", (buf[6] << 8) | buf[7]);
        /* source mac */
        fprintf(stderr, "src %02x:%02x:%02x:%02x:%02x:%02x ",
            buf[8], buf[9], buf[10], buf[11], buf[12], buf[13]);
        /* source ip */
        fprintf(stderr, "%d.%d.%d.%d ", buf[14], buf[15], buf[16], buf[17]);
        /* dest mac */
        fprintf(stderr, "dst %02x:%02x:%02x:%02x:%02x:%02x ",
            buf[18], buf[19], buf[20], buf[21], buf[22], buf[23]);
        /* dest ip */
        fprintf(stderr, "%d.%d.%d.%d ", buf[24], buf[25], buf[26], buf[27]);
        len -= 28;
        buf += 28;
    }

    fprintf(stderr, " data:");
    /* payload data */
    for (i = 0; i < len; i++)
        fprintf(stderr, " %02x", buf[i]);
    fprintf(stderr, "\n");
    fflush(stderr);
}
