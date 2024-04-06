#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <arpa/inet.h>

#include "sock.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4000

int main(int argc, char *argv[]) {
  int soc, ret;
  struct sockaddr_in addr = { .sin_family = AF_INET };
  uint8_t buf[1024] = "Hello from client";
  socklen_t addrlen = sizeof(addr);

  // Convert server IP address to binary form
  if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) == -1) {
    perror("inet_pton");
    exit(1);
  }

  // Set server port
  addr.sin_port = htons(SERVER_PORT);

  // Open a UDP socket
  soc = sock_open(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (soc == -1) {
    perror("sock_open");
    exit(1);
  }

  // Send data to the server
  ret = sendto(soc, buf, strlen((char *)buf), 0, (struct sockaddr *)&addr, addrlen);
  if (ret == -1) {
    perror("sendto");
    close(soc);
    exit(1);
  }

  printf("Sent %d bytes to %s:%d\n", ret, SERVER_IP, SERVER_PORT);

  // Receive data from the server (optional)
  ret = recvfrom(soc, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen);
  if (ret != -1) {
    printf("Received %d bytes from server\n", ret);
    printf("%s\n", buf);
  } else {
    perror("recvfrom");
  }

  close(soc);
  return 0;
}
