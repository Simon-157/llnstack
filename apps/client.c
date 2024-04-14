#include "app.h"

/**
 * @brief The client application
 * @details This application opens a UDP socket, binds it to a local address
 * if one is provided and sends data to a remote address.
 */
int main(int argc, char *argv[]){
    int command_option; // The command-line option
    int socket_fd; // The socket file descriptor
    long int local_port; // The local port

    struct sockaddr_in local_address = { .sin_family=AF_INET }; // The local address
    struct sockaddr_in remote_address; // The remote address
    uint8_t data_buffer[1024]; // The data buffer

    // Parse the command-line options
    while((command_option = getopt(argc, argv, "a:p:")) != -1){
        switch(command_option){
            case 'a': // Set the local address
                if(ip_string_to_endpoint(optarg, &local_address.sin_addr) == -1){
                    errorf(stderr, "Invalid IP address: %s\n", optarg);
                    return -1;
                }
                break;
            case 'p': // Set the local port
                local_port = strtol(optarg, NULL, 10);
                if(local_port < 0 || local_port > UINT16_MAX){
                    errorf(stderr, "Invalid port: %s\n", optarg);
                    return -1;
                }
                local_address.sin_port = htons(local_port);
                break;
            default:
                fprintf(stderr, "Usage: %s [-a local_address] [-p local_port] remote_address:port\n", argv[0]);
                return -1;

        }

    }

    int arg_count = argc - optind; // The number of remaining arguments
    if(arg_count != 1){ // Check if there is exactly one argument left
        printf("Usage: %s [-a local_address] [-p local_port] remote_address:port\n", argv[0]);
        return -1;
    }

    if(sockaddr_pton(argv[optind], (struct sockaddr *)&remote_address, sizeof(remote_address)) == -1){ // Parse the remote address
        errorf(stderr, "Invalid remote address: %s\n", argv[optind]);
        return -1;
    }

    // if(setup_network() == -1){ // Initialize the network
    //     errorf(stderr, "Network setup failure\n");
    //     return -1;
    // }

    socket_fd = sock_open(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Open a UDP socket

    if(socket_fd == -1){ // Check if the socket was opened successfully
        errorf(stderr, "Socket opening failure\n");
        return -1;
    }

    if(local_address.sin_port){ // Bind the local address if it is specified
        if(sock_bind(socket_fd, (struct sockaddr *)&local_address, sizeof(local_address)) == -1){
            errorf(stderr, "Socket binding failure\n");
            close_udp_socket(socket_fd);
            network_shutdown();
            return -1;
        }
    }

    while(!terminate){ // Read data from stdin and send it to the remote address
        printf("Enter data to send (max 1024 bytes):\n");
        if (!fgets((char *)data_buffer, sizeof(data_buffer), stdin)) {
            break;
        }
        ssize_t data_length = strlen((char *)data_buffer);
        if (data_length <= 0) {
            continue; // No data entered, continue to next iteration
        }
      
        if (data_buffer[data_length - 1] == '\n') {
            data_buffer[data_length - 1] = '\0';
            data_length--; // adjust length after removing newline
        }
        ssize_t ret = sock_sendto(socket_fd, data_buffer, data_length, (struct sockaddr *)&remote_address, sizeof(remote_address));
        if(ret == -1){
            if(errno == EINTR){
                continue;
            }
            errorf(stderr, "Socket sending failure\n");
            break;
        }
        printf("Data sent successfully.\n");
    }

    close_udp_socket(socket_fd); // Close the socket
    network_shutdown(); // Shutdown the network
    return 0;
}

