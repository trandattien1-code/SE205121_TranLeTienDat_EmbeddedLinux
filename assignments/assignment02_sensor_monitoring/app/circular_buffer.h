#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stddef.h>
#include <pthread.h>
#include "sms_common.h"

typedef struct {
    SensorData *data;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t size;

    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;

    int shutdown;
} CircularBuffer;

typedef struct {
    size_t current_size;
    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;
} BufferStats;

int circular_buffer_init(CircularBuffer *buffer, size_t capacity);

int circular_buffer_push(CircularBuffer *buffer,
                         const SensorData *sample);

int circular_buffer_pop(CircularBuffer *buffer,
                        SensorData *sample);

void circular_buffer_stop(CircularBuffer *buffer);

void circular_buffer_get_stats(CircularBuffer *buffer,
                               BufferStats *stats);

void circular_buffer_destroy(CircularBuffer *buffer);

#endif
