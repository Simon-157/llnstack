/**
 * @file ip.h
 * @brief Header file for IP (Internet Protocol) related functions and structures.
 *
 * This file defines constants, data structures, and function prototypes related to IP.
 * It provides functions for IP address manipulation, endpoint conversion, routing, and protocol handling.
 * IP version 4 (IPv4) is supported.
 *
 * @see net.h
 */

#ifndef IP_HEADER
#define IP_HEADER

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "net2.h"

#define IPV4 4 /**< IP version 4 */

#define MIN_IP_HEADER_SIZE 20 /**< Minimum IP header size in bytes */
#define MAX_IP_HEADER_SIZE 60 /**< Maximum IP header size in bytes */

#define MAX_IP_PACKET_SIZE UINT16_MAX                                 /**< Maximum total IP packet size */
#define MAX_IP_PAYLOAD_SIZE (MAX_IP_PACKET_SIZE - MIN_IP_HEADER_SIZE) /**< Maximum IP payload size */

#define IP_ADDRESS_LENGTH 4             /**< Length of an IP address in bytes */
#define MAX_IP_ADDRESS_STRING_LENGTH 16 /**< Maximum length of an IP address string representation */

#define MAX_IP_ENDPOINT_STRING_LENGTH (MAX_IP_ADDRESS_STRING_LENGTH + 6) /**< Maximum length of an IP endpoint string representation */

#define ICMP_PROTOCOL 0x01 /**< IP protocol number for ICMP = 1 */
#define TCP_PROTOCOL 0x06  /**< IP protocol number for TCP = 6 */
#define UDP_PROTOCOL 0x11  /**< IP protocol number for UDP = 17 */

typedef uint32_t IPAddress; /**< Type definition for IP address */

/**
 * @struct IP_ENDPOINT
 * @brief Structure representing an IP endpoint (address and port).
 */
struct IP_ENDPOINT
{
    IPAddress address; /**< IP address */
    uint16_t port;     /**< Port number */
};

/**
 * @struct IP_INTERFACE
 * @brief Structure representing an IP interface.
 */
struct IP_INTERFACE
{
    struct network_interface iface; /**< Network interface */
    struct IP_INTERFACE *next;      /**< Pointer to the next IP interface */
    IPAddress unicast;              /**< Unicast IP address */
    IPAddress netmask;              /**< Network mask */
    IPAddress broadcast;            /**< Broadcast IP address */
};


extern const IPAddress IP_ADDR_ANY ;       /**< Constant representing any IP address */
extern const IPAddress IP_BROADCAST; /**< Constant representing the broadcast IP address */

/**
 * @brief Converts a string representation of an IP address to its binary form.
 *
 * @param str Pointer to the string representation of the IP address.
 * @param addr Pointer to the variable where the binary IP address will be stored.
 * @return 0 on success, -1 on failure.
 */
extern int ip_string_to_address(const char *str, IPAddress *addr);

/**
 * @brief Converts a binary IP address to its string representation.
 *
 * @param addr Binary IP address.
 * @param str Pointer to the buffer where the string representation will be stored.
 * @param size Size of the buffer.
 * @return Pointer to the buffer containing the string representation.
 */
extern char *ip_address_to_string(const IPAddress addr, char *str, size_t size);

/**
 * @brief Converts a string representation of an IP endpoint to its binary form.
 *
 * @param str Pointer to the string representation of the IP endpoint.
 * @param endpoint Pointer to the structure where the binary IP endpoint will be stored.
 * @return 0 on success, -1 on failure.
 */
extern int ip_string_to_endpoint(const char *str, struct IP_ENDPOINT *endpoint);

/**
 * @brief Converts a binary IP endpoint to its string representation.
 *
 * @param endpoint Binary IP endpoint.
 * @param str Pointer to the buffer where the string representation will be stored.
 * @param size Size of the buffer.
 * @return Pointer to the buffer containing the string representation.
 */
extern char *ip_endpoint_to_string(const struct IP_ENDPOINT *endpoint, char *str, size_t size);

/**
 * @brief Sets the default gateway for an IP interface.
 *
 * @param iface Pointer to the IP interface.
 * @param gateway String representation of the default gateway.
 * @return 0 on success, -1 on failure.
 */
extern int ip_set_default_gateway(struct IP_INTERFACE *iface, const char *gateway);

/**
 * @brief Retrieves the IP interface for a given destination IP address.
 *
 * @param dst Destination IP address.
 * @return Pointer to the IP interface, or NULL if not found.
 */
extern struct IP_INTERFACE *ip_get_interface(IPAddress dst);

/**
 * @brief Allocates and initializes an IP interface.
 *
 * @param addr String representation of the IP address.
 * @param netmask String representation of the network mask.
 * @return Pointer to the allocated IP interface, or NULL on failure.
 */
extern struct IP_INTERFACE *ip_allocate_interface(const char *addr, const char *netmask);

/**
 * @brief Registers an IP interface with a network device.
 *
 * @param dev Pointer to the network device.
 * @param iface Pointer to the IP interface.
 * @return 0 on success, -1 on failure.
 */
extern int ip_register_interface(struct network_device *dev, struct IP_INTERFACE *iface);

/**
 * @brief Selects an IP interface based on the given IP address.
 *
 * @param addr IP address.
 * @return Pointer to the selected IP interface, or NULL if not found.
 */
extern struct IP_INTERFACE *ip_select_interface(IPAddress addr);

/**
 * @brief Sends an IP packet.
 *
 * @param protocol IP protocol number.
 * @param data Pointer to the data to be sent.
 * @param len Length of the data.
 * @param src Source IP address.
 * @param dst Destination IP address.
 * @return Number of bytes sent on success, -1 on failure.
 */
extern ssize_t ip_send_packet(uint8_t protocol, const uint8_t *data, size_t len, IPAddress src, IPAddress dst);

/**
 * @brief Registers a handler function for a specific IP protocol.
 *
 * @param name Name of the IP protocol.
 * @param type IP protocol number.
 * @param handler Pointer to the handler function.
 * @return 0 on success, -1 on failure.
 */
extern int ip_register_protocol(const char *name, uint8_t type, void (*handler)(const uint8_t *data, size_t len, IPAddress src, IPAddress dst, struct IP_INTERFACE *iface));

/**
 * @brief Retrieves the name of an IP protocol based on its number.
 *
 * @param type IP protocol number.
 * @return Pointer to the name of the IP protocol, or NULL if not found.
 */
extern char *ip_get_protocol_name(uint8_t type);

/**
 * @brief Initializes the IP module.
 *
 * @return 0 on success, -1 on failure.
 */
extern int ip_initialize(void);

#endif
