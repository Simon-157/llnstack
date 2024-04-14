#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include "handler.h"

#include "util.h"
#include "net2.h"
#include <asm-generic/signal-defs.h>

struct irq_entry {
    struct irq_entry *next;
    unsigned int irq;
    int (*handler)(unsigned int irq, void *dev);
    int flags;
    char name[16];
    void *dev;
};

sigset_t sigmask;
struct irq_entry *irq_vec;

int intr_request_irq(unsigned int irq, int (*handler)(unsigned int irq, void *dev), int flags, const char *name, void *dev)
{
    debugf("interrupt_num=%u, handler=%p, flags=%d, name=%s, device=%p", irq, handler, flags, name, dev);
    struct irq_entry *entry;
    for (entry = irq_vec; entry; entry = entry->next) {
        if (entry->irq == irq) {
            if (entry->flags ^ NETWORK_IRQ_SHARED || flags ^ NETWORK_IRQ_SHARED) {
                errorf("conflicts with already registered IRQs");
                return -1;
            }
        }
    }
    entry = memory_alloc(sizeof(*entry));
    if (!entry) {
        errorf("memory allocation failure");
        return -1;
    }
    entry->irq = irq;
    entry->handler = handler;
    entry->flags = flags;
    strncpy(entry->name, name, sizeof(entry->name)-1);
    entry->dev = dev;
    entry->next = irq_vec;
    irq_vec = entry;
    sigaddset(&sigmask, irq);
    debugf("interrupt registered: number =%u, name=%s", irq, name);
    return 0;
}

static int intr_timer_setup(struct itimerspec *interval)
{
    timer_t id;

    if (timer_create(CLOCK_REALTIME, NULL, &id) == -1) {
        errorf("timer creation failed: %s", strerror(errno));
        return -1;
    }
    if (timer_settime(id, 0, interval, NULL) == -1) {
        errorf("timer settime failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static void * intr_thread(void *arg)
{
    struct timespec ts = {0, 1000000}; // 1ms
    struct itimerspec interval = {ts, ts};
    int sig, err;
    struct irq_entry *entry;

    if (intr_timer_setup(&interval) == -1) {
        return NULL;
    }
    while (1) {
        err = sigwait(&sigmask, &sig);
        if (err) {
            errorf("signal wait failed %s", strerror(err));
            break;
        }
        switch (sig) {
        case SIGUSR1:
            network_protocol_handler();
            break;
        case SIGUSR2:
            network_event_handler();
            break;
        case SIGALRM:
            network_timer_handler();
            break;
        default:
            for (entry = irq_vec; entry; entry = entry->next) {
                if (entry->irq == (unsigned int)sig) {
                    debugf("interrupt_handler: number=%d, name=%s", entry->irq, entry->name);
                    entry->handler(entry->irq, entry->dev);
                }
            }
            break;
        }
    }
    return NULL;
}

pthread_t tid;

int intr_run(void)
{
    int err;

    err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
    if (err) {
        errorf(" sigmask failed  %s", strerror(err));
        return -1;
    }
    err = pthread_create(&tid, NULL, intr_thread, NULL);
    if (err) {
        errorf("thread create failed %s", strerror(err));
        return -1;
    }
    return 0;
}

int intr_init(void)
{
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGUSR1);
    sigaddset(&sigmask, SIGUSR2);
    sigaddset(&sigmask, SIGALRM);
    return 0;
}