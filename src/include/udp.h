/**
 * @file udp.h
 * @brief UDP networking interface
 *
 * This header provides an interface for using UDP sockets. It wraps the IP
 * layer to provide a higher-level interface for sending and receiving UDP
 * packets.
 *
 * @author Simon Junior Boateng
 */
#ifndef UDP_H
#define UDP_H

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint8_t */

#include "ip2.h"

/**
 * @brief Output a UDP packet over the network
 *
 * This function sends a UDP packet to the destination specified by the given
 * endpoints.
 *
 * @param src Source IP endpoint
 * @param dst Destination IP endpoint
 * @param buf Pointer to UDP payload
 * @param len Length of UDP payload
 * @return Number of bytes sent on success, negative on failure
 */
extern ssize_t udp_output(struct IP_ENDPOINT *src, struct IP_ENDPOINT *dst,
                          const uint8_t *buf, size_t len);

/**
 * @brief Initialize UDP subsystem
 *
 * This function initializes the UDP subsystem. It must be called before
 * any other UDP functions.
 *
 * @return 0 on success, negative on failure
 */
extern int udp_init(void);

/**
 * @brief Open a UDP socket
 *
 * This function opens a new UDP socket.
 *
 * @return Socket descriptor on success, negative on failure
 */
extern int udp_open(void);

/**
 * @brief Bind a UDP socket to a local IP endpoint
 *
 * This function binds a UDP socket to a local IP endpoint.
 *
 * @param index Socket descriptor
 * @param local Local IP endpoint
 * @return 0 on success, negative on failure
 */
extern int udp_bind(int index, struct IP_ENDPOINT *local);

/**
 * @brief Send a UDP packet over a socket
 *
 * This function sends a UDP packet to the destination specified by the given
 * foreign endpoint.
 *
 * @param id Socket descriptor
 * @param buf Pointer to UDP payload
 * @param len Length of UDP payload
 * @param foreign Destination IP endpoint
 * @return Number of bytes sent on success, negative on failure
 */
extern ssize_t udp_sendto(int id, uint8_t *buf, size_t len, struct IP_ENDPOINT *foreign);

/**
 * @brief Receive a UDP packet from a socket
 *
 * This function receives a UDP packet from the given socket.
 *
 * @param id Socket descriptor
 * @param buf Buffer to store received data
 * @param size Size of buffer
 * @param foreign Pointer to store source IP endpoint
 * @return Number of bytes received on success, negative on failure
 */
extern ssize_t udp_recvfrom(int id, uint8_t *buf, size_t size, struct IP_ENDPOINT *foreign);

/**
 * @brief Close a UDP socket
 *
 * This function closes a UDP socket.
 *
 * @param id Socket descriptor
 * @return 0 on success, negative on failure
 */
extern int udp_close(int id);

#endif
