#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#include "net2.h"
#include "ip2.h"


#define ARP_RESOLVE_ERROR      -1
#define ARP_RESOLVE_INCOMPLETE  0
#define ARP_RESOLVE_FOUND       1

/**
 * @brief Resolves the hardware address (MAC address) corresponding to the given IP address.
 *
 * This function is used to resolve the hardware address (MAC address) corresponding to the given IP address.
 *
 * @param iface Pointer to the network interface structure.
 * @param pa The IP address to resolve.
 * @param ha Pointer to the buffer where the resolved hardware address will be stored.
 * @return Returns ARP_RESOLVE_ERROR if an error occurred during resolution, ARP_RESOLVE_INCOMPLETE if the resolution is still in progress, or ARP_RESOLVE_FOUND if the resolution was successful.
 */
extern int arp_resolve(struct network_interface *iface, IPAddress pa, uint8_t *ha);

/**
 * @brief Initializes the ARP module.
 *
 * This function initializes the ARP module and performs any necessary setup.
 *
 * @return Returns 0 on success, or a negative value if an error occurred.
 */
extern int arp_init(void);

#endif


