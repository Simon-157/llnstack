#include "app.h"

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
     * two command line arguments: the address to bind to and the port number.
     * If no address is provided, it binds to the default address. If no port number is provided,
     * it binds to the default port.
     *
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
        fprintf(stderr, "Usage: %s [ip_address] port\n", argv[0]);
        return -1;
    }

    if (setup_network() == -1) {
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
        close_udp_socket(soc);
        network_shutdown();
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
        infof("%zu bytes data from %s", ret, sockaddr_ntop((struct sockaddr *)&foreign, addr, sizeof(addr)));
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
