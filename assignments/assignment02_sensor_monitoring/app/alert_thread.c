/*
 * File: alert_thread.c
 * Purpose: Receive and process sensor alarms
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "alert_thread.h"

static const char *alarm_type_name(AlarmType type)
{
    switch (type) {
    case TEMP_HIGH:
        return "TEMP_HIGH";
    case HUMID_HIGH:
        return "HUMID_HIGH";
    case TEMP_SPIKE:
        return "TEMP_SPIKE";
    default:
        return "UNKNOWN";
    }
}

void *alert_thread_main(void *arg)
{
    AlertThreadContext *ctx = arg;
    AlarmMessage alarm;

    while (1) {
        ssize_t n = mq_receive(ctx->alarm_mq,
                               (char *)&alarm,
                               sizeof(alarm), NULL);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            perror("alert: mq_receive");
            break;
        }

        if (alarm.type == 0)
            break;

        switch (alarm.type) {
        case TEMP_HIGH:
            ctx->temp_high_count++;
            break;
        case HUMID_HIGH:
            ctx->humid_high_count++;
            break;
        case TEMP_SPIKE:
            ctx->temp_spike_count++;
            break;
        default:
            break;
        }

        fprintf(stderr,
                "[ALERT][%llu] TYPE: %s VALUE: %.2f THRESHOLD: %.2f\n",
                (unsigned long long)alarm.timestamp_ms,
                alarm_type_name(alarm.type),
                alarm.value,
                alarm.threshold);

        LogMessage log_msg = {0};
        log_msg.timestamp_ms = alarm.timestamp_ms;

        snprintf(log_msg.message, sizeof(log_msg.message),
                 "ALERT TYPE=%s VALUE=%.2f THRESHOLD=%.2f",
                 alarm_type_name(alarm.type),
                 alarm.value,
                 alarm.threshold);

        if (mq_send(ctx->log_mq,
                    (const char *)&log_msg,
                    sizeof(log_msg), 0) == -1)
            perror("alert: mq_send log");
    }

    return NULL;
}
