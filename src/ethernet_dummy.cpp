/**
 * Ethernet Emulation using tuntap driver in Linux
 *
 * Standa and Joy of ARAnyM team (c) 2004-2008
 *
 * GPL
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
#undef flock /* conflicts with struct flock */
#include "network.h"

DummyEthernetHandler::DummyEthernetHandler(unsigned int eth_idx)
    : Handler(eth_idx)
{
}

DummyEthernetHandler::~DummyEthernetHandler()
{
    close();
}

bool DummyEthernetHandler::open()
{
    return false;
}

void DummyEthernetHandler::close()
{
    DebugError2("ETH%d: close", ethX);
}


int DummyEthernetHandler::recv(uint8_t *buf, int len)
{
    (void)buf;
    (void)len;
    return -1;
}

int DummyEthernetHandler::send(const uint8_t *buf, int len)
{
    (void)buf;
    (void)len;
    return -1;
}
