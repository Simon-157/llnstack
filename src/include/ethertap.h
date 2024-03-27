#ifndef ETHER_TAP_H
#define ETHER_TAP_H

#include "net2.h"

extern struct network_device * ether_tap_init(const char *name, const char *addr);

#endif