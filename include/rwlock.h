#ifndef RWLOCK_H
#define RWLOCK_H

#include <semaphore.h>
#include <pthread.h>

/**
 * A writer-priority reader-writer lock.
 *
 * The turnstile prevents new readers from overtaking a waiting writer. The
 * room_empty semaphore protects the shared resource, while reader_mutex
 * protects the active reader count.
 */
typedef struct {
    sem_t turnstile;
    sem_t room_empty;
    pthread_mutex_t reader_mutex;
    unsigned int active_readers;
} rwlock_t;

int rwlock_init(rwlock_t *lock);
int rwlock_read_lock(rwlock_t *lock);
int rwlock_read_unlock(rwlock_t *lock);
int rwlock_write_lock(rwlock_t *lock);
int rwlock_write_unlock(rwlock_t *lock);
int rwlock_destroy(rwlock_t *lock);

#endif
