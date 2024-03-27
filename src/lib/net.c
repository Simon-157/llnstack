#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "handler.h"

#include "util.h"
#include "net2.h"

#include "arp.h"
#include "ip2.h"
#include "icmp.h"
#include "udp.h"



struct network_protocol {
    struct network_protocol *next;
    char name[16];
    uint16_t type;
    struct queue_head queue; /* input queue */
    void (*handler)(const uint8_t *data, size_t len, struct network_device *dev);
};

/* NOTE: the data follows immediately after the structure */
struct network_protocol_queue_entry {
    struct network_device *dev;
    size_t len;
};

struct network_timer {
    struct network_timer *next;
    char name[16];
    struct timeval interval;
    struct timeval last;
    void (*handler)(void);
};

struct network_event {
    struct network_event *next;
    void (*handler)(void *arg);
    void *arg;
};

/* NOTE: if you want to add/delete the entries after network_run(), you need to protect these lists with a mutex. */
static struct network_device *devices;
static struct network_protocol *protocols;
static struct network_timer *timers;
static struct network_event *events;

struct network_device *network_device_allocate(void (*setup)(struct network_device *dev))
{
    struct network_device *dev;

    dev = memory_alloc(sizeof(*dev));
    if (!dev) {
        errorf("memory_alloc() failure");
        return NULL;
    }
    if (setup) {
        setup(dev);
    }
    return dev;
}

/* NOTE: must not be call after network_run() */
int network_device_register(struct network_device *dev)
{
    static unsigned int index = 0;

    dev->index = index++;
    snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);
    dev->next = devices;
    devices = dev;
    infof("registered, dev=%s, type=0x%04x", dev->name, dev->type);
    return 0;
}

static int network_device_open(struct network_device *dev)
{
    if (NETWORK_DEVICE_IS_UP(dev)) {
        errorf("already opened, dev=%s", dev->name);
        return -1;
    }
    if (dev->ops->open) {
        if (dev->ops->open(dev) == -1) {
            errorf("failure, dev=%s", dev->name);
            return -1;
        }
    }
    dev->flags |= NETWORK_DEVICE_FLAG_UP;
    infof("dev=%s, state=%s", dev->name, NETWORK_DEVICE_STATE(dev));
    return 0;
}

static int network_device_close(struct network_device *dev)
{
    if (!NETWORK_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return -1;
    }
    if (dev->ops->close) {
        if (dev->ops->close(dev) == -1) {
            errorf("failure, dev=%s", dev->name);
            return -1;
        }
    }
    dev->flags &= ~NETWORK_DEVICE_FLAG_UP;
    infof("dev=%s, state=%s", dev->name, NETWORK_DEVICE_STATE(dev));
    return 0;
}

/* NOTE: must not be call after network_run() */
int network_device_add_interface (struct network_device *dev, struct network_interface *iface)
{
    struct network_interface *entry;

    for (entry = dev->interfaces; entry; entry = entry->next) {
        if (entry->family == iface->family) {
            errorf("already exists, dev=%s, family=%d", dev->name, entry->family);
            return -1;
        }
    }
    iface->next = dev->interfaces;
    iface->dev = dev;
    dev->interfaces = iface;
    return 0;
}

struct network_interface *network_device_get_interface(struct network_device *dev, int family)
{
    struct network_interface *entry;

    for (entry = dev->interfaces; entry; entry = entry->next) {
        if (entry->family == family) {
            break;
        }
    }
    return entry;
}

int network_device_output(struct network_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    if (!NETWORK_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return -1;
    }
    if (len > dev->mtu) {
        errorf("too long, dev=%s, mtu=%u, len=%zu", dev->name, dev->mtu, len);
        return -1;
    }
    debugf("dev=%s, type=%s(0x%04x), len=%zu", dev->name, network_protocol_name(type), type, len);
    debugdump(data, len);
    if (dev->ops->transmit(dev, type, data, len, dst) == -1) {
        errorf("device transmit failure, dev=%s, len=%zu", dev->name, len);
        return -1;
    }
    return 0;
}

int network_input_handler(uint16_t type, const uint8_t *data, size_t len, struct network_device *dev)
{
    struct network_protocol *proto;
    struct network_protocol_queue_entry *entry;

    for (proto = protocols; proto; proto = proto->next) {
        if (proto->type == type) {
            entry = memory_alloc(sizeof(*entry) + len);
            if (!entry) {
                errorf("memory_alloc() failure");
                return -1;
            }
            entry->dev = dev;
            entry->len = len;
            memcpy(entry+1, data, len);
            if (!queue_push(&proto->queue, entry)) {
                errorf("queue_push() failure");
                memory_free(entry);
                return -1;
            }
            debugf("queue pushed (num:%u), dev=%s, type=%s(0x%04x), len=%zd", proto->queue.num, dev->name, proto->name, type, len);
            debugdump(data, len);
            raise_softirq();
            return 0;
        }
    }
    /* unsupported protocol */
    return 0;
}

/* NOTE: must not be call after network_run() */
int network_protocol_register(const char *name, uint16_t type, void (*handler)(const uint8_t *data, size_t len, struct network_device *dev))
{
    struct network_protocol *proto;

    for (proto = protocols; proto; proto = proto->next) {
        if (type == proto->type) {
            errorf("already registered, type=%s(0x%04x), exist=%s(0x%04x)", name, type, proto->name, proto->type);
            return -1;
        }
    }
    proto = memory_alloc(sizeof(*proto));
    if (!proto) {
        errorf("memory_alloc() failure");
        return -1;
    }
    strncpy(proto->name, name, sizeof(proto->name)-1);
    proto->type = type;
    proto->handler = handler;
    proto->next = protocols;
    protocols = proto;
    infof("registered, type=%s(0x%04x)", proto->name, type);
    return 0;
}

char *network_protocol_name(uint16_t type)
{
    struct network_protocol *entry;

    for (entry = protocols; entry; entry = entry->next) {
        if (entry->type == type) {
            return entry->name;
        }
    }
    return "UNKNOWN";
}

int network_protocol_handler(void)
{
    struct network_protocol *proto;
    struct network_protocol_queue_entry *entry;
    unsigned int num;

    for (proto = protocols; proto; proto = proto->next) {
        while (1) {
            entry = queue_pop(&proto->queue);
            if (!entry) {
                break;
            }
            num = proto->queue.num;
            debugf("queue popped (num:%u), dev=%s, type=0x%04x, len=%zd", num, entry->dev->name, proto->type, entry->len);
            debugdump((uint8_t *)(entry+1), entry->len);
            proto->handler((uint8_t *)(entry+1), entry->len, entry->dev);
            free(entry);
        }
    }
    return 0;
}

/* NOTE: must not be call after network_run() */
int network_timer_register(const char *name, struct timeval interval, void (*handler)(void))
{
    struct network_timer *timer;

    timer = memory_alloc(sizeof(*timer));
    if (!timer) {
        errorf("memory_alloc() failure");
        return -1;
    }
    strncpy(timer->name, name, sizeof(timer->name)-1);
    timer->interval = interval;
    gettimeofday(&timer->last, NULL);
    timer->handler = handler;
    timer->next = timers;
    timers = timer;
    infof("registered: %s interval={%ld, %ld}", timer->name, interval.tv_sec, interval.tv_usec);
    return 0;
}

int network_timer_handler(void)
{
    struct network_timer *timer;
    struct timeval now, diff;

    for (timer = timers; timer; timer = timer->next) {
        gettimeofday(&now, NULL);
        timersub(&now, &timer->last, &diff);
        if (timercmp(&timer->interval, &diff, <) != 0) { /* true (!0) or false (0) */
            timer->handler();
            timer->last = now;
        }
    }
    return 0;
}

int network_interrupt(void)
{
    /* getpid(2) and kill(2) are signal safety functions. see signal-safety(7). */
    return kill(getpid(), SIGUSR2);
}

/* NOTE: must not be call after network_run() */
int network_event_subscribe(void (*handler)(void *arg), void *arg)
{
    struct network_event *event;

    event = memory_alloc(sizeof(*event));
    if (!event) {
        errorf("memory_alloc() failure");
        return -1;
    }
    event->handler = handler;
    event->arg = arg;
    event->next = events;
    events = event;
    return 0;
}

int network_event_handler(void)
{
    struct network_event *event;

    for (event = events; event; event = event->next) {
        event->handler(event->arg);
    }
    return 0;
}

int network_run(void)
{
    struct network_device *dev;

    if (intr_run() == -1) {
        errorf("intr_run() failure");
        return -1;
    }
    debugf("open all devices...");
    for (dev = devices; dev; dev = dev->next) {
        network_device_open(dev);
    }
    debugf("running...");
    return 0;
}

void network_shutdown(void)
{
    struct network_device *dev;

    debugf("close all devices...");
    for (dev = devices; dev; dev = dev->next) {
        network_device_close(dev);
    }
    debugf("shutdown");
}


int network_init(void)
{
    if (intr_init() == -1) {
        errorf("intr_init() failure");
        return -1;
    }
    if (arp_init() == -1) {
        errorf("arp_init() failure");
        return -1;
    }
    if (ip_initialize() == -1) {
        errorf("ip_init() failure");
        return -1;
    }
    if (icmp_init() == -1) {
        errorf("icmp_init() failure");
        return -1;
    }
    if (udp_init() == -1) {
        errorf("udp_init() failure");
        return -1;
    }
    // if (tcp_init() == -1) {
    //     errorf("tcp_init() failure");
    //     return -1;
    // }
    infof("initialized");
    return 0;
}