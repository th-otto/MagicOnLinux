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
#include <sys/poll.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <errno.h>
#include <unistd.h>

#if defined(__linux__) || defined(__APPLE__)

/****************************
 * Configuration zone begins
 */

static char tap_init[] = "aratapif";
static char tap_mtu[] = "1500";

/*
 * Configuration zone ends
 **************************/

TunTapEthernetHandler::TunTapEthernetHandler(unsigned int eth_idx)
    : Handler(eth_idx),
    fd(-1)
{
}

TunTapEthernetHandler::~TunTapEthernetHandler()
{
    close();
}

bool TunTapEthernetHandler::open()
{
    // int nonblock = 1;
    int type = Preferences::eth[ethX].type;
    char *devName = Preferences::eth[ethX].tunnel;

    close();

    if (type == ETH_TYPE_NONE)
    {
        return false;
    }

    // get the tunnel nif name if provided
    if (strlen(devName) == 0)
    {
        DebugInfo2("ETH%d: tunnel name undefined", ethX);
        return false;
    }

    DebugInfo2("ETH%d: open('%s')", ethX, devName);

    fd = tapOpen(devName);
    if (fd < 0)
    {
        DebugError2("ETH%d: NO_NET_DRIVER_WARN '%s': %s", ethX, devName, strerror(errno));
        return false;
    }

    // if 'bridge' mode then we are done
    if (type == ETH_TYPE_BRIDGE)
        return true;

    int pid = fork();
    if (pid < 0)
    {
        DebugError2("ETH%d: ERROR: fork() failed. Ethernet disabled!", ethX);
        close();
        return false;
    }

    if (pid == 0)
    {
        // the arguments _need_ to be placed into the child process
        // memory (otherwise this does not work here)
        char *args[] = {
            tap_init,
            Preferences::eth[ethX].tunnel,
            Preferences::eth[ethX].host_ip,
            Preferences::eth[ethX].atari_ip,
            Preferences::eth[ethX].netmask,
            tap_mtu, NULL
        };
        int result;
        result = execvp(tap_init, args);
        _exit(result);
    }

    DebugInfo2("ETH%d: waiting for %s at pid %d", ethX, tap_init, pid);
    int status;
    waitpid(pid, &status, 0);
    bool failed = true;
    if (WIFEXITED(status))
    {
        int err = WEXITSTATUS(status);
        if (err == 255)
        {
            DebugError2("ETH%d: ERROR: %s not found. Ethernet disabled!", ethX, tap_init);
        } else if (err != 0)
        {
            DebugError2("ETH%d: ERROR: %s failed (code %d). Ethernet disabled!", ethX, tap_init, err);
        } else
        {
            failed = false;
            DebugInfo2("ETH%d: %s initialized OK", ethX, tap_init);
        }
    } else
    {
        DebugError2("ETH%d: ERROR: %s could not be started. Ethernet disabled!", ethX, tap_init);
    }

    // Close /dev/net/tun device if exec failed
    if (failed)
    {
        close();
        return false;
    }

    // Set nonblocking I/O
    //ioctl(fd, FIONBIO, &nonblock);

    return true;
}

void TunTapEthernetHandler::close()
{
    DebugError2("ETH%d: close", ethX);

    // Close /dev/net/tun device
    if (fd > 0)
    {
        ::close(fd);
        fd = -1;
    }
}

int TunTapEthernetHandler::recv(uint8_t *buf, int len)
{
    /*
     * this is called from a thread,
     * so we don't need to poll() here
     */
    return ::read(fd, buf, len);
}

int TunTapEthernetHandler::send(const uint8_t *buf, int len)
{
    int res = write(fd, buf, len);

    if (res < 0)
    {
        DebugWarning2("ETH%d: Couldn't transmit packet", ethX);
    }
    return res;
}


/*
 * Allocate ETHERNETDriver TAP device, returns opened fd.
 * Stores dev name in the first arg(must be large enough).
 */
int TunTapEthernetHandler::tapOpenOld(char *dev)
{
    char tapname[sizeof(Preferences::eth[0].tunnel) + 5];
    int i, fd;

    if (*dev)
    {
        sprintf(tapname, "/dev/%s", dev);
        DebugInfo2("ETH%d: tapOpenOld %s", ethX, tapname);
        return ::open(tapname, O_RDWR);
    }

    for (i = 0; i < 255; i++)
    {
        sprintf(tapname, "/dev/tap%d", i);
        /* Open device */
        if ((fd = ::open(tapname, O_RDWR)) > 0)
        {
            sprintf(dev, "tap%d",i);
            DebugInfo2("ETH%d: tapOpenOld %s", ethX, dev);
            return fd;
        }
    }
    return -1;
}

#ifdef __linux__
#include <linux/if_tun.h>

/* pre 2.4.6 compatibility */
#define OTUNSETNOCSUM  (('T'<< 8) | 200)
#define OTUNSETDEBUG   (('T'<< 8) | 201)
#define OTUNSETIFF     (('T'<< 8) | 202)
#define OTUNSETPERSIST (('T'<< 8) | 203)
#define OTUNSETOWNER   (('T'<< 8) | 204)

int TunTapEthernetHandler::tapOpen(char *dev)
{
    struct ifreq ifr;

    if ((fd = ::open("/dev/net/tun", O_RDWR)) < 0)
    {
        fprintf(stderr, "ETH%d: Error opening /dev/net/tun. Check if module is loaded and privileges are OK", ethX);
        return tapOpenOld(dev);
    }

    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (*dev)
        memcpy(ifr.ifr_name, dev, MIN(strlen(dev), IFNAMSIZ));

    if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0)
    {
        if (errno != EBADFD)
            goto failed;

        /* Try old ioctl */
        if (ioctl(fd, OTUNSETIFF, (void *) &ifr) < 0)
            goto failed;
    }

    strcpy(dev, ifr.ifr_name);

    DebugInfo2("ETH%d: if opened %s", ethX, dev);
    return fd;

  failed:
    close();
    return -1;
}

#else

int TunTapEthernetHandler::tapOpen(char *dev)
{
    return tapOpenOld(dev);
}

#endif /* New driver support */

#endif /* __linux__ */
