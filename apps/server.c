#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "util.h"
#include "net2.h"
#include "ip2.h"
#include "icmp.h"
#include "udp.h"
#include "sock.h"

#include "loopback.h"
#include "ethertap.h"

#include "params.h"

static volatile sig_atomic_t terminate;

static void on_signal(int s)
{
    (void)s;
    terminate = 1;
    network_interrupt();
}

/**
 * @brief Initializes the network environment.
 * 
 * @return 0 on success, -1 on failure
 */
static int setup(void)
{
    struct network_device *dev;
    struct IP_INTERFACE *iface;

    /* register signal handler */
    signal(SIGINT, on_signal);

    /* initialize network stack */
    if (network_init() == -1) {
        errorf("net_init() failure");
        return -1;
    }

    /* initialize ethernet interface */
    dev = ether_tap_init(ETHER_TAP_NAME, ETHER_TAP_HW_ADDR);
    if (!dev) {
        errorf("ether tap inititialize failure");
        return -1;
    }

    /* allocate ip interface for ethernet */
    iface = ip_allocate_interface(ETHER_TAP_IP_ADDR, ETHER_TAP_NETMASK);
    if (!iface) {
        errorf("ip interface allocate failure");
        return -1;
    }

    /* register ethernet interface to network device */
    if (ip_register_interface(dev, iface) == -1) {
        errorf("ip register interface failure");
        return -1;
    }

    /* set default gateway for ethernet */
    if (ip_set_default_gateway(iface, DEFAULT_GATEWAY) == -1) {
        errorf("ip set default gateway failure");
        return -1;
    }

    /* run network */
    if (network_run() == -1) {
        errorf("network run failure");
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    /**
     * @file udp_app.c
     * @brief UDP application code for sending and receiving data over UDP.
     *
     * This is an UDP application that sends and receives data over UDP.
     * It binds to a specified address and port, and then continuously receives data from remote hosts
     * and sends back the received data to the sender.
     *
     * Usage: udp_app [addr] port
     *
     * The application takes two command line arguments: the address to bind to and the port number.
     * If no address is provided, it binds to the default address. If no port number is provided,
     * it binds to the default port.
     *
     * The application uses the following functions:
     * - ip_string_to_address(): Converts a string representation of an IP address to a binary format.
     * - setup(): Performs any necessary setup operations.
     * - sock_open(): Opens a socket for communication.
     * - sock_bind(): Binds a socket to a local address and port.
     * - sock_recvfrom(): Receives data from a socket.
     * - sock_sendto(): Sends data to a remote host.
     * - close_udp_socket(): Closes the UDP socket.
     * - network_shutdown(): Shuts down the network.
     */
    int soc;
    long int port;
    struct sockaddr_in local = { .sin_family=AF_INET }, foreign;
    int foreignlen;
    uint8_t buf[1024];
    char addr[SOCKADDR_STR_LEN];
    ssize_t ret;


    switch (argc) {
    case 3:
        if (ip_string_to_address(argv[argc-2], &local.sin_addr) == -1) {
            errorf("ip convertion to string failure, addr=%s", optarg);
            return -1;
        }

    case 2:
        port = strtol(argv[argc-1], NULL, 10);
        if (port < 0 || port > UINT16_MAX) {
            errorf("invalid port, port=%s", optarg);
            return -1;
        }
        local.sin_port = hton16(port);
        break;
    default:
        fprintf(stderr, "Usage: %s [addr] port\n", argv[0]);
        return -1;
    }

    if (setup() == -1) {
        errorf("socket setup failure");
        return -1;
    }
  
    soc = sock_open(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (soc == -1) {
        errorf("sock opening failure");
        return -1;
    }
    if (sock_bind(soc, (struct sockaddr *)&local, sizeof(local)) == -1) {
        errorf("sock binding failure");
        return -1;
    }
    while (!terminate) {
        foreignlen = sizeof(foreignlen);
        ret = sock_recvfrom(soc, buf, sizeof(buf), (struct sockaddr *)&foreign, &foreignlen);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            errorf("sock receving failure");
            break;
        }
        infof("%zu bytes data form %s", ret, sockaddr_ntop((struct sockaddr *)&foreign, addr, sizeof(addr)));
        hexdump(stderr, buf, ret);
        if (sock_sendto(soc, buf, ret, (struct sockaddr *)&foreign, foreignlen) == -1) {
            errorf("sock sending failure");
            break;
        }
    }
    close_udp_socket(soc);

    network_shutdown();
    return 0;
}