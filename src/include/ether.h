#ifndef ETHER_H
#define ETHER_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include "net2.h"

// Length of an Ethernet address
#define ETHER_ADDR_LEN 6

// Maximum length of a string representation of an Ethernet address
#define ETHER_ADDR_STR_LEN 18 

// Size of the Ethernet header
#define ETHER_HDR_SIZE 14

// Minimum size of an Ethernet frame
#define ETHER_FRAME_SIZE_MIN   60

// Maximum size of an Ethernet frame
#define ETHER_FRAME_SIZE_MAX 1514

// Minimum size of the payload in an Ethernet frame
#define ETHER_PAYLOAD_SIZE_MIN (ETHER_FRAME_SIZE_MIN - ETHER_HDR_SIZE)

// Maximum size of the payload in an Ethernet frame
#define ETHER_PAYLOAD_SIZE_MAX (ETHER_FRAME_SIZE_MAX - ETHER_HDR_SIZE)

// Ethernet frame types
#define ETHER_TYPE_IP   0x0800
#define ETHER_TYPE_ARP  0x0806
#define ETHER_TYPE_IPV6 0x86dd

// Special Ethernet addresses
extern const uint8_t ETHER_ADDR_ANY[ETHER_ADDR_LEN];
extern const uint8_t ETHER_ADDR_BROADCAST[ETHER_ADDR_LEN];

// Function to convert a string representation of an Ethernet address to binary
extern int ether_addr_pton(const char *p, uint8_t *n);

// Function to convert a binary Ethernet address to a string representation
extern char *ether_addr_ntop(const uint8_t *n, char *p, size_t size);

// Helper function for transmitting an Ethernet frame
extern int ether_transmit_helper(struct network_device *dev, uint16_t type, const uint8_t *payload, size_t plen, const void *dst, ssize_t (*callback)(struct network_device *dev, const uint8_t *buf, size_t len));

// Helper function for polling an Ethernet device for received frames
extern int ether_poll_helper(struct network_device *dev, ssize_t (*callback)(struct network_device *dev, uint8_t *buf, size_t size));

// Helper function for setting up an Ethernet device
extern void ether_setup_helper(struct network_device *network_device);

// Function to initialize an Ethernet device
extern struct network_device * ether_init(const char *name);

#endif