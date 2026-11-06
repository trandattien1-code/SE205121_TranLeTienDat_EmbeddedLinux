/*
 * File: sms_app.c
 * Purpose: Main Sensor Monitoring System application
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>
#include <mqueue.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>
#include <math.h>

#include "sms_common.h"
#include "circular_buffer.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"

static void print_usage(const char *program)
{
    printf("Usage: %s [OPTIONS]\n", program);
    printf("  -i <ms>   Sampling interval (default: 500)\n");
    printf("  -t <C>    Temperature threshold (default: 40)\n");
    printf("  -h <%%>    Humidity threshold (default: 80)\n");
    printf("  -d <sec>  Run duration (default: 60)\n");
    printf("  -v        Verbose mode\n");
}

int main(int argc, char *argv[])
{
    SmsConfig config = {
        .interval_ms = 500,
        .temp_threshold = 40.0f,
        .humidity_threshold = 80.0f,
        .duration_sec = 60,
        .verbose = 0
    };

    int opt;

    while ((opt = getopt(argc, argv, "i:t:h:d:v")) != -1) {
        switch (opt) {
        case 'i':
            {
                char *end;
                errno = 0;
                unsigned long value = strtoul(optarg, &end, 10);
                if (errno || optarg[0] == '-' || *end ||
                    value == 0 || value > 60000) {
                    fprintf(stderr, "Invalid interval\n");
                    return EXIT_FAILURE;
                }
                config.interval_ms = (unsigned int)value;
            }
            break;
        case 't':
            config.temp_threshold = strtof(optarg, NULL);
            break;
        case 'h':
            config.humidity_threshold = strtof(optarg, NULL);
            break;
        case 'd':
            {
                char *end;
                errno = 0;
                unsigned long value = strtoul(optarg, &end, 10);
                if (errno || optarg[0] == '-' || *end ||
                    value > UINT_MAX) {
                    fprintf(stderr, "Invalid duration\n");
                    return EXIT_FAILURE;
                }
                config.duration_sec = (unsigned int)value;
            }
            break;
        case 'v':
            config.verbose = 1;
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("===== SMS CONFIGURATION =====\n");
    printf("Interval: %u ms\n", config.interval_ms);
    printf("Temperature threshold: %.2f C\n", config.temp_threshold);
    printf("Humidity threshold: %.2f %%\n", config.humidity_threshold);
    printf("Duration: %u sec\n", config.duration_sec);
    printf("Verbose: %s\n", config.verbose ? "ON" : "OFF");

    if (optind != argc ||
        !isfinite(config.temp_threshold) ||
        !isfinite(config.humidity_threshold) ||
        config.temp_threshold < 15.0f ||
        config.temp_threshold > 45.0f ||
        config.humidity_threshold < 30.0f ||
        config.humidity_threshold > 90.0f) {
        fprintf(stderr, "Invalid arguments\n");
        return EXIT_FAILURE;
    }

    CircularBuffer buffer;

    if (circular_buffer_init(&buffer, SMS_BUFFER_CAPACITY) != 0) {
        fprintf(stderr, "Circular buffer initialization failed\n");
        return EXIT_FAILURE;
    }

    struct mq_attr alarm_attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(AlarmMessage),
        .mq_curmsgs = 0
    };

    struct mq_attr log_attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = sizeof(LogMessage),
        .mq_curmsgs = 0
    };

    mq_unlink(SMS_ALARM_QUEUE);
    mq_unlink(SMS_LOG_QUEUE);

    mqd_t alarm_mq = mq_open(SMS_ALARM_QUEUE,
                             O_CREAT | O_RDWR,
                             0600, &alarm_attr);

    if (alarm_mq == (mqd_t)-1) {
        perror("mq_open alarm");
        circular_buffer_destroy(&buffer);
        return EXIT_FAILURE;
    }

    mqd_t log_mq = mq_open(SMS_LOG_QUEUE,
                           O_CREAT | O_RDWR,
                           0600, &log_attr);

    if (log_mq == (mqd_t)-1) {
        perror("mq_open log");
        mq_close(alarm_mq);
        mq_unlink(SMS_ALARM_QUEUE);
        circular_buffer_destroy(&buffer);
        return EXIT_FAILURE;
    }

    printf("Circular buffer initialized\n");
    printf("POSIX message queues initialized\n");

    atomic_bool running = ATOMIC_VAR_INIT(true);

    SensorThreadContext sensor_ctx = {
        .buffer = &buffer,
        .config = &config,
        .running = &running
    };

    ProcessorThreadContext processor_ctx = {
        .buffer = &buffer,
        .config = &config,
        .running = &running,
        .alarm_mq = alarm_mq,
        .log_mq = log_mq
    };

    AlertThreadContext alert_ctx = {
        .running = &running,
        .alarm_mq = alarm_mq,
        .log_mq = log_mq
    };

    LoggerThreadContext logger_ctx = {
        .running = &running,
        .log_mq = log_mq
    };

    pthread_t sensor_tid;
    pthread_t processor_tid;
    pthread_t alert_tid;
    pthread_t logger_tid;

    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);

    if (pthread_sigmask(SIG_BLOCK, &signals, NULL) != 0) {
        fprintf(stderr, "Failed to block signals\n");
        return EXIT_FAILURE;
    }

    int ret;
    int logger_started = 0;
    int alert_started = 0;
    int processor_started = 0;
    int sensor_started = 0;

    ret = pthread_create(&logger_tid, NULL,
                         logger_thread_main, &logger_ctx);
    if (ret != 0) {
        fprintf(stderr, "Logger thread failed: %d\n", ret);
        goto shutdown;
    }
    logger_started = 1;

    ret = pthread_create(&alert_tid, NULL,
                         alert_thread_main, &alert_ctx);
    if (ret != 0) {
        fprintf(stderr, "Alert thread failed: %d\n", ret);
        goto shutdown;
    }
    alert_started = 1;

    ret = pthread_create(&processor_tid, NULL,
                         processor_thread_main, &processor_ctx);
    if (ret != 0) {
        fprintf(stderr, "Processor thread failed: %d\n", ret);
        goto shutdown;
    }
    processor_started = 1;

    ret = pthread_create(&sensor_tid, NULL,
                         sensor_thread_main, &sensor_ctx);
    if (ret != 0) {
        fprintf(stderr, "Sensor thread failed: %d\n", ret);
        goto shutdown;
    }
    sensor_started = 1;

    const char *fifo_env = getenv("SMS_SENSOR_FIFO");

    if (fifo_env && strcmp(fifo_env, "1") == 0) {
        struct sched_param param = {
            .sched_priority = 50
        };

        int sched_ret = pthread_setschedparam(
            sensor_tid, SCHED_FIFO, &param
        );

        if (sched_ret != 0) {
            fprintf(stderr, "SCHED_FIFO failed: %s\n",
                    strerror(sched_ret));
            ret = -1;
            goto shutdown;
        }

        printf("Sensor Thread: SCHED_FIFO priority 50\n");
    } else {
        printf("Sensor Thread: SCHED_OTHER\n");
    }


    printf("All four threads started successfully\n");


    struct timespec start_time;
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (1) {
        struct timespec timeout = {1, 0};

        int sig = sigtimedwait(&signals, NULL, &timeout);

        if (sig == SIGINT || sig == SIGTERM) {
            printf("\nShutdown signal received\n");
            break;
        }

        if (sig == -1 && errno != EAGAIN && errno != EINTR) {
            perror("sigtimedwait");
            ret = -1;
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        if (config.duration_sec > 0 &&
            current_time.tv_sec - start_time.tv_sec >=
                (time_t)config.duration_sec) {
            printf("\nRun duration completed\n");
            break;
        }
    }


shutdown:
    atomic_store(&running, false);

    if (sensor_started)
        pthread_join(sensor_tid, NULL);

    circular_buffer_stop(&buffer);

    if (processor_started)
        pthread_join(processor_tid, NULL);

    if (alert_started) {
        AlarmMessage stop_alarm = {0};
        mq_send(alarm_mq,
                (const char *)&stop_alarm,
                sizeof(stop_alarm), 0);
        pthread_join(alert_tid, NULL);
    }

    if (logger_started) {
        LogMessage stop_log = {0};
        mq_send(log_mq,
                (const char *)&stop_log,
                sizeof(stop_log), 0);
        pthread_join(logger_tid, NULL);
    }

    BufferStats stats;
    circular_buffer_get_stats(&buffer, &stats);

    printf("\n===== SMS SUMMARY =====\n");
    printf("Samples read: %lu\n", sensor_ctx.samples_read);
    printf("Samples processed: %lu\n",
           processor_ctx.samples_processed);
    printf("Total alarms: %lu\n",
           processor_ctx.alarms_generated);
    printf("TEMP_HIGH: %lu\n", alert_ctx.temp_high_count);
    printf("HUMID_HIGH: %lu\n", alert_ctx.humid_high_count);
    printf("TEMP_SPIKE: %lu\n", alert_ctx.temp_spike_count);
    printf("Buffer overflow: %lu\n", stats.overflow_count);
    printf("Log messages: %lu\n", logger_ctx.messages_written);

    mq_close(alarm_mq);
    mq_close(log_mq);
    mq_unlink(SMS_ALARM_QUEUE);
    mq_unlink(SMS_LOG_QUEUE);

    circular_buffer_destroy(&buffer);

    printf("Graceful shutdown completed\n");

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
