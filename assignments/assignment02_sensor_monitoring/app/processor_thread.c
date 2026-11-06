/*
 * File: processor_thread.c
 * Purpose: Process sensor samples and detect abnormal values
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "processor_thread.h"

static int send_alarm(ProcessorThreadContext *ctx,
                      const SensorData *sample,
                      AlarmType type,
                      float value,
                      float threshold)
{
    AlarmMessage alarm = {
        .timestamp_ms = sample->timestamp_ms,
        .type = type,
        .value = value,
        .threshold = threshold
    };

    return mq_send(ctx->alarm_mq,
                   (const char *)&alarm,
                   sizeof(alarm), 0);
}

void *processor_thread_main(void *arg)
{
    ProcessorThreadContext *ctx = arg;

    float temperatures[SMS_ROLLING_WINDOW] = {0};
    float humidities[SMS_ROLLING_WINDOW] = {0};

    float temp_sum = 0.0f;
    float hum_sum = 0.0f;
    float previous_temp = 0.0f;

    unsigned int count = 0;
    unsigned int index = 0;
    int has_previous = 0;

    SensorData sample;

    while (circular_buffer_pop(ctx->buffer, &sample) == 0) {
        if (count == SMS_ROLLING_WINDOW) {
            temp_sum -= temperatures[index];
            hum_sum -= humidities[index];
        } else {
            count++;
        }

        temperatures[index] = sample.temperature;
        humidities[index] = sample.humidity;

        temp_sum += sample.temperature;
        hum_sum += sample.humidity;

        index = (index + 1) % SMS_ROLLING_WINDOW;

        float temp_avg = temp_sum / count;
        float hum_avg = hum_sum / count;

        ctx->samples_processed++;

        if (sample.temperature > ctx->config->temp_threshold) {
            if (send_alarm(ctx, &sample, TEMP_HIGH,
                           sample.temperature,
                           ctx->config->temp_threshold) == 0)
                ctx->alarms_generated++;
        }

        if (sample.humidity > ctx->config->humidity_threshold) {
            if (send_alarm(ctx, &sample, HUMID_HIGH,
                           sample.humidity,
                           ctx->config->humidity_threshold) == 0)
                ctx->alarms_generated++;
        }

        if (has_previous &&
            fabsf(sample.temperature - previous_temp) > 5.0f) {
            if (send_alarm(ctx, &sample, TEMP_SPIKE,
                           fabsf(sample.temperature - previous_temp),
                           5.0f) == 0)
                ctx->alarms_generated++;
        }

        LogMessage log_msg = {0};

        log_msg.timestamp_ms = sample.timestamp_ms;

        snprintf(log_msg.message, sizeof(log_msg.message),
                 "TEMP=%.2f HUM=%.2f AVG_TEMP=%.2f AVG_HUM=%.2f",
                 sample.temperature, sample.humidity,
                 temp_avg, hum_avg);

        if (mq_send(ctx->log_mq,
                    (const char *)&log_msg,
                    sizeof(log_msg), 0) == -1)
            perror("processor: mq_send log");
        previous_temp = sample.temperature;
        has_previous = 1;
    }

    return NULL;
}
