/**
 * @file handler.h
 * @brief This file contains declarations and inline functions for handling memory allocation, mutex operations, and interrupt handling.
 */

#ifndef HANDLE_H
#define HANDLE_H

#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief Allocates memory of the specified size.
 * @param size The size of the memory to allocate.
 * @return A pointer to the allocated memory.
 */
static inline void *memory_alloc(size_t size)
{
    return calloc(1, size);
}

/**
 * @brief Frees the memory pointed to by the given pointer.
 * @param ptr A pointer to the memory to free.
 */
static inline void memory_free(void *ptr)
{
    free(ptr);
}

/**
 * @brief Type definition for a mutex.
 */
typedef pthread_mutex_t mutex_t;

#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

/**
 * @brief Initializes a mutex.
 * @param mutex A pointer to the mutex to initialize.
 * @return 0 on success, or an error code on failure.
 */
static inline int mutex_init(mutex_t *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

/**
 * @brief Locks a mutex.
 * @param mutex A pointer to the mutex to lock.
 * @return 0 on success, or an error code on failure.
 */
static inline int mutex_lock(mutex_t *mutex)
{
    return pthread_mutex_lock(mutex);
}

/**
 * @brief Unlocks a mutex.
 * @param mutex A pointer to the mutex to unlock.
 * @return 0 on success, or an error code on failure.
 */
static inline int mutex_unlock(mutex_t *mutex)
{
    return pthread_mutex_unlock(mutex);
}

/**
 * @brief Structure representing the scheduling context.
 */
struct sched_ctx {
    pthread_cond_t cond; /**< Condition variable for signaling threads. */
    int interrupted; /**< Flag indicating if the context is interrupted. */
    int wc; /**< Wait count. */
};

#define SCHED_CTX_INITIALIZER {PTHREAD_COND_INITIALIZER, 0, 0}

/**
 * @brief Initializes a scheduling context.
 * @param ctx A pointer to the scheduling context to initialize.
 * @return 0 on success, or an error code on failure.
 */
extern int sched_ctx_init(struct sched_ctx *ctx);

/**
 * @brief Destroys a scheduling context.
 * @param ctx A pointer to the scheduling context to destroy.
 * @return 0 on success, or an error code on failure.
 */
extern int sched_ctx_destroy(struct sched_ctx *ctx);

/**
 * @brief Puts the calling thread to sleep until the specified absolute time.
 * @param ctx A pointer to the scheduling context.
 * @param mutex A pointer to the mutex to lock before sleeping.
 * @param abstime A pointer to the absolute time to sleep until.
 * @return 0 on success, or an error code on failure.
 */
extern int sched_sleep(struct sched_ctx *ctx, mutex_t *mutex, const struct timespec *abstime);

/**
 * @brief Wakes up a thread waiting in the scheduling context.
 * @param ctx A pointer to the scheduling context.
 * @return 0 on success, or an error code on failure.
 */
extern int sched_wakeup(struct sched_ctx *ctx);

/**
 * @brief Interrupts the scheduling context.
 * @param ctx A pointer to the scheduling context.
 * @return 0 on success, or an error code on failure.
 */
extern int sched_interrupt(struct sched_ctx *ctx);

/**
 * @brief Requests an interrupt handler for the specified IRQ.
 * @param irq The IRQ number.
 * @param handler A pointer to the interrupt handler function.
 * @param flags Flags for the interrupt handler.
 * @param name The name of the interrupt handler.
 * @param dev A pointer to the device associated with the interrupt.
 * @return 0 on success, or an error code on failure.
 */
extern int intr_request_irq(unsigned int irq, int (*handler)(unsigned int irq, void *id), int flags, const char *name, void *dev);

/**
 * @brief Runs the interrupt handling loop.
 * @return 0 on success, or an error code on failure.
 */
extern int intr_run(void);

/**
 * @brief Initializes the interrupt handling subsystem.
 * @return 0 on success, or an error code on failure.
 */
extern int intr_init(void);

/**
 * @brief Raises a software interrupt.
 */
static inline void raise_softirq(void)
{
    kill(getpid(), SIGUSR1);
}

#endif