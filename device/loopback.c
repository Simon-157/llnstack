/**
 * @file loopback.c
 * @brief loopback interface
 * @author Simon Boateng
 * @date 2024-03-25
 */

#include <stdio.h>
#include <stdint.h>

#include "util.h"
#include "net2.h"

#include "loopback.h"

/**
 * @brief Maximum size of IP datagram
 */
#define LOOPBACK_MTU UINT16_MAX


static int loopback_transmit(struct network_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    debugf("dev=%s, type=%s(0x%04x), len=%zu", dev->name, network_protocol_name(type), type, len);
    debugdump(data, len);
    network_input_handler(type, data, len, dev);
    return 0;
}


static struct network_device_operations loopback_ops = {
   
    .transmit = loopback_transmit,
};

static void loopback_setup(struct network_device *dev)
{
    dev->type = NETWORK_DEVICE_TYPE_LOOPBACK; /* loopback interface */
    dev->mtu = LOOPBACK_MTU; /* maximum size of IP datagram */
    dev->header_len = 0; /* non header */
    dev->address_len = 0; /* non address */
    dev->flags = NETWORK_DEVICE_FLAG_LOOPBACK; /* loopback interface */
    dev->ops = &loopback_ops; /* the network device operations */
}


struct network_device *loopback_init(void)
{
    struct network_device *dev;

    /* allocate and initialize the loopback network device */
    dev = network_device_allocate(loopback_setup);
    if (!dev) {
        errorf("net_device_alloc() failure");
        return NULL;
    }
    /* register the loopback network device */
    if (network_device_register(dev) == -1) {
        errorf("net_device_register() failure");
        return NULL;
    }
    debugf("initialized, dev=%s", dev->name);
    return dev;
}

