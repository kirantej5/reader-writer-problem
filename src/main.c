#include "rwlock.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    rwlock_t lock;
    int value;
    atomic_int reads_completed;
    atomic_int writes_completed;
    int reader_count;
    int writer_count;
    int iterations;
    int delay_ms;
    pthread_mutex_t output_mutex;
} shared_store_t;

typedef struct {
    shared_store_t *store;
    int id;
} worker_arg_t;

static void pause_for(int milliseconds)
{
    struct timespec duration = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L
    };
    nanosleep(&duration, NULL);
}

static void *reader_worker(void *argument)
{
    worker_arg_t *worker = argument;
    shared_store_t *store = worker->store;

    for (int i = 0; i < store->iterations; i++) {
        rwlock_read_lock(&store->lock);
        int snapshot = store->value;
        rwlock_read_unlock(&store->lock);
        atomic_fetch_add(&store->reads_completed, 1);
        if (i == 0) {
            pthread_mutex_lock(&store->output_mutex);
            printf("reader %d observed value %d\n", worker->id, snapshot);
            pthread_mutex_unlock(&store->output_mutex);
        }
        pause_for(store->delay_ms);
    }
    return NULL;
}

static void *writer_worker(void *argument)
{
    worker_arg_t *worker = argument;
    shared_store_t *store = worker->store;

    for (int i = 0; i < store->iterations; i++) {
        rwlock_write_lock(&store->lock);
        store->value++;
        int updated = store->value;
        rwlock_write_unlock(&store->lock);
        atomic_fetch_add(&store->writes_completed, 1);
        if (i == 0) {
            pthread_mutex_lock(&store->output_mutex);
            printf("writer %d completed first update -> %d\n", worker->id, updated);
            pthread_mutex_unlock(&store->output_mutex);
        }
        pause_for(store->delay_ms);
    }
    return NULL;
}

static void usage(const char *program)
{
    printf("Usage: %s [--readers N] [--writers N] [--iterations N] [--delay-ms N]\n", program);
}

static int positive_number(const char *value)
{
    char *end = NULL;
    long number = strtol(value, &end, 10);
    if (*value == '\0' || *end != '\0' || number < 1 || number > 1000000) {
        return -1;
    }
    return (int)number;
}

int main(int argc, char **argv)
{
    int readers = 4;
    int writers = 2;
    int iterations = 5;
    int delay_ms = 25;

    for (int i = 1; i < argc; i++) {
        if (i + 1 >= argc) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        int parsed = positive_number(argv[++i]);
        if (parsed < 1) {
            fprintf(stderr, "Invalid numeric argument: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
        if (strcmp(argv[i - 1], "--readers") == 0) readers = parsed;
        else if (strcmp(argv[i - 1], "--writers") == 0) writers = parsed;
        else if (strcmp(argv[i - 1], "--iterations") == 0) iterations = parsed;
        else if (strcmp(argv[i - 1], "--delay-ms") == 0) delay_ms = parsed;
        else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    shared_store_t store = {
        .value = 0,
        .reads_completed = 0,
        .writes_completed = 0,
        .reader_count = readers,
        .writer_count = writers,
        .iterations = iterations,
        .delay_ms = delay_ms
    };
    if (rwlock_init(&store.lock) != 0 || pthread_mutex_init(&store.output_mutex, NULL) != 0) {
        perror("failed to initialize synchronization primitives");
        return EXIT_FAILURE;
    }

    int total_threads = readers + writers;
    pthread_t *threads = calloc((size_t)total_threads, sizeof(*threads));
    worker_arg_t *arguments = calloc((size_t)total_threads, sizeof(*arguments));
    if (!threads || !arguments) {
        perror("allocation failed");
        free(threads);
        free(arguments);
        return EXIT_FAILURE;
    }

    printf("Starting %d readers and %d writers for %d iterations...\n", readers, writers, iterations);
    int created = 0;
    for (int i = 0; i < readers; i++) {
        arguments[created] = (worker_arg_t){.store = &store, .id = i + 1};
        if (pthread_create(&threads[created], NULL, reader_worker, &arguments[created]) != 0) break;
        created++;
    }
    for (int i = 0; i < writers && created == readers + i; i++) {
        arguments[created] = (worker_arg_t){.store = &store, .id = i + 1};
        if (pthread_create(&threads[created], NULL, writer_worker, &arguments[created]) != 0) break;
        created++;
    }
    for (int i = 0; i < created; i++) pthread_join(threads[i], NULL);

    printf("\nCompleted %d reads and %d writes. Final value: %d (expected %d)\n",
           atomic_load(&store.reads_completed), atomic_load(&store.writes_completed),
           store.value, atomic_load(&store.writes_completed));

    free(threads);
    free(arguments);
    pthread_mutex_destroy(&store.output_mutex);
    rwlock_destroy(&store.lock);
    return EXIT_SUCCESS;
}
