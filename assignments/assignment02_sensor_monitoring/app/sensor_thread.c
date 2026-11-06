/*
 * File: sensor_thread.c
 * Purpose: Periodic sensor sampling thread
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "sensor_thread.h"
#include "../driver/sms_sensor.h"

static void add_ms(struct timespec *ts, unsigned int ms)
{
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (long)(ms % 1000) * 1000000L;

    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

void *sensor_thread_main(void *arg)
{
    SensorThreadContext *ctx = arg;
    struct timespec next;
    char line[128];
    int fd;

    fd = open(SMS_DEVICE_PATH, O_RDONLY);

    if (fd < 0) {
        perror("sensor: open");
        ctx->read_errors++;
        return NULL;
    }

    unsigned int interval = ctx->config->interval_ms;

    if (ioctl(fd, SMS_SET_INTERVAL, &interval) < 0) {
        perror("sensor: ioctl");
        ctx->read_errors++;
        close(fd);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &next);

    FILE *jitter_fp = NULL;
    unsigned long jitter_count = 0;
    const char *jitter_path = getenv("SMS_JITTER_CSV");

    if (jitter_path && *jitter_path) {
        jitter_fp = fopen(jitter_path, "w");
        if (jitter_fp)
            fprintf(jitter_fp, "sample,jitter_us\n");
        else
            perror("sensor: jitter file");
    }

    while (atomic_load(ctx->running)) {
        SensorData sample;
        ssize_t n;
        int ret;

        if (lseek(fd, 0, SEEK_SET) < 0) {
            ctx->read_errors++;
            break;
        }

        do {
            n = read(fd, line, sizeof(line) - 1);

            if (n < 0 && errno == EAGAIN) {
                struct timespec retry_delay = {
                    .tv_sec = 0,
                    .tv_nsec = 1000000L
                };
                nanosleep(&retry_delay, NULL);
            }
        } while (n < 0 && errno == EAGAIN &&
                 atomic_load(ctx->running));

        if (n > 0) {
            unsigned long long timestamp;

            line[n] = '\0';

            if (sscanf(line, "%llu,%f,%f",
                       &timestamp,
                       &sample.temperature,
                       &sample.humidity) == 3) {

                sample.timestamp_ms = (uint64_t)timestamp;

                ret = circular_buffer_push(ctx->buffer, &sample);

                if (ret == 0) {
                    ctx->samples_read++;

                    if (ctx->config->verbose) {
                        printf("[SENSOR] %llu,%.2f,%.2f\n",
                               timestamp,
                               sample.temperature,
                               sample.humidity);
                        fflush(stdout);
                    }
                } else if (ret == -ENOSPC) {
                    fprintf(stderr,
                            "[SENSOR] Circular buffer overflow\n");
                }
            } else {
                ctx->read_errors++;
            }
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EINTR) {
                perror("sensor: read");
            }
            ctx->read_errors++;
        }

        add_ms(&next, interval);

        do {
            ret = clock_nanosleep(CLOCK_MONOTONIC,
                                  TIMER_ABSTIME,
                                  &next, NULL);
        } while (ret == EINTR && atomic_load(ctx->running));

        if (ret != 0 && ret != EINTR) {
            fprintf(stderr, "sensor: clock_nanosleep: %s\n",
                    strerror(ret));
            ctx->read_errors++;
            break;
        }
        if (jitter_fp && jitter_count < 1000) {
            struct timespec actual;
            clock_gettime(CLOCK_MONOTONIC, &actual);

            double jitter_us =
                (double)(actual.tv_sec - next.tv_sec) * 1000000.0 +
                (double)(actual.tv_nsec - next.tv_nsec) / 1000.0;

            fprintf(jitter_fp, "%lu,%.3f\n",
                    ++jitter_count, jitter_us);
        }
    }

    if (jitter_fp)
        fclose(jitter_fp);

    close(fd);
    return NULL;
}
