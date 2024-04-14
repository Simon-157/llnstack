#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "192.0.2.2"
#define PORT 4000
#define BUFFER_SIZE 1024

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create client socket
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Enter message: ");
        fgets(buffer, BUFFER_SIZE, stdin);

        // Send message to server
        sendto(client_fd, buffer, strlen(buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }

    return 0;
} 