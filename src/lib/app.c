#include "app.h"

volatile sig_atomic_t terminate;

void signal_handler(int sig){
    (void)sig;
    terminate = 1;
    network_interrupt();
    close(0);
}

int setup_network(void){
    struct network_device *device;
    struct IP_INTERFACE *interface;

    // handle signals
    signal(SIGINT, signal_handler);

    // initialize network stack
    if(network_init() == -1){
        errorf("failed to initiailized the network stack");
        return -1;
    }

    device = ether_tap_init(ETHER_TAP_NAME, ETHER_TAP_HW_ADDR);
    if(!device){
        errorf("failed to initilize the ethernet device");
        return -1;
    }

    interface = ip_allocate_interface(ETHER_TAP_IP_ADDR, ETHER_TAP_NETMASK);
    if(!interface){
        errorf("failed to allocate ip interface");
        return -1;
    }

    if(ip_register_interface(device, interface) == -1){
        errorf("failed to register ip interface");
        return -1;
    }

    if(ip_set_default_gateway(interface, DEFAULT_GATEWAY) == -1){
        errorf("failed to set default gateway");
        return -1;
    }

    if(network_run() == -1){
        errorf("failed to run network");
        return -1;
    }

    return 0;
}