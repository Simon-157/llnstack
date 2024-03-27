/**
 * @file network.h
 * @brief Header file for network device and interface management.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <signal.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

/**
 * @brief Network device types.
 */
#define NETWORK_DEVICE_TYPE_NULL 0x0000
#define NETWORK_DEVICE_TYPE_LOOPBACK 0x0001
#define NETWORK_DEVICE_TYPE_ETHERNET 0x0002

/**
 * @brief Network device flags.
 */
#define NETWORK_DEVICE_FLAG_UP 0x0001
#define NETWORK_DEVICE_FLAG_LOOPBACK 0x0010
#define NETWORK_DEVICE_FLAG_BROADCAST 0x0020
#define NETWORK_DEVICE_FLAG_P2P 0x0040
#define NETWORK_DEVICE_FLAG_NEED_ARP 0x0100

#define NETWORK_DEVICE_ADDR_LEN 16

/**
 * @brief Macro to check if a network device is up.
 */
#define NETWORK_DEVICE_IS_UP(x) ((x)->flags & NETWORK_DEVICE_FLAG_UP)

/**
 * @brief Macro to get the state of a network device.
 */
#define NETWORK_DEVICE_STATE(x) (NETWORK_DEVICE_IS_UP(x) ? "up" : "down")

#define NETWORK_INTERFACE_FAMILY_IP 1
#define NETWORK_INTERFACE_FAMILY_IPV6 2

#define NETWORK_INTERFACE(x) ((struct network_interface *)(x))

/**
 * @brief Network protocol types.
 */
#define NETWORK_PROTOCOL_TYPE_IP 0x0800
#define NETWORK_PROTOCOL_TYPE_ARP 0x0806
#define NETWORK_PROTOCOL_TYPE_IPV6 0x86dd

#define NETWORK_IRQ_SHARED 0x0001

/**
 * @struct network_interface
 * @brief Network interface structure.
 */
struct network_interface
{
    struct network_interface *next; /**< Pointer to the next network interface. */
    struct network_device *dev; /**< Pointer to the network device associated with this interface. */
    int family; /**< Family of the interface (IP or IPv6). */
};

/**
 * @struct network_device_operations
 * @brief Network device operations structure.
 */
struct network_device_operations
{
    int (*open)(struct network_device *dev); /**< Function pointer to open the network device. */
    int (*close)(struct network_device *dev); /**< Function pointer to close the network device. */
    int (*transmit)(struct network_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst); /**< Function pointer to transmit data through the network device. */
    int (*poll)(struct network_device *dev); /**< Function pointer to poll the network device for incoming data. */
};

/**
 * @struct network_device
 * @brief Network device structure.
 */
struct network_device
{
    struct network_device *next; /**< Pointer to the next network device. */
    struct network_interface *interfaces; /**< Pointer to the network interfaces associated with this device. */
    unsigned int index; /**< Index of the network device. */
    char name[IFNAMSIZ]; /**< Name of the network device. */
    uint16_t type; /**< Type of the network device. */
    uint16_t mtu; /**< Maximum Transmission Unit (MTU) of the network device. */
    uint16_t flags; /**< Flags of the network device. */
    uint16_t header_len; /**< Header length of the network device. */
    uint16_t address_len; /**< Address length of the network device. */
    uint8_t address[NETWORK_DEVICE_ADDR_LEN]; /**< Address of the network device. */
    union
    {
        uint8_t peer[NETWORK_DEVICE_ADDR_LEN]; /**< Peer address of the network device. */
        uint8_t broadcast[NETWORK_DEVICE_ADDR_LEN]; /**< Broadcast address of the network device. */
    };
    struct network_device_operations *ops; /**< Pointer to the network device operations. */
    void *priv; /**< Pointer to private data associated with the network device. */
};

/**
 * @brief Allocate a new network device.
 * @param setup Function pointer to setup the network device.
 * @return Pointer to the allocated network device.
 */
extern struct network_device *network_device_allocate(void (*setup)(struct network_device *dev));

/**
 * @brief Register a network device.
 * @param dev Pointer to the network device to register.
 * @return 0 on success, -1 on failure.
 */
extern int network_device_register(struct network_device *dev);

/**
 * @brief Add a network interface to a network device.
 * @param dev Pointer to the network device.
 * @param iface Pointer to the network interface to add.
 * @return 0 on success, -1 on failure.
 */
extern int network_device_add_interface(struct network_device *dev, struct network_interface *iface);

/**
 * @brief Get a network interface of a specific family from a network device.
 * @param dev Pointer to the network device.
 * @param family Family of the network interface (IP or IPv6).
 * @return Pointer to the network interface, or NULL if not found.
 */
extern struct network_interface *network_device_get_interface(struct network_device *dev, int family);

/**
 * @brief Output data through a network device.
 * @param dev Pointer to the network device.
 * @param type Type of the protocol.
 * @param data Pointer to the data to transmit.
 * @param len Length of the data.
 * @param dst Destination address.
 * @return 0 on success, -1 on failure.
 */
extern int network_device_output(struct network_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst);

/**
 * @brief Network input handler.
 * @param type Type of the protocol.
 * @param data Pointer to the received data.
 * @param len Length of the received data.
 * @param dev Pointer to the network device.
 * @return 0 on success, -1 on failure.
 */
extern int network_input_handler(uint16_t type, const uint8_t *data, size_t len, struct network_device *dev);

/**
 * @brief Register a network protocol.
 * @param name Name of the protocol.
 * @param type Type of the protocol.
 * @param handler Function pointer to the protocol handler.
 * @return 0 on success, -1 on failure.
 */
extern int network_protocol_register(const char *name, uint16_t type, void (*handler)(const uint8_t *data, size_t len, struct network_device *dev));

/**
 * @brief Get the name of a network protocol.
 * @param type Type of the protocol.
 * @return Name of the protocol, or NULL if not found.
 */
extern char *network_protocol_name(uint16_t type);

/**
 * @brief Network protocol handler.
 * @return 0 on success, -1 on failure.
 */
extern int network_protocol_handler(void);

/**
 * @brief Register a network timer.
 * @param name Name of the timer.
 * @param interval Timer interval.
 * @param handler Function pointer to the timer handler.
 * @return 0 on success, -1 on failure.
 */
extern int network_timer_register(const char *name, struct timeval interval, void (*handler)(void));

/**
 * @brief Network timer handler.
 * @return 0 on success, -1 on failure.
 */
extern int network_timer_handler(void);

/**
 * @brief
 * Subscribe to network events.
 * @param handler Function pointer to the event handler.
 * @param arg Argument to pass to the event handler.
 * @return 0 on success, -1 on failure.
 */
extern int network_event_subscribe(void (*handler)(void *arg), void *arg);

/**
 * @brief Network event handler.
 * @return 0 on success, -1 on failure.
 */
extern int network_event_handler(void);

/**
 * @brief Handle network interrupts.
 * @return 0 on success, -1 on failure.
 */
extern int network_interrupt(void);

/**
 * @brief Run the network stack.
 * @return 0 on success, -1 on failure.
 */
extern int network_run(void);

/**
 * @brief Shutdown the network stack.
 */
extern void network_shutdown(void);

/**
 * @brief Initialize the network stack.
 * @return 0 on success, -1 on failure.
 */
extern int network_init(void);

#endif
