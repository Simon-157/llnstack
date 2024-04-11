#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "ip2.h"
#include "icmp.h"

#define ICMP_BUFSIZ MAX_IP_PAYLOAD_SIZE

// ANSI escape codes for text formatting and colors
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BOLD    "\x1b[1m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"

struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t sum;
    uint32_t values;
};

struct icmp_echo {
    uint8_t type;
    uint8_t code;
    uint16_t sum;
    uint16_t id;
    uint16_t seq;
};

static char * icmp_type_ntoa(uint8_t type) {
    switch (type) {
    case ICMP_TYPE_ECHOREPLY: return "EchoReply";
    case ICMP_TYPE_DEST_UNREACH: return "DestinationUnreachable";
    case ICMP_TYPE_SOURCE_QUENCH: return "SourceQuench";
    case ICMP_TYPE_REDIRECT: return "Redirect";
    case ICMP_TYPE_ECHO: return "Echo";
    case ICMP_TYPE_TIME_EXCEEDED: return "TimeExceeded";
    case ICMP_TYPE_PARAM_PROBLEM: return "ParameterProblem";
    case ICMP_TYPE_TIMESTAMP: return "Timestamp";
    case ICMP_TYPE_TIMESTAMPREPLY: return "TimestampReply";
    case ICMP_TYPE_INFO_REQUEST: return "InformationRequest";
    case ICMP_TYPE_INFO_REPLY: return "InformationReply";
    }
    return "Unknown";
}

static void icmp_dump(const uint8_t *data, size_t len) {
    struct icmp_header *hdr;
    struct icmp_echo *echo;

    flockfile(stderr);
    hdr = (struct icmp_header *)data;

    fprintf(stderr, "**********************************************\n");
    fprintf(stderr, "*        Starting New ICMP Packet Analysis     *\n");
    fprintf(stderr, "**********************************************\n");


    // Initialize a string buffer to hold the formatted output
    char output_buffer[1024];
    int offset = 0; // Keep track of the current position in the buffer

    // Print type with color
    const char* type_color = hdr->type == ICMP_TYPE_ECHO ? ANSI_COLOR_YELLOW : ANSI_COLOR_GREEN;
    offset += snprintf(output_buffer + offset, sizeof(output_buffer) - offset, ANSI_COLOR_BOLD "Type: " ANSI_COLOR_RESET "%s%-15s (%u)    ", type_color, icmp_type_ntoa(hdr->type), hdr->type);

    // Print code
    offset += snprintf(output_buffer + offset, sizeof(output_buffer) - offset, ANSI_COLOR_BOLD "Code: " ANSI_COLOR_RESET "%-15u    ", hdr->code);

    // Print checksum and calculated checksum
    uint16_t checksum = ntoh16(hdr->sum);
    uint16_t calculated_checksum = ntoh16(cksum16((uint16_t *)data, len, -hdr->sum));
    const char* checksum_color = checksum == calculated_checksum ? ANSI_COLOR_GREEN : ANSI_COLOR_RED;
    offset += snprintf(output_buffer + offset, sizeof(output_buffer) - offset, ANSI_COLOR_BOLD "Sum: " ANSI_COLOR_RESET "0x%04x (%s0x%04x%s)    ", checksum, checksum_color, calculated_checksum, ANSI_COLOR_RESET);

    // Print ID and sequence if applicable
    switch (hdr->type) {
        case ICMP_TYPE_ECHOREPLY:
        case ICMP_TYPE_ECHO:
            echo = (struct icmp_echo *)hdr;
            offset += snprintf(output_buffer + offset, sizeof(output_buffer) - offset, ANSI_COLOR_BOLD "ID: " ANSI_COLOR_RESET "%-15u    ", ntoh16(echo->id));
            offset += snprintf(output_buffer + offset, sizeof(output_buffer) - offset, ANSI_COLOR_BOLD "Seq: " ANSI_COLOR_RESET "%u\n", ntoh16(echo->seq));
            break;
        default:
            // Print values for other types
            offset += snprintf(output_buffer + offset, sizeof(output_buffer) - offset, ANSI_COLOR_BOLD "Values: " ANSI_COLOR_RESET "0x%08x\n", ntoh32(hdr->values));
            break;
    }

#ifdef HEXDUMP
    hexdump(stderr, data, len);
#endif

    fprintf(stderr, "%s", output_buffer);
    funlockfile(stderr);
}


static void icmp_input(const uint8_t *data, size_t len, IPAddress src, IPAddress dst, struct IP_INTERFACE *iface)
{
    struct icmp_header *hdr;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr3[MAX_IP_ADDRESS_STRING_LENGTH];

    if (len < sizeof(*hdr)) {
        errorf("too short");
        return;
    }
    hdr = (struct icmp_header *)data;
    if (cksum16((uint16_t *)data, len, 0) != 0) {
        errorf("checksum error, sum=0x%04x, verify=0x%04x", ntoh16(hdr->sum), ntoh16(cksum16((uint16_t *)data, len, -hdr->sum)));
        return;
    }
    debugf("%s => %s, type=%s(%u), len=%zu, iface=%s",
        ip_address_to_string(src, addr1, sizeof(addr1)),
        ip_address_to_string(dst, addr2, sizeof(addr2)),
        icmp_type_ntoa(hdr->type), hdr->type, len,
        ip_address_to_string(iface->unicast, addr3, sizeof(addr3)));
    icmp_dump(data, len);
    switch (hdr->type) {
    case ICMP_TYPE_ECHO:
        if (dst != iface->unicast) {
            dst = iface->unicast;
        }
        icmp_output(ICMP_TYPE_ECHOREPLY, hdr->code, hdr->values, (uint8_t *)(hdr + 1), len - sizeof(*hdr), dst, src);
        break;
    default:
        /* ignore */
        break;
    }
}

int icmp_output(uint8_t type, uint8_t code, uint32_t values, const uint8_t *data, size_t len, IPAddress src, IPAddress dst)
{
    uint8_t buf[ICMP_BUFSIZ];
    struct icmp_header *hdr;
    size_t msg_len;
    char addr1[MAX_IP_ADDRESS_STRING_LENGTH];
    char addr2[MAX_IP_ADDRESS_STRING_LENGTH];

    hdr = (struct icmp_header *)buf;
    hdr->type = type;
    hdr->code = code;
    hdr->sum = 0;
    hdr->values = values;
    memcpy(hdr + 1, data, len);
    msg_len = sizeof(*hdr) + len;
    hdr->sum = cksum16((uint16_t *)hdr, msg_len, 0);
    debugf("%s => %s, type=%s(%u), len=%zu",
        ip_address_to_string(src, addr1, sizeof(addr1)),
        ip_address_to_string(dst, addr2, sizeof(addr2)),
        icmp_type_ntoa(hdr->type), hdr->type, msg_len);
    icmp_dump((uint8_t *)hdr, msg_len);
    return ip_send_packet(ICMP_PROTOCOL, (uint8_t *)hdr, msg_len, src, dst);
}

int icmp_init(void)
{
    if (ip_register_protocol("ICMP", ICMP_PROTOCOL, icmp_input) == -1) {
        errorf("ip protocol register failure");
        return -1;
    }
    return 0;
}