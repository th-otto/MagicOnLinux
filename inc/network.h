/*
 * Copyright (C) 1990-2025 Andreas Kromke, andreas.kromke@gmail.com (Thorsten Otto)
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
* Does everything that deals with network access
*
*/

#include <stdint.h>
#include "Atari.h"
#include "preferences.h"

#define MAX_PACKET_SIZE    9000

class CNetwork
{
   public:
    typedef uint32_t m68k_addr_type;
    typedef uint32_t m68k_data_type;

    static uint32_t AtariNetwork(m68k_addr_type params, uint8_t *addrOffset68k);
    void init(void);
    void exit(void);
    void reset(void);

    class Handler
    {
    public:
        unsigned int ethX;
        ssize_t packet_length;
        uint8_t packet[MAX_PACKET_SIZE+2];
        pthread_t handlingThread;    // Packet reception thread
        pthread_cond_t intAck;            // Interrupt acknowledge semaphore
        pthread_mutex_t intLock;
        bool debug;

        Handler(unsigned int eth_idx)
        {
            ethX = eth_idx;
            packet_length = 0;
            handlingThread = 0;
            pthread_cond_init(&intAck, NULL);
            pthread_mutex_init(&intLock, NULL);
            debug = false;
        }
        virtual ~Handler()
        {
            pthread_mutex_destroy(&intLock);
            pthread_cond_destroy(&intAck);
        }
        virtual bool open() = 0;
        virtual void close() = 0;
        virtual int recv(uint8_t *, int) = 0;
        virtual int send(const uint8_t *, int) = 0;
        virtual const char *type(void) = 0;
    };

   private:
    static Handler *handlers[MAX_ETH];
    static Handler *getHandler(unsigned int ethX, bool msg);
    static int pending_interrupts;

    static int32_t readPacketLength(unsigned int ethX);
    static void readPacket(unsigned int ethX, m68k_addr_type buffer, uint32_t len);
    static void sendPacket(unsigned int ethX, m68k_addr_type buffer, uint32_t len);

    // emulators handling the TAP device
    static bool startThread(unsigned int ethX);
    static void stopThread(Handler *handler);
    static void *receiveFunc(void *arg);

protected:
    typedef enum { HOST_IP, ATARI_IP, NETMASK, GATEWAY } GET_PAR;
    static int get_params(m68k_addr_type params, GET_PAR which);
    static void dump_ether_packet(const uint8_t *buf, int len);
};


#if defined(__linux__) || defined(__APPLE__)

class TunTapEthernetHandler : public CNetwork::Handler
{
    int fd;

    // the /dev/net/tun driver (TAP)
    int tapOpenOld(char *dev);
    int tapOpen(char *dev);

public:
    TunTapEthernetHandler(unsigned int eth_idx);
    virtual ~TunTapEthernetHandler();
    virtual const char *type(void) { return "TunTapEthernetHandler"; }

    virtual bool open();
    virtual void close();
    virtual int recv(uint8_t *buf, int len);
    virtual int send(const uint8_t *buf, int len);
};

#define ETHERNET_HANDLER_CLASSNAME TunTapEthernetHandler

#endif

#if defined(_WIN32) || defined(__CYGWIN__)
class WinTapEthernetHandler : public CNetwork::Handler
{
    OVERLAPPED read_overlapped;
    OVERLAPPED write_overlapped;
    HANDLE device_handle;
    char *device;
    char *iface;

    int device_total_in;
    int device_total_out;

public:
    WinTapEthernetHandler(unsigned int eth_idx);
    virtual ~WinTapEthernetHandler();
    virtual const char *type(void) { return "WinTapEthernetHandler"; }

    virtual bool open();
    virtual void close();
    virtual int recv(uint8_t *buf, int len);
    virtual int send(const uint8_t *buf, int len);
};

#define ETHERNET_HANDLER_CLASSNAME WinTapEthernetHandler

#endif

#if 0
class BPFEthernetHandler : public CNetwork::Handler
{
    int fd;
    int buf_len;
    struct bpf_hdr* bpf_buf;

    // variables used for looping over multiple packets
    int read_len;
    struct bpf_hdr* bpf_packet;

    void reset_read_pos();

public:
    BPFEthernetHandler(unsigned int eth_idx);
    virtual ~BPFEthernetHandler();
    virtual const char *type(void) { return "BPFEthernetHandler"; }

    virtual bool open();
    virtual void close();
    virtual int recv(uint8_t *buf, int len);
    virtual int send(const uint8_t *buf, int len);
};

#define ETHERNET_HANDLER_CLASSNAME BPFEthernetHandler

#endif
