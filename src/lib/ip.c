#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "handler.h"
#include "util.h"
#include "net2.h"
#include "arp.h"
#include "ip2.h"

const IPAddress IP_ADDR_ANY = 0x00000000; /* 0.0.0.0 */
const IPAddress IP_ADDR_BROADCAST = 0xffffffff; /* 255.255.255.255 */

struct ip_protocol {
    struct ip_protocol *next;
    char name[16];
    uint8_t type;
    void (*handler)(const uint8_t *data, size_t len, IPAddress src, IPAddress dst, struct IP_INTERFACE *iface);
};

struct ip_route {
    struct ip_route *next;
    IPAddress network;
    IPAddress netmask;
    IPAddress nexthop;
    struct IP_INTERFACE *iface;
};

struct ip_hdr {
    uint8_t vhl;
    uint8_t tos;
    uint16_t total;
    uint16_t id;
    uint16_t offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t sum;
    IPAddress src;
    IPAddress dst;
    uint8_t options[0];
};

static struct IP_INTERFACE *ifaces;
static struct ip_protocol *protocols;
static struct ip_route *routes;

int ip_string_to_address(const char *p, IPAddress *n) {
    char *sp, *ep;
    int idx;
    long ret;

    sp = (char *)p;
    for (idx = 0; idx < 4; idx++) {
        ret = strtol(sp, &ep, 10);
        if (ret < 0 || ret > 255 || ep == sp) {
            return -1;
        }
        if ((idx == 3 && *ep != '\0') || (idx != 3 && *ep != '.')) {
            return -1;
        }
        ((uint8_t *)n)[idx] = ret;
        sp = ep + 1;
    }
    return 0;
}

char *ip_address_to_string(const IPAddress n, char *p, size_t size) {
    uint8_t *u8;
    u8 = (uint8_t *)&n;
    snprintf(p, size, "%d.%d.%d.%d", u8[0], u8[1], u8[2], u8[3]);
    return p;
}

int ip_string_to_endpoint(const char *p, struct IP_ENDPOINT *n) {
    char *sep;
    char addr[MAX_IP_ADDRESS_STRING_LENGTH] = {};
    long int port;

    sep = strrchr(p, ':');
    if (!sep) {
        return -1;
    }
    memcpy(addr, p, sep - p);
    if (ip_string_to_address(addr, &n->address) == -1) {
        return -1;
    }
    port = strtol(sep + 1, NULL, 10);
    if (port <= 0 || port > UINT16_MAX) {
        return -1;
    }
    n->port = hton16(port);
    return 0;
}

char *ip_endpoint_to_string(const struct IP_ENDPOINT *n, char *p, size_t size) {
    size_t offset;
    ip_address_to_string(n->address, p, size);
    offset = strlen(p);
    snprintf(p + offset, size - offset, ":%d", ntoh16(n->port));
    return p;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void ip_dump(const uint8_t *data, size_t len) {
    struct ip_hdr *hdr;
    uint8_t v, hl, hlen;
    uint16_t total, offset;
    char addr[MAX_IP_ADDRESS_STRING_LENGTH];

    /* Lock the standard error stream for thread safety */
    flockfile(stderr);

    /* Extract IP header information */
    hdr = (struct ip_hdr *)data;
    v = (hdr->vhl & 0xf0) >> 4;
    hl = hdr->vhl & 0x0f;
    hlen = hl << 2;

    /* Print IP header fields */
    fprintf(stderr, "\x1b[33mVersion and Header Length (vhl):\x1b[0m 0x%02x [\x1b[32mVersion:\x1b[0m %u, \x1b[32mHeader Length:\x1b[0m %u (%u bytes)]\n",
            hdr->vhl, v, hl, hlen);
    fprintf(stderr, "\x1b[33mType of Service (tos):\x1b[0m 0x%02x\n", hdr->tos);
    total = ntoh16(hdr->total);
    fprintf(stderr, "\x1b[33mTotal Length:\x1b[0m %u bytes (\x1b[32mPayload Length:\x1b[0m %u bytes)\n", total, total - hlen);
    fprintf(stderr, "\x1b[33mIdentification (id):\x1b[0m %u\n", ntoh16(hdr->id));
    offset = ntoh16(hdr->offset);
    fprintf(stderr, "\x1b[33mFragment Offset:\x1b[0m 0x%04x [\x1b[32mFlags:\x1b[0m %x, \x1b[32mOffset:\x1b[0m %u]\n", offset, (offset & 0xe000) >> 13, offset & 0x1fff);
    fprintf(stderr, "\x1b[33mTime to Live (ttl):\x1b[0m %u\n", hdr->ttl);
    fprintf(stderr, "\x1b[33mProtocol:\x1b[0m %u (\x1b[32m%s\x1b[0m)\n", hdr->protocol, ip_get_protocol_name(hdr->protocol));
    fprintf(stderr, "\x1b[33mHeader Checksum:\x1b[0m 0x%04x (\x1b[32mCalculated Checksum:\x1b[0m 0x%04x)\n",
            ntoh16(hdr->sum), ntoh16(cksum16((uint16_t *)data, hlen, -hdr->sum)));
    fprintf(stderr, "\x1b[33mSource IP Address:\x1b[0m %s\n", ip_address_to_string(hdr->src, addr, sizeof(addr)));
    fprintf(stderr, "\x1b[33mDestination IP Address:\x1b[0m %s\n", ip_address_to_string(hdr->dst, addr, sizeof(addr)));

#ifdef HEXDUMP
    /* Print hexadecimal dump if enabled */
    hexdump(stderr, data, len);
#endif

    /* Unlock the standard error stream */
    funlockfile(stderr);
}

static struct ip_route *ip_route_add(IPAddress network, IPAddress netmask, IPAddress nexthop, struct IP_INTERFACE *iface) {
    struct ip_route *route;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr3[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr4[MAX_IP_ADDRESS_STRING_LENGTH];

    route = memory_alloc(sizeof(*route));
    if (!route) {
        errorf("memory allocation failed");
        return NULL;
    }
    route->network = network;
    route->netmask = netmask;
    route->nexthop = nexthop;
    route->iface = iface;
    route->next = routes;
    routes = route;
    infof("network=%s, netmask=%s, nexthop=%s, iface=%s dev=%s",
        ip_address_to_string(route->network, addr1, sizeof(addr1)),
        ip_address_to_string(route->netmask, addr2, sizeof(addr2)),
        ip_address_to_string(route->nexthop, addr3, sizeof(addr3)),
        ip_address_to_string(route->iface->unicast, addr4, sizeof(addr4)),
        NETWORK_INTERFACE(iface)->dev->name
    );
    return route;
}

static struct ip_route *ip_route_lookup(IPAddress dst) {
    struct ip_route *route, *candidate = NULL;
    for (route = routes; route; route = route->next) {
        if ((dst & route->netmask) == route->network) {
            if (!candidate || ntoh32(candidate->netmask) < ntoh32(route->netmask)) {
                candidate = route;
            }
        }
    }
    return candidate;
}

int ip_set_default_gateway(struct IP_INTERFACE *iface, const char *gateway) {
    IPAddress gw;
    if (ip_string_to_address(gateway, &gw) == -1) {
        errorf("ip string to address failure, addr=%s", gateway);
        return -1;
    }
    if (!ip_route_add(IP_ADDR_ANY, IP_ADDR_ANY, gw, iface)) {
        errorf("ip route add failed");
        return -1;
    }
    return 0;
}

struct IP_INTERFACE *ip_get_interface(IPAddress dst) {
    struct ip_route *route;
    route = ip_route_lookup(dst);
    return route ? route->iface : NULL;
}

struct IP_INTERFACE *ip_allocate_interface(const char *unicast, const char *netmask) {
    struct IP_INTERFACE *iface;
    iface = memory_alloc(sizeof(*iface));
    if (!iface) {
        errorf("allocation of memory failed ");
        return NULL;
    }
    NETWORK_INTERFACE(iface)->family = NETWORK_INTERFACE_FAMILY_IP;
    if (ip_string_to_address(unicast, &iface->unicast) == -1) {
        errorf("converting ip address to string failed, addr=%s", unicast);
        memory_free(iface);
        return NULL;
    }
    if (ip_string_to_address(netmask, &iface->netmask) == -1) {
        errorf("converting ip address to string failure, addr=%s", netmask);
        memory_free(iface);
        return NULL;
    }
    iface->broadcast = (iface->unicast & iface->netmask) | ~iface->netmask;
    return iface;
}

int ip_register_interface(struct network_device *dev, struct IP_INTERFACE *iface) {
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr3[MAX_IP_ADDRESS_STRING_LENGTH];
    if (network_device_add_interface(dev, NETWORK_INTERFACE(iface)) == -1 ||
        !ip_route_add(iface->unicast & iface->netmask, iface->netmask, IP_ADDR_ANY, iface)) {
        errorf("registration failure");
        return -1;
    }
    iface->next = ifaces;
    ifaces = iface;
    infof("registered: dev=%s, unicast=%s, netmask=%s, broadcast=%s",
        dev->name,
        ip_address_to_string(iface->unicast, addr1, sizeof(addr1)),
        ip_address_to_string(iface->netmask, addr2, sizeof(addr2)),
        ip_address_to_string(iface->broadcast, addr3, sizeof(addr3)));
    return 0;
}

struct IP_INTERFACE *ip_select_interface(IPAddress addr) {
    struct IP_INTERFACE *entry;
    for (entry = ifaces; entry; entry = entry->next) {
        if (entry->unicast == addr) {
            break;
        }
    }
    return entry;
}

static void ip_input(const uint8_t *data, size_t len, struct network_device *dev) {
    struct ip_hdr *hdr;
    uint8_t v;
    uint16_t hlen, total, offset;
    struct IP_INTERFACE *iface;
    char addr[MAX_IP_ADDRESS_STRING_LENGTH];
    struct ip_protocol *proto;

    if (len < MIN_IP_HEADER_SIZE || !(iface = (struct IP_INTERFACE *)network_device_get_interface(dev, NETWORK_INTERFACE_FAMILY_IP))) {
        return;
    }
    hdr = (struct ip_hdr *)data;
    v = hdr->vhl >> 4;
    if (v != IPV4 || (hlen = (hdr->vhl & 0x0f) << 2) > len || (total = ntoh16(hdr->total)) > len || cksum16((uint16_t *)hdr, hlen, 0) != 0) {
        return;
    }
    offset = ntoh16(hdr->offset);
    if (offset & 0x2000 || offset & 0x1fff || (hdr->dst != iface->unicast && hdr->dst != iface->broadcast && hdr->dst != IP_ADDR_BROADCAST)) {
        return;
    }
    debugf("dev=%s, iface=%s, protocol=%s(0x%02x), len=%u",
        dev->name, ip_address_to_string(iface->unicast, addr, sizeof(addr)), ip_get_protocol_name(hdr->protocol), hdr->protocol, total);
    ip_dump(data, total);
    for (proto = protocols; proto; proto = proto->next) {
        if (proto->type == hdr->protocol) {
            proto->handler((uint8_t *)hdr + hlen, total - hlen, hdr->src, hdr->dst, iface);
            return;
        }
    }
}

static ssize_t ip_output_device(struct IP_INTERFACE *iface, const uint8_t *data, size_t len, IPAddress dst) {
    uint8_t hwaddr[NETWORK_DEVICE_ADDR_LEN] = {};
    int ret;
    if (NETWORK_INTERFACE(iface)->dev->flags & NETWORK_DEVICE_FLAG_NEED_ARP) {
        if (dst == iface->broadcast || dst == IP_ADDR_BROADCAST) {
            memcpy(hwaddr, NETWORK_INTERFACE(iface)->dev->broadcast, NETWORK_INTERFACE(iface)->dev->address_len);
        } else if ((ret = arp_resolve(NETWORK_INTERFACE(iface), dst, hwaddr)) != ARP_RESOLVE_FOUND) {
            return ret;
        }
    }
    return network_device_output(NETWORK_INTERFACE(iface)->dev, NETWORK_PROTOCOL_TYPE_IP, data, len, hwaddr);
}

static ssize_t ip_output_core(struct IP_INTERFACE *iface, uint8_t protocol, const uint8_t *data, size_t len, IPAddress src, IPAddress dst, IPAddress nexthop, uint16_t id, uint16_t offset) {
    uint8_t buf[MAX_IP_PACKET_SIZE];
    struct ip_hdr *hdr;
    uint16_t hlen, total;
    char addr[MAX_IP_ADDRESS_STRING_LENGTH];

    hdr = (struct ip_hdr *)buf;
    hlen = sizeof(*hdr);
    hdr->vhl = (IPV4 << 4) | (hlen >> 2);
    hdr->tos = 0;
    total = hlen + len;
    hdr->total = hton16(total);
    hdr->id = hton16(id);
    hdr->offset = hton16(offset);
    hdr->ttl = 0xff;
    hdr->protocol = protocol;
    hdr->sum = 0;
    hdr->src = src;
    hdr->dst = dst;
    hdr->sum = cksum16((uint16_t *)hdr, hlen, 0);
    memcpy(hdr + 1, data, len);
    debugf("dev=%s, iface=%s, protocol=%s(0x%02x), len=%u",
        NETWORK_INTERFACE(iface)->dev->name, ip_address_to_string(iface->unicast, addr, sizeof(addr)), ip_get_protocol_name(protocol), protocol, total);
    ip_dump(buf, total);
    return ip_output_device(iface, buf, total, nexthop);
}

static uint16_t ip_generate_id(void) {
    static mutex_t mutex = MUTEX_INITIALIZER;
    static uint16_t id = 128;
    uint16_t ret;
    mutex_lock(&mutex);
    ret = id++;
    mutex_unlock(&mutex);
    return ret;
}

ssize_t ip_send_packet(uint8_t protocol, const uint8_t *data, size_t len, IPAddress src, IPAddress dst) {
    struct ip_route *route;
    struct IP_INTERFACE *iface;
    char addr[MAX_IP_ADDRESS_STRING_LENGTH];
    IPAddress nexthop;
    uint16_t id;

    if (src == IP_ADDR_ANY && dst == IP_ADDR_BROADCAST) {
        errorf("source address is required for broadcast addresses");
        return -1;
    }
    if (!(route = ip_route_lookup(dst)) || !(iface = route->iface) || (src != IP_ADDR_ANY && src != iface->unicast)) {
        errorf("routing failure");
        return -1;
    }
    nexthop = (route->nexthop != IP_ADDR_ANY) ? route->nexthop : dst;
    if (NETWORK_INTERFACE(iface)->dev->mtu < MIN_IP_HEADER_SIZE + len) {
        errorf("packet size too large");
        return -1;
    }
    id = ip_generate_id();
    if (ip_output_core(iface, protocol, data, len, iface->unicast, dst, nexthop, id, 0) == -1) {
        errorf("ip output failed");
        return -1;
    }
    return len;
}

int ip_register_protocol(const char *name, uint8_t type, void (*handler)(const uint8_t *data, size_t len, IPAddress src, IPAddress dst, struct IP_INTERFACE *iface)) {
    struct ip_protocol *entry;

    for (entry = protocols; entry; entry = entry->next) {
        if (entry->type == type) {
            errorf("protocol already exists");
            return -1;
        }
    }
    if (!(entry = memory_alloc(sizeof(*entry)))) {
        errorf("memory allocation failure");
        return -1;
    }
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->type = type;
    entry->handler = handler;
    entry->next = protocols;
    protocols = entry;
    infof("protocol registered: %s (0x%02x)", entry->name, entry->type);
    return 0;
}

char *ip_get_protocol_name(uint8_t type) {
    struct ip_protocol *entry;

    for (entry = protocols; entry; entry = entry->next) {
        if (entry->type == type) {
            return entry->name;
        }
    }
    return "UNKNOWN";
}

int ip_initialize(void) {
    if (network_protocol_register("IP", NETWORK_PROTOCOL_TYPE_IP, ip_input) == -1) {
        errorf("network protocol registration failure");
        return -1;
    }
    return 0;
}
