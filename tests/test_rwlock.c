#include "rwlock.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define READER_THREADS 6
#define WRITER_THREADS 3
#define OPERATIONS 1000

typedef struct {
    rwlock_t lock;
    pthread_barrier_t start;
    pthread_mutex_t monitor;
    int value;
    int active_readers;
    int active_writers;
    int maximum_readers;
    int violations;
} test_state_t;

static void short_pause(void)
{
    struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
    nanosleep(&pause, NULL);
}

static void *reader(void *argument)
{
    test_state_t *state = argument;
    pthread_barrier_wait(&state->start);
    for (int i = 0; i < OPERATIONS; i++) {
        assert(rwlock_read_lock(&state->lock) == 0);
        pthread_mutex_lock(&state->monitor);
        state->active_readers++;
        if (state->active_writers != 0) state->violations++;
        if (state->active_readers > state->maximum_readers) state->maximum_readers = state->active_readers;
        pthread_mutex_unlock(&state->monitor);
        short_pause();
        pthread_mutex_lock(&state->monitor);
        state->active_readers--;
        pthread_mutex_unlock(&state->monitor);
        assert(rwlock_read_unlock(&state->lock) == 0);
    }
    return NULL;
}

static void *writer(void *argument)
{
    test_state_t *state = argument;
    pthread_barrier_wait(&state->start);
    for (int i = 0; i < OPERATIONS; i++) {
        assert(rwlock_write_lock(&state->lock) == 0);
        pthread_mutex_lock(&state->monitor);
        if (state->active_writers != 0 || state->active_readers != 0) state->violations++;
        state->active_writers++;
        pthread_mutex_unlock(&state->monitor);
        state->value++;
        short_pause();
        pthread_mutex_lock(&state->monitor);
        state->active_writers--;
        pthread_mutex_unlock(&state->monitor);
        assert(rwlock_write_unlock(&state->lock) == 0);
    }
    return NULL;
}

int main(void)
{
    test_state_t state = {0};
    assert(rwlock_init(&state.lock) == 0);
    assert(pthread_barrier_init(&state.start, NULL, READER_THREADS + WRITER_THREADS) == 0);
    assert(pthread_mutex_init(&state.monitor, NULL) == 0);

    pthread_t readers[READER_THREADS];
    pthread_t writers[WRITER_THREADS];
    for (int i = 0; i < READER_THREADS; i++) assert(pthread_create(&readers[i], NULL, reader, &state) == 0);
    for (int i = 0; i < WRITER_THREADS; i++) assert(pthread_create(&writers[i], NULL, writer, &state) == 0);
    for (int i = 0; i < READER_THREADS; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < WRITER_THREADS; i++) pthread_join(writers[i], NULL);

    assert(state.value == WRITER_THREADS * OPERATIONS);
    assert(state.violations == 0);
    assert(state.maximum_readers >= 2);

    pthread_mutex_destroy(&state.monitor);
    pthread_barrier_destroy(&state.start);
    assert(rwlock_destroy(&state.lock) == 0);
    puts("reader-writer stress test passed");
    return EXIT_SUCCESS;
}
