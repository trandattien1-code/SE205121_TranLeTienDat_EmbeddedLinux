/*
 * File: circular_buffer.c
 * Purpose: Thread-safe circular buffer for sensor samples
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "circular_buffer.h"

int circular_buffer_init(CircularBuffer *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0)
        return -EINVAL;

    memset(buffer, 0, sizeof(*buffer));

    buffer->data = calloc(capacity, sizeof(SensorData));
    if (buffer->data == NULL)
        return -ENOMEM;

    buffer->capacity = capacity;

    int ret = pthread_mutex_init(&buffer->mutex, NULL);
    if (ret != 0) {
        free(buffer->data);
        buffer->data = NULL;
        return -ret;
    }

    ret = pthread_cond_init(&buffer->not_empty, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->data);
        buffer->data = NULL;
        return -ret;
    }

    return 0;
}

int circular_buffer_push(CircularBuffer *buffer,
                         const SensorData *sample)
{
    if (buffer == NULL || sample == NULL)
        return -EINVAL;

    pthread_mutex_lock(&buffer->mutex);

    if (buffer->shutdown) {
        pthread_mutex_unlock(&buffer->mutex);
        return -ECANCELED;
    }

    if (buffer->size == buffer->capacity) {
        buffer->overflow_count++;
        pthread_mutex_unlock(&buffer->mutex);
        return -ENOSPC;
    }

    buffer->data[buffer->head] = *sample;
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->size++;
    buffer->total_produced++;

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);

    return 0;
}

int circular_buffer_pop(CircularBuffer *buffer,
                        SensorData *sample)
{
    if (buffer == NULL || sample == NULL)
        return -EINVAL;

    pthread_mutex_lock(&buffer->mutex);

    while (buffer->size == 0 && !buffer->shutdown)
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);

    if (buffer->size == 0 && buffer->shutdown) {
        pthread_mutex_unlock(&buffer->mutex);
        return -ECANCELED;
    }

    *sample = buffer->data[buffer->tail];
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->size--;
    buffer->total_consumed++;

    pthread_mutex_unlock(&buffer->mutex);

    return 0;
}

void circular_buffer_stop(CircularBuffer *buffer)
{
    if (buffer == NULL)
        return;

    pthread_mutex_lock(&buffer->mutex);
    buffer->shutdown = 1;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

void circular_buffer_get_stats(CircularBuffer *buffer,
                               BufferStats *stats)
{
    if (buffer == NULL || stats == NULL)
        return;

    pthread_mutex_lock(&buffer->mutex);

    stats->current_size = buffer->size;
    stats->total_produced = buffer->total_produced;
    stats->total_consumed = buffer->total_consumed;
    stats->overflow_count = buffer->overflow_count;

    pthread_mutex_unlock(&buffer->mutex);
}

void circular_buffer_destroy(CircularBuffer *buffer)
{
    if (buffer == NULL)
        return;

    pthread_cond_destroy(&buffer->not_empty);
    pthread_mutex_destroy(&buffer->mutex);

    free(buffer->data);
    buffer->data = NULL;
}
