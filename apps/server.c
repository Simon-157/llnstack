#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>


#include "util.h"
#include "net2.h"
#include "ip2.h"
#include "icmp.h"
#include "udp.h"
#include "sock.h"

#include "loopback.h"
#include "ethertap.h"

#include "params.h"

#define SERVER_PORT 4000

int main(int argc, char *argv[]) {
  int soc, ret;
  struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(SERVER_PORT) };
  uint8_t buf[1024];
  socklen_t addrlen = sizeof(addr);

  // Open a UDP socket
 soc = sock_open(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (soc == -1) {
    errorf("sock_open() failure");
    return -1;
  }

  // Bind the socket to the server port
  if (bind(soc, (struct sockaddr *)&addr, addrlen) == -1) {
    perror("bind");
    close(soc);
    exit(1);
  }

  printf("Server started on port %d\n", SERVER_PORT);

  while (1) {
    // Receive data from the client
    ret = recvfrom(soc, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);
    if (ret == -1) {
      perror("recvfrom");
      close(soc);
      exit(1);
    }

    printf("Received %d bytes from %s:%d\n", ret, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    // Send data back to the client
    if (sendto(soc, buf, ret, 0, (struct sockaddr *)&addr, addrlen) == -1) {
      perror("sendto");
      close(soc);
      exit(1);
    }
  }

  close(soc);
  return 0;
}
