#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "ip2.h"
#include "udp.h"

#include "sock.h"

#define MAX_SOCKS 128

static struct sock socks[MAX_SOCKS];

int sockaddr_pton(const char *p, struct sockaddr *n, size_t size)
{
    struct IP_ENDPOINT ep;

    if (ip_string_to_endpoint(p, &ep) == 0 && size >= sizeof(struct sockaddr_in))
    {
        ((struct sockaddr_in *)n)->sin_family = AF_INET;
        ((struct sockaddr_in *)n)->sin_port = ep.port;
        ((struct sockaddr_in *)n)->sin_addr = ep.address;
        return 0;
    }
    return -1;
}

char *sockaddr_ntop(const struct sockaddr *n, char *p, size_t size)
{
    if (n->sa_family == AF_INET && size >= MAX_IP_ENDPOINT_STRING_LENGTH)
    {
        struct IP_ENDPOINT ep = {
            .port = ((struct sockaddr_in *)n)->sin_port,
            .address = ((struct sockaddr_in *)n)->sin_addr
        };
        return ip_endpoint_to_string(&ep, p, size);
    }
    return NULL;
}

static struct sock *sock_alloc(void)
{
    for (size_t i = 0; i < MAX_SOCKS; i++)
    {
        if (!socks[i].used)
        {
            socks[i].used = 1;
            return &socks[i];
        }
    }
    return NULL;
}

static struct sock *sock_get(int id)
{
    if (id < 0 || (size_t)id >= MAX_SOCKS)
    {
        return NULL;
    }
    return &socks[id];
}

int sock_open(int domain, int type, int protocol)
{
    if (domain != AF_INET || (type != SOCK_STREAM && type != SOCK_DGRAM) || protocol != 0)
    {
        return -1;
    }

    struct sock *s = sock_alloc();
    if (!s)
    {
        return -1;
    }

    s->family = domain;
    s->type = type;

    switch (s->type)
    {
    case SOCK_DGRAM:
        s->desc = open_new_udp_socket();
        break;
    }

    if (s->desc == -1)
    {
        return -1;
    }

    return (int)(s - socks);
}

int sock_close(int id)
{
    struct sock *s = sock_get(id);
    if (!s)
    {
        return -1;
    }

    switch (s->type)
    {
    case SOCK_DGRAM:
        close_udp_socket(s->desc);
        break;
    default:
        return -1;
    }

    s->used = 0; // Mark the socket as unused
    return 0;
}

ssize_t sock_recvfrom(int id, void *buf, size_t n, struct sockaddr *addr, int *addrlen)
{
    struct sock *s = sock_get(id);
    if (!s || s->type != SOCK_DGRAM)
    {
        return -1;
    }

    struct IP_ENDPOINT ep;
    int ret = receive_udp_packet_from_socket(s->desc, (uint8_t *)buf, n, &ep);
    if (ret != -1 && addr && addrlen)
    {
        ((struct sockaddr_in *)addr)->sin_family = AF_INET;
        ((struct sockaddr_in *)addr)->sin_addr = ep.address;
        ((struct sockaddr_in *)addr)->sin_port = ep.port;
        *addrlen = sizeof(struct sockaddr_in);
    }
    return ret;
}

ssize_t sock_sendto(int id, const void *buf, size_t n, const struct sockaddr *addr, int addrlen)
{
    struct sock *s = sock_get(id);
    if (!s || s->type != SOCK_DGRAM)
    {
        return -1;
    }

    if (addr == NULL || addrlen != sizeof(struct sockaddr_in))
    {
        return -1;
    }

    struct IP_ENDPOINT ep = {
        .address = ((struct sockaddr_in *)addr)->sin_addr,
        .port = ((struct sockaddr_in *)addr)->sin_port
    };
    return send_udp_packet_over_socket(s->desc, (uint8_t *)buf, n, &ep);
}

int sock_bind(int id, const struct sockaddr *addr, int addrlen)
{
    struct sock *s = sock_get(id);
    if (!s || s->type != SOCK_DGRAM)
    {
        return -1;
    }

    if (addr == NULL || addrlen != sizeof(struct sockaddr_in))
    {
        return -1;
    }

    struct IP_ENDPOINT ep = {
        .address = ((struct sockaddr_in *)addr)->sin_addr,
        .port = ((struct sockaddr_in *)addr)->sin_port
    };
    return bind_udp_socket_to_local_endpoint(s->desc, &ep);
}
