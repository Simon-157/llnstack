/**
 * @file icmp.h
 * @brief Header file for ICMP (Internet Control Message Protocol) module.
 *
 * This file contains the declarations and constants related to ICMP.
 * ICMP is used for error reporting, diagnostic messages, and network testing.
 * It provides feedback about problems in the communication environment.
 */

#ifndef ICMP_H
#define ICMP_H

#include <stddef.h>
#include <stdint.h>

#include "ip2.h"

#define ICMP_HDR_SIZE 8

// ICMP message types
#define ICMP_TYPE_ECHOREPLY           0
#define ICMP_TYPE_DEST_UNREACH        3
#define ICMP_TYPE_SOURCE_QUENCH       4
#define ICMP_TYPE_REDIRECT            5
#define ICMP_TYPE_ECHO                8
#define ICMP_TYPE_TIME_EXCEEDED      11
#define ICMP_TYPE_PARAM_PROBLEM      12
#define ICMP_TYPE_TIMESTAMP          13
#define ICMP_TYPE_TIMESTAMPREPLY     14
#define ICMP_TYPE_INFO_REQUEST       15
#define ICMP_TYPE_INFO_REPLY         16

// ICMP destination unreachable codes
#define ICMP_CODE_NET_UNREACH         0
#define ICMP_CODE_HOST_UNREACH        1
#define ICMP_CODE_PROTO_UNREACH       2
#define ICMP_CODE_PORT_UNREACH        3
#define ICMP_CODE_FRAGMENT_NEEDED     4
#define ICMP_CODE_SOURCE_ROUTE_FAILED 5

// ICMP redirect codes
#define ICMP_CODE_REDIRECT_NET        0
#define ICMP_CODE_REDIRECT_HOST       1
#define ICMP_CODE_REDIRECT_TOS_NET    2
#define ICMP_CODE_REDIRECT_TOS_HOST   3

// ICMP time exceeded codes
#define ICMP_CODE_EXCEEDED_TTL        0
#define ICMP_CODE_EXCEEDED_FRAGMENT   1

/**
 * @brief Sends an ICMP message.
 *
 * This function sends an ICMP message with the specified type, code, values, data, length, source IP address, and destination IP address.
 *
 * @param type The type of the ICMP message.
 * @param code The code of the ICMP message.
 * @param values Additional values for the ICMP message.
 * @param data The data payload of the ICMP message.
 * @param len The length of the data payload.
 * @param src The source IP address.
 * @param dst The destination IP address.
 * @return Returns 0 on success, or a negative error code on failure.
 */
extern int icmp_output(uint8_t type, uint8_t code, uint32_t values, const uint8_t *data, size_t len, IPAddress src, IPAddress dst);

/**
 * @brief Initializes the ICMP module.
 *
 * This function initializes the ICMP module and sets up any necessary resources.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
extern int icmp_init(void);

#endif