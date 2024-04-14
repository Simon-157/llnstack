#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "handler.h"

#include "util.h"
#include "net2.h"
// #include "ip2.h"
#include "udp.h"

#define UDP_PROTOCOL_CONTROL_BLOCK_SIZE 16

#define UDP_PROTOCOL_CONTROL_BLOCK_STATE_FREE    0
#define UDP_PROTOCOL_CONTROL_BLOCK_STATE_OPEN    1
#define UDP_PROTOCOL_CONTROL_BLOCK_STATE_CLOSE 2

/* see https://tools.ietf.org/html/rfc6335 */
#define UDP_SOURCE_PORT_MIN 49152
#define UDP_SOURCE_PORT_MAX 65535

struct pre_header {
    uint32_t src;
    uint32_t dst;
    uint8_t zero;
    uint8_t protocol;
    uint16_t len;
};

struct UDP_HEADER {
    uint16_t src;
    uint16_t dst;
    uint16_t len;
    uint16_t sum;
};

struct UDP_PROTOCOL_CONTROL_BLOCK {
    int state;
    struct IP_ENDPOINT local;
    struct queue_head queue; 
    struct sched_ctx ctx;
};

struct udp_queue_entry {
    struct IP_ENDPOINT foreign;
    uint16_t len;
};

static mutex_t mutex = MUTEX_INITIALIZER;
static struct UDP_PROTOCOL_CONTROL_BLOCK pcbs[UDP_PROTOCOL_CONTROL_BLOCK_SIZE];

#pragma GCC diagnostic ignored "-Wunused-parameter"
static void udp_dump(const uint8_t *data, size_t len)
{
    struct UDP_HEADER *hdr;

    flockfile(stderr);
    hdr = (struct UDP_HEADER *)data;
    fprintf(stderr, "        src: %u, dst: %u, len: %u, sum: 0x%04x\n",
            ntoh16(hdr->src), ntoh16(hdr->dst), ntoh16(hdr->len), ntoh16(hdr->sum));
#ifdef HEXDUMP
    hexdump(stderr, data, len);
#endif
    funlockfile(stderr);
}



static struct UDP_PROTOCOL_CONTROL_BLOCK * udp_pcb_alloc(void)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;

    for (pcb = pcbs; pcb < tailof(pcbs); pcb++) {
        if (pcb->state == UDP_PROTOCOL_CONTROL_BLOCK_STATE_FREE) {
            pcb->state = UDP_PROTOCOL_CONTROL_BLOCK_STATE_OPEN;
            sched_ctx_init(&pcb->ctx);
            return pcb;
        }
    }
    return NULL;
}

static void udp_pcb_release(struct UDP_PROTOCOL_CONTROL_BLOCK *pcb)
{
    struct queue_entry *entry;

    pcb->state = UDP_PROTOCOL_CONTROL_BLOCK_STATE_CLOSE;
    if (sched_ctx_destroy(&pcb->ctx) == -1) {
        sched_wakeup(&pcb->ctx);
        return;
    }
    pcb->state = UDP_PROTOCOL_CONTROL_BLOCK_STATE_FREE;
    pcb->local.address = IP_ADDR_ANY;
    pcb->local.port = 0;
    while ((entry = queue_pop(&pcb->queue)) != NULL) {
        memory_free(entry);
    }
}

static struct UDP_PROTOCOL_CONTROL_BLOCK * udp_pcb_select(IPAddress addr, uint16_t port)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;

    for (pcb = pcbs; pcb < tailof(pcbs); pcb++) {
        if (pcb->state == UDP_PROTOCOL_CONTROL_BLOCK_STATE_OPEN) {
            if ((pcb->local.address == IP_ADDR_ANY || pcb->local.address == addr) && pcb->local.port == port) {
                return pcb;
            }
        }
    }
    return NULL;
}

static struct UDP_PROTOCOL_CONTROL_BLOCK * udp_pcb_get(int id)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;

    if (id < 0 || id >= (int)countof(pcbs)) {
        return NULL;
    }
    pcb = &pcbs[id];
    if (pcb->state != UDP_PROTOCOL_CONTROL_BLOCK_STATE_OPEN) {
        return NULL;
    }
    return pcb;
}

static int udp_pcb_id(struct UDP_PROTOCOL_CONTROL_BLOCK *pcb)
{
    return indexof(pcbs, pcb);
}

static void udp_input(const uint8_t *data, size_t len, IPAddress src, IPAddress dst, struct IP_INTERFACE *iface)
{
    struct pre_header pseudo;
    uint16_t psum = 0;
    struct UDP_HEADER *hdr;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[MAX_IP_ADDRESS_STRING_LENGTH];
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;
    struct udp_queue_entry *entry;

    if (len < sizeof(*hdr)) {
        errorf("too short");
        return;
    }
    hdr = (struct UDP_HEADER *)data;
    if (len != ntoh16(hdr->len)) {
        errorf("length error: len=%zu, hdr->len=%u", len, ntoh16(hdr->len));
        return;
    }
    pseudo.src = src;
    pseudo.dst = dst;
    pseudo.zero = 0;
    pseudo.protocol = UDP_PROTOCOL;
    pseudo.len = hton16(len);
    psum = ~cksum16((uint16_t *)&pseudo, sizeof(pseudo), 0);
    if (cksum16((uint16_t *)hdr, len, psum) != 0) {
        errorf("checksum error: sum=0x%04x, verify=0x%04x", ntoh16(hdr->sum), ntoh16(cksum16((uint16_t *)hdr, len, -hdr->sum + psum)));
        return;
    }
    debugf("%s:%d => %s:%d, len=%zu (payload=%zu)",
        ip_address_to_string(src, addr1, sizeof(addr1)), ntoh16(hdr->src),
        ip_address_to_string(dst, addr2, sizeof(addr2)), ntoh16(hdr->dst),
        len, len - sizeof(*hdr));
    udp_dump(data, len);
    mutex_lock(&mutex);
    pcb = udp_pcb_select(dst, hdr->dst);
    if (!pcb) {
      
        mutex_unlock(&mutex);
        return;
    }
    entry = memory_alloc(sizeof(*entry) + (len - sizeof(*hdr)));
    if (!entry) {
        mutex_unlock(&mutex);
        errorf("memory allocation failure");
        return;
    }
    entry->foreign.address = src;
    entry->foreign.port = hdr->src;
    entry->len = len - sizeof(*hdr);
    memcpy(entry + 1, hdr + 1, entry->len);
    if (!queue_push(&pcb->queue, entry)) {
        mutex_unlock(&mutex);
        errorf("push queue failure");
        return;
    }
    sched_wakeup(&pcb->ctx);
    mutex_unlock(&mutex);
}

ssize_t send_udp_packet_to_network(struct IP_ENDPOINT *src, struct IP_ENDPOINT *dst, const  uint8_t *data, size_t len)
{
    uint8_t buf[MAX_IP_PAYLOAD_SIZE];
    struct UDP_HEADER *hdr;
    struct pre_header pseudo;
    uint16_t total, psum = 0;
    char ep1[MAX_IP_ENDPOINT_STRING_LENGTH];
    char ep2[MAX_IP_ENDPOINT_STRING_LENGTH];

    if (len > MAX_IP_PAYLOAD_SIZE - sizeof(*hdr)) {
        errorf("too long");
        return -1;
    }
    hdr = (struct UDP_HEADER *)buf;
    hdr->src = src->port;
    hdr->dst = dst->port;
    total = sizeof(*hdr) + len;
    hdr->len = hton16(total);
    hdr->sum = 0;
    memcpy(hdr + 1, data, len);
    pseudo.src = src->address;
    pseudo.dst = dst->address;
    pseudo.zero = 0;
    pseudo.protocol = UDP_PROTOCOL;
    pseudo.len = hton16(total);
    psum = ~cksum16((uint16_t *)&pseudo, sizeof(pseudo), 0);
    hdr->sum = cksum16((uint16_t *)hdr, total, psum);
    debugf("%s => %s, len=%u (payload=%zu)",
        ip_endpoint_to_string(src, ep1, sizeof(ep1)), ip_endpoint_to_string(dst, ep2, sizeof(ep2)), total, len);
    udp_dump((uint8_t *)hdr, total);
    if (ip_send_packet(UDP_PROTOCOL, (uint8_t *)hdr, total, src->address, dst->address) == -1) {
        errorf("ip_output() failure");
        return -1;
    }
    return len;
}

static void event_handler(void *arg)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;

    mutex_lock(&mutex);
    for (pcb = pcbs; pcb < tailof(pcbs); pcb++) {
        if (pcb->state == UDP_PROTOCOL_CONTROL_BLOCK_STATE_OPEN) {
            sched_interrupt(&pcb->ctx);
        }
    }
    mutex_unlock(&mutex);
}

int initialize_udp_subsystem(void)
{
    if (ip_register_protocol("UDP", UDP_PROTOCOL, udp_input) == -1) {
        errorf("ip register protocol failed");
        return -1;
    }
    network_event_subscribe(event_handler, NULL);
    return 0;
}



int open_new_udp_socket(void)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;
    int id;

    mutex_lock(&mutex);
    pcb = udp_pcb_alloc();
    if (!pcb) {
        errorf("udp allocation failed");
        mutex_unlock(&mutex);
        return -1;
    }
    id = udp_pcb_id(pcb);
    mutex_unlock(&mutex);
    return id;
}

int close_udp_socket(int id)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;

    mutex_lock(&mutex);
    pcb = udp_pcb_get(id);
    if (!pcb) {
        errorf("pcb not found, id=%d", id);
        mutex_unlock(&mutex);
        return -1;
    }
    udp_pcb_release(pcb);
    mutex_unlock(&mutex);
    return 0;
}

int bind_udp_socket_to_local_endpoint(int id, struct IP_ENDPOINT *local)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb, *exist;
    char ep1[MAX_IP_ENDPOINT_STRING_LENGTH];
    char ep2[MAX_IP_ENDPOINT_STRING_LENGTH];

    mutex_lock(&mutex);
    pcb = udp_pcb_get(id);
    if (!pcb) {
        errorf("pcb not found, id=%d", id);
        mutex_unlock(&mutex);
        return -1;
    }
    exist = udp_pcb_select(local->address, local->port);
    if (exist) {
        errorf("already in use, id=%d, want=%s, exist=%s",
            id, ip_endpoint_to_string(local, ep1, sizeof(ep1)), ip_endpoint_to_string(&exist->local, ep2, sizeof(ep2)));
        mutex_unlock(&mutex);
        return -1;
    }
    pcb->local = *local;
    debugf("bound, id=%d, local=%s", id, ip_endpoint_to_string(&pcb->local, ep1, sizeof(ep1)));
    mutex_unlock(&mutex);
    return 0;
}

ssize_t send_udp_packet_over_socket(int id, uint8_t *data, size_t len, struct IP_ENDPOINT *foreign)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;
    struct IP_ENDPOINT local;
    struct IP_INTERFACE *iface;
    char addr[MAX_IP_ADDRESS_STRING_LENGTH];
    uint32_t p;

    mutex_lock(&mutex);
    pcb = udp_pcb_get(id);
    if (!pcb) {
        errorf("pcb not found, id=%d", id);
        mutex_unlock(&mutex);
        return -1;
    }
    local.address = pcb->local.address;
    if (local.address == IP_ADDR_ANY) {
        iface = ip_get_interface(foreign->address);
        if (!iface) {
            errorf("iface not found that can reach foreign address, addr=%s",
                ip_address_to_string(foreign->address, addr, sizeof(addr)));
            mutex_unlock(&mutex);
            return -1;
        }
        local.address = iface->unicast;
        debugf("select local address, addr=%s", ip_address_to_string(local.address, addr, sizeof(addr)));
    }
    if (!pcb->local.port) {
        for (p = UDP_SOURCE_PORT_MIN; p <= UDP_SOURCE_PORT_MAX; p++) {
            if (!udp_pcb_select(local.address, hton16(p))) {
                pcb->local.port = hton16(p);
                debugf("dynamic assign local port, port=%d", p);
                break;
            }
        }
        if (!pcb->local.port) {
            debugf("failed to dynamic assign local port, addr=%s", ip_address_to_string(local.address, addr, sizeof(addr)));
            mutex_unlock(&mutex);
            return -1;
        }
    }
    local.port = pcb->local.port;
    mutex_unlock(&mutex);
    return send_udp_packet_to_network(&local, foreign, data, len);
}

ssize_t receive_udp_packet_from_socket(int id, uint8_t *buf, size_t size, struct IP_ENDPOINT *foreign)
{
    struct UDP_PROTOCOL_CONTROL_BLOCK *pcb;
    struct udp_queue_entry *entry;
    ssize_t len;

    mutex_lock(&mutex);
    pcb = udp_pcb_get(id);
    if (!pcb) {
        errorf("pcb not found, id=%d", id);
        mutex_unlock(&mutex);
        return -1;
    }
    while (!(entry = queue_pop(&pcb->queue))) {
        if (sched_sleep(&pcb->ctx, &mutex, NULL) == -1) {
            debugf("interrupted");
            mutex_unlock(&mutex);
            errno = EINTR;
            return -1;
        }
        if (pcb->state == UDP_PROTOCOL_CONTROL_BLOCK_STATE_CLOSE) {
            debugf("closed");
            udp_pcb_release(pcb);
            mutex_unlock(&mutex);
            return -1;
        }
    }
    mutex_unlock(&mutex);
    if (foreign) {
        *foreign = entry->foreign;
    }
    len = MIN(size, entry->len);
    memcpy(buf, entry + 1, len);
    memory_free(entry);
    return len;
}