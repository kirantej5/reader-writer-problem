#include "rwlock.h"

#include <errno.h>

static int wait_semaphore(sem_t *semaphore)
{
    int result;
    do {
        result = sem_wait(semaphore);
    } while (result == -1 && errno == EINTR);
    return result;
}

int rwlock_init(rwlock_t *lock)
{
    if (lock == NULL) {
        errno = EINVAL;
        return -1;
    }

    lock->active_readers = 0;
    if (sem_init(&lock->turnstile, 0, 1) != 0) {
        return -1;
    }
    if (sem_init(&lock->room_empty, 0, 1) != 0) {
        sem_destroy(&lock->turnstile);
        return -1;
    }
    if (pthread_mutex_init(&lock->reader_mutex, NULL) != 0) {
        sem_destroy(&lock->room_empty);
        sem_destroy(&lock->turnstile);
        return -1;
    }
    return 0;
}

int rwlock_read_lock(rwlock_t *lock)
{
    if (wait_semaphore(&lock->turnstile) != 0) {
        return -1;
    }
    if (sem_post(&lock->turnstile) != 0) {
        return -1;
    }
    if (pthread_mutex_lock(&lock->reader_mutex) != 0) {
        return -1;
    }
    lock->active_readers++;
    if (lock->active_readers == 1 && wait_semaphore(&lock->room_empty) != 0) {
        lock->active_readers--;
        pthread_mutex_unlock(&lock->reader_mutex);
        return -1;
    }
    return pthread_mutex_unlock(&lock->reader_mutex);
}

int rwlock_read_unlock(rwlock_t *lock)
{
    if (pthread_mutex_lock(&lock->reader_mutex) != 0) {
        return -1;
    }
    lock->active_readers--;
    if (lock->active_readers == 0 && sem_post(&lock->room_empty) != 0) {
        pthread_mutex_unlock(&lock->reader_mutex);
        return -1;
    }
    return pthread_mutex_unlock(&lock->reader_mutex);
}

int rwlock_write_lock(rwlock_t *lock)
{
    if (wait_semaphore(&lock->turnstile) != 0) {
        return -1;
    }
    if (wait_semaphore(&lock->room_empty) != 0) {
        sem_post(&lock->turnstile);
        return -1;
    }
    return 0;
}

int rwlock_write_unlock(rwlock_t *lock)
{
    if (sem_post(&lock->room_empty) != 0) {
        return -1;
    }
    return sem_post(&lock->turnstile);
}

int rwlock_destroy(rwlock_t *lock)
{
    int result = 0;
    if (pthread_mutex_destroy(&lock->reader_mutex) != 0) {
        result = -1;
    }
    if (sem_destroy(&lock->room_empty) != 0) {
        result = -1;
    }
    if (sem_destroy(&lock->turnstile) != 0) {
        result = -1;
    }
    return result;
}
