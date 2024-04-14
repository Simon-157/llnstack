#ifndef APP_H
#define APP_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "util.h"
#include "net2.h"
#include "ip2.h"
#include "icmp.h"
#include "udp.h"
#include "sock.h"

#include "loopback.h"
#include "ethertap.h"

#include "params.h"


extern volatile sig_atomic_t terminate;

void signal_handler(int sig);

int setup_network(void);

#endif // APP_H