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
 * @brief Sends a UDP packet to a network destination.
 *
 * This function sends a UDP packet from a source IP endpoint to a destination IP endpoint.
 *
 * @param src The source IP endpoint.
 * @param dst The destination IP endpoint.
 * @param buf The buffer containing the data to be sent.
 * @param len The length of the data in the buffer.
 * @return The number of bytes sent on success, or -1 on failure.
 */
extern ssize_t send_udp_packet_to_network(struct IP_ENDPOINT *src, struct IP_ENDPOINT *dst,
                                          const uint8_t *buf, size_t len);

/**
 * @brief Initializes the UDP subsystem.
 *
 * This function initializes the UDP subsystem and prepares it for use.
 *
 * @return 0 on success, or a negative value on failure.
 */
extern int initialize_udp_subsystem(void);

/**
 * @brief Opens a new UDP socket.
 *
 * This function opens a new UDP socket and returns its identifier.
 *
 * @return The identifier of the newly opened UDP socket, or a negative value on failure.
 */
extern int open_new_udp_socket(void);

/**
 * @brief Binds a UDP socket to a local IP endpoint.
 *
 * This function binds a UDP socket to a specific local IP endpoint.
 *
 * @param index The index of the UDP socket.
 * @param local The local IP endpoint to bind to.
 * @return 0 on success, or a negative value on failure.
 */
extern int bind_udp_socket_to_local_endpoint(int index, struct IP_ENDPOINT *local);

/**
 * @brief Sends a UDP packet over a socket.
 *
 * This function sends a UDP packet over a specific UDP socket to a foreign IP endpoint.
 *
 * @param id The identifier of the UDP socket.
 * @param buf The buffer containing the data to be sent.
 * @param len The length of the data in the buffer.
 * @param foreign The foreign IP endpoint to send the packet to.
 * @return The number of bytes sent on success, or -1 on failure.
 */
extern ssize_t send_udp_packet_over_socket(int id, uint8_t *buf, size_t len, struct IP_ENDPOINT *foreign);

/**
 * @brief Receives a UDP packet from a socket.
 *
 * This function receives a UDP packet from a specific UDP socket and stores it in a buffer.
 *
 * @param id The identifier of the UDP socket.
 * @param buf The buffer to store the received data.
 * @param size The size of the buffer.
 * @param foreign The foreign IP endpoint from which the packet was received.
 * @return The number of bytes received on success, or -1 on failure.
 */
extern ssize_t receive_udp_packet_from_socket(int id, uint8_t *buf, size_t size, struct IP_ENDPOINT *foreign);

/**
 * @brief Closes a UDP socket.
 *
 * This function closes a UDP socket with the specified identifier.
 *
 * @param id The identifier of the UDP socket to close.
 * @return 0 on success, or a negative value on failure.
 */
extern int close_udp_socket(int id);

#endif // UDP_H
