#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "handler.h"

#include "util.h"
// #include "net2.h"
#include "ether.h"
#include "arp.h"
// #include "ip2.h"

/* see https://www.iana.org/assignments/arp-parameters/arp-parameters.txt */
#define ARP_HRD_ETHER 0x0001
/* NOTE: use same value as the Ethernet types */
#define ARP_PRO_IP ETHER_TYPE_IP

#define ARP_OP_REQUEST 0x0001
#define ARP_OP_REPLY   0x0002

#define ARP_CACHE_SIZE 32
#define ARP_CACHE_TIMEOUT 30 /* seconds */

#define ARP_CACHE_STATE_FREE       0
#define ARP_CACHE_STATE_INCOMPLETE 1
#define ARP_CACHE_STATE_RESOLVED   2
#define ARP_CACHE_STATE_STATIC     3

struct AddressResolutionProtocolHeader {
    uint16_t hrd;
    uint16_t pro;
    uint8_t hln;
    uint8_t pln;
    uint16_t op;
};

struct arp_ether {
    struct AddressResolutionProtocolHeader hdr;
    uint8_t sha[ETHER_ADDR_LEN];
    uint8_t spa[IP_ADDRESS_LENGTH];
    uint8_t tha[ETHER_ADDR_LEN];
    uint8_t tpa[IP_ADDRESS_LENGTH];
};

struct arp_cache {
    unsigned char state;
    IPAddress pa;
    uint8_t ha[ETHER_ADDR_LEN];
    struct timeval timestamp;
};

static mutex_t mutex = MUTEX_INITIALIZER;
static struct arp_cache caches[ARP_CACHE_SIZE];

static char *
arp_opcode_ntoa(uint16_t opcode)
{
    switch (ntoh16(opcode)) {
    case ARP_OP_REQUEST:
        return "Request";
    case ARP_OP_REPLY:
        return "Reply";
    }
    return "Unknown";
}


// Function to display ARP message details with custom colors and table structure
static void arp_dump(const uint8_t *data, size_t len)
{
    struct arp_ether *message;
    IPAddress spa, tpa;
    char addr[128];

    message = (struct arp_ether *)data;
    flockfile(stderr);
    fprintf(stderr, "\033[1;34m================== ARP Dump ==================\033[0m\n");
    fprintf(stderr, "\033[1;36m|   Field   |   Value   |\033[0m\n");
    fprintf(stderr, "\033[1;34m==============================================\033[0m\n");
    fprintf(stderr, "\033[1;36m|   Hardware Type     |   0x%04x  |\033[0m\n", ntoh16(message->hdr.hrd));
    fprintf(stderr, "\033[1;36m|   Protocol Type      |   0x%04x  |\033[0m\n", ntoh16(message->hdr.pro));
    fprintf(stderr, "\033[1;36m|   Hardware Address Length     |   %u       |\033[0m\n", message->hdr.hln);
    fprintf(stderr, "\033[1;36m|   Protocol Address Length     |   %u       |\033[0m\n", message->hdr.pln);
    fprintf(stderr, "\033[1;36m|   Operation Code      |   0x%04x  |   (\033[1;32m%s\033[0m)\033[0m\n", ntoh16(message->hdr.op), arp_opcode_ntoa(message->hdr.op));
    fprintf(stderr, "\033[1;36m|   Sender Mac Addr     |   %s   |\033[0m\n", ether_addr_ntop(message->sha, addr, sizeof(addr)));
    memcpy(&spa, message->spa, sizeof(spa));
    fprintf(stderr, "\033[1;36m|   Sender IP Addr     |   %s   |\033[0m\n", ip_address_to_string(spa, addr, sizeof(addr)));
    fprintf(stderr, "\033[1;36m|   Target Mac Addr     |   %s   |\033[0m\n", ether_addr_ntop(message->tha, addr, sizeof(addr)));
    memcpy(&tpa, message->tpa, sizeof(tpa));
    fprintf(stderr, "\033[1;36m|   Target IP Addr     |   %s   |\033[0m\n", ip_address_to_string(tpa, addr, sizeof(addr)));
    fprintf(stderr, "\033[1;34m==============================================\033[0m\n");
#ifdef HEXDUMP
    hexdump(stderr, data, len);
#endif
    funlockfile(stderr);
}



static struct arp_cache * arp_cache_alloc(void)
{
    struct arp_cache *entry, *oldest = NULL;

    for (entry = caches; entry < tailof(caches); entry++) {
        if (entry->state == ARP_CACHE_STATE_FREE) {
            return entry;
        }
        if (!oldest || timercmp(&oldest->timestamp, &entry->timestamp, >)) {
            oldest = entry;
        }
    }
    return oldest;
}

static struct arp_cache * arp_cache_select(IPAddress pa)
{
    struct arp_cache *entry;

    for (entry = caches; entry < tailof(caches); entry++) {
        if (entry->state != ARP_CACHE_STATE_FREE && entry->pa == pa) {
            return entry;
        }
    }
    return NULL;
}

static struct arp_cache * arp_cache_update(IPAddress pa, const uint8_t *ha)
{
    struct arp_cache *cache;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[ETHER_ADDR_STR_LEN];

    cache = arp_cache_select(pa);
    if (!cache) {
        /* not found */
        return NULL;
    }
    cache->state = ARP_CACHE_STATE_RESOLVED;
    memcpy(cache->ha, ha, ETHER_ADDR_LEN);
    gettimeofday(&cache->timestamp, NULL);
    debugf("UPDATE: pa=%s, ha=%s", ip_address_to_string(pa, addr1, sizeof(addr1)), ether_addr_ntop(ha, addr2, sizeof(addr2)));
    return cache;
}

static struct arp_cache * arp_cache_insert(IPAddress pa, const uint8_t *ha)
{
    struct arp_cache *cache;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[ETHER_ADDR_STR_LEN];

    cache = arp_cache_alloc();
    if (!cache) {
        errorf("arp_cache_alloc() failure");
        return NULL;
    }
    cache->state = ARP_CACHE_STATE_RESOLVED;
    cache->pa = pa;
    memcpy(cache->ha, ha, ETHER_ADDR_LEN);
    gettimeofday(&cache->timestamp, NULL);
    debugf("INSERT: pa=%s, ha=%s", ip_address_to_string(pa, addr1, sizeof(addr1)), ether_addr_ntop(ha, addr2, sizeof(addr2)));
    return cache;
}

static void arp_cache_delete(struct arp_cache *cache)
{
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[ETHER_ADDR_STR_LEN];

    debugf("DELETE: pa=%s, ha=%s", ip_address_to_string(cache->pa, addr1, sizeof(addr1)), ether_addr_ntop(cache->ha, addr2, sizeof(addr2)));
    cache->state = ARP_CACHE_STATE_FREE;
    cache->pa = 0;
    memset(cache->ha, 0, ETHER_ADDR_LEN);
    timerclear(&cache->timestamp);
}

static int arp_request(struct network_interface *iface, IPAddress tpa)
{
    struct arp_ether request;

    request.hdr.hrd = hton16(ARP_HRD_ETHER);
    request.hdr.pro = hton16(ARP_PRO_IP);
    request.hdr.hln = ETHER_ADDR_LEN;
    request.hdr.pln = IP_ADDRESS_LENGTH;
    request.hdr.op = hton16(ARP_OP_REQUEST);
    memcpy(request.sha, iface->dev->address, ETHER_ADDR_LEN);
    memcpy(request.spa, &((struct IP_INTERFACE *)iface)->unicast, IP_ADDRESS_LENGTH);
    memset(request.tha, 0, ETHER_ADDR_LEN);
    memcpy(request.tpa, &tpa, IP_ADDRESS_LENGTH);
    debugf("Sending ARP request: dev=%s, opcode=%s(0x%04x), len=%zu", iface->dev->name, arp_opcode_ntoa(request.hdr.op), ntoh16(request.hdr.op), sizeof(request));

    arp_dump((uint8_t *)&request, sizeof(request));
    return network_device_output(iface->dev, ETHER_TYPE_ARP, (uint8_t *)&request, sizeof(request), iface->dev->broadcast);
}

static int arp_reply(struct network_interface *iface, const uint8_t *tha, IPAddress tpa, const uint8_t *dst)
{
    struct arp_ether reply;

    reply.hdr.hrd = hton16(ARP_HRD_ETHER);
    reply.hdr.pro = hton16(ARP_PRO_IP);
    reply.hdr.hln = ETHER_ADDR_LEN;
    reply.hdr.pln = IP_ADDRESS_LENGTH;
    reply.hdr.op = hton16(ARP_OP_REPLY);
    memcpy(reply.sha, iface->dev->address, ETHER_ADDR_LEN);
    memcpy(reply.spa, &((struct IP_INTERFACE *)iface)->unicast, IP_ADDRESS_LENGTH);
    memcpy(reply.tha, tha, ETHER_ADDR_LEN);
    memcpy(reply.tpa, &tpa, IP_ADDRESS_LENGTH);
     debugf("Sending ARP reply: dev=%s, opcode=%s(0x%04x), len=%zu", iface->dev->name, arp_opcode_ntoa(reply.hdr.op), ntoh16(reply.hdr.op), sizeof(reply));
    arp_dump((uint8_t *)&reply, sizeof(reply));
    return network_device_output(iface->dev, ETHER_TYPE_ARP, (uint8_t *)&reply, sizeof(reply), dst);
}

static void arp_input(const uint8_t *data, size_t len, struct network_device *dev)
{
    struct arp_ether *msg;
    IPAddress spa, tpa;
    int merge = 0;
    struct network_interface *iface;

    if (len < sizeof(*msg)) {
                errorf("Received ARP packet is too short");

        return;
    }
    msg = (struct arp_ether *)data;
    if (ntoh16(msg->hdr.hrd) != ARP_HRD_ETHER || msg->hdr.hln != ETHER_ADDR_LEN) {
                errorf("Unsupported hardware address type");

        return;
    }
    if (ntoh16(msg->hdr.pro) != ARP_PRO_IP || msg->hdr.pln != IP_ADDRESS_LENGTH) {
                errorf("Unsupported protocol address type");
        return;
    }
        debugf("Received ARP packet: dev=%s, opcode=%s(0x%04x), len=%zu", dev->name, arp_opcode_ntoa(msg->hdr.op), ntoh16(msg->hdr.op), len);

    arp_dump(data, len);
    memcpy(&spa, msg->spa, sizeof(spa));
    memcpy(&tpa, msg->tpa, sizeof(tpa));
    mutex_lock(&mutex);
    if (arp_cache_update(spa, msg->sha)) {
        /* updated */
        merge = 1;
    }
    mutex_unlock(&mutex);
    iface = network_device_get_interface(dev, NETWORK_INTERFACE_FAMILY_IP);
    if (iface && ((struct IP_INTERFACE *)iface)->unicast == tpa) {
        if (!merge) {
            mutex_lock(&mutex);
            arp_cache_insert(spa, msg->sha);
            mutex_unlock(&mutex);
        }
        if (ntoh16(msg->hdr.op) == ARP_OP_REQUEST) {
            arp_reply(iface, msg->sha, spa, msg->sha);
        }
    }
}

int arp_resolve(struct network_interface *iface, IPAddress pa, uint8_t *ha)
{
    struct arp_cache *cache;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[ETHER_ADDR_STR_LEN];

    if (iface->dev->type != NETWORK_DEVICE_TYPE_ETHERNET) {
        debugf("unsupported hardware address type");
        return ARP_RESOLVE_ERROR;
    }
    if (iface->family != NETWORK_INTERFACE_FAMILY_IP) {
        debugf("unsupported protocol address type");
        return ARP_RESOLVE_ERROR;
    }
    mutex_lock(&mutex);
    cache = arp_cache_select(pa);
    if (!cache) {
        cache = arp_cache_alloc();
        if (!cache) {

            mutex_unlock(&mutex);
            errorf("Failed to allocate ARP cache");

            return ARP_RESOLVE_ERROR;
        }
        cache->state = ARP_CACHE_STATE_INCOMPLETE;
        cache->pa = pa;
        gettimeofday(&cache->timestamp, NULL);
        arp_request(iface, pa);
        mutex_unlock(&mutex);
        debugf("ARP cache not found, protocol_addr=%s", ip_address_to_string(pa, addr1, sizeof(addr1)));

        return ARP_RESOLVE_INCOMPLETE;
    }
    if (cache->state == ARP_CACHE_STATE_INCOMPLETE) {
        arp_request(iface, pa); /* just in case packet loss */
        mutex_unlock(&mutex);
        return ARP_RESOLVE_INCOMPLETE;
    }
    memcpy(ha, cache->ha, ETHER_ADDR_LEN);
    mutex_unlock(&mutex);
    debugf("ARP resolved: protocol_addr=%s, hardware_addr=%s",
        ip_address_to_string(pa, addr1, sizeof(addr1)), ether_addr_ntop(ha, addr2, sizeof(addr2)));

    return ARP_RESOLVE_FOUND;
}

static void arp_timer(void)
{
    struct arp_cache *entry;
    struct timeval now, diff;

    mutex_lock(&mutex);
    gettimeofday(&now, NULL);
    for (entry = caches; entry < tailof(caches); entry++) {
        if (entry->state != ARP_CACHE_STATE_FREE && entry->state != ARP_CACHE_STATE_STATIC) {
            timersub(&now, &entry->timestamp, &diff);
            if (diff.tv_sec > ARP_CACHE_TIMEOUT) {
                arp_cache_delete(entry);
            }
        }
    }
    mutex_unlock(&mutex);
}

int arp_init(void)
{
    struct timeval interval = {1, 0};

    if (network_protocol_register("ARP", NETWORK_PROTOCOL_TYPE_ARP, arp_input) == -1) {
        errorf("network protocol register failure");
        return -1;
    }
    if (network_timer_register("ARP Timer", interval, arp_timer) == -1) {
        errorf("network timer register failure");
        return -1;
    }
    return 0;
}