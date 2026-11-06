#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include <stdatomic.h>
#include <pthread.h>
#include "sms_common.h"
#include "circular_buffer.h"

typedef struct {
    CircularBuffer *buffer;
    SmsConfig *config;
    atomic_bool *running;
    unsigned long samples_read;
    unsigned long read_errors;
} SensorThreadContext;

void *sensor_thread_main(void *arg);

#endif
