#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H

#include <stdatomic.h>
#include <mqueue.h>
#include "sms_common.h"
#include "circular_buffer.h"

typedef struct {
    CircularBuffer *buffer;
    SmsConfig *config;
    atomic_bool *running;
    mqd_t alarm_mq;
    mqd_t log_mq;

    unsigned long samples_processed;
    unsigned long alarms_generated;
} ProcessorThreadContext;

void *processor_thread_main(void *arg);

#endif
