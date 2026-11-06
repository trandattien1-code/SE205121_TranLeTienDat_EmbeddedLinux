#ifndef LOGGER_THREAD_H
#define LOGGER_THREAD_H

#include <stdatomic.h>
#include <mqueue.h>
#include "sms_common.h"

typedef struct {
    atomic_bool *running;
    mqd_t log_mq;
    unsigned long messages_written;
    unsigned long log_rotations;
} LoggerThreadContext;

void *logger_thread_main(void *arg);

#endif
