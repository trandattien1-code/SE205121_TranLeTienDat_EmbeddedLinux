#ifndef ALERT_THREAD_H
#define ALERT_THREAD_H

#include <stdatomic.h>
#include <mqueue.h>
#include "sms_common.h"

typedef struct {
    atomic_bool *running;
    mqd_t alarm_mq;
    mqd_t log_mq;

    unsigned long temp_high_count;
    unsigned long humid_high_count;
    unsigned long temp_spike_count;
} AlertThreadContext;

void *alert_thread_main(void *arg);

#endif
