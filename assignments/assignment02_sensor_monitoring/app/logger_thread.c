/*
 * File: logger_thread.c
 * Purpose: Asynchronous logging with rotation and periodic flush
 * Author: Tran Le Tien Dat - SE205121
 * Date: 2026-11-07
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include "logger_thread.h"

#define SMS_MAX_LOG_SIZE (1024 * 1024)

static int rotate_log(FILE **fp)
{
    char backup_path[256];

    if (snprintf(backup_path, sizeof(backup_path),
                 "%s.1", SMS_LOG_PATH) >= (int)sizeof(backup_path))
        return -1;

    fclose(*fp);
    *fp = NULL;

    if (rename(SMS_LOG_PATH, backup_path) != 0)
        return -1;

    *fp = fopen(SMS_LOG_PATH, "a");
    return *fp ? 0 : -1;
}

void *logger_thread_main(void *arg)
{
    LoggerThreadContext *ctx = arg;
    LogMessage msg;
    FILE *fp = fopen(SMS_LOG_PATH, "r+");

    if (fp) {
        fseek(fp, 0, SEEK_END);
    } else {
        fp = fopen(SMS_LOG_PATH, "a");
    }

    if (!fp) {
        perror("logger: fopen");
        return NULL;
    }

    time_t last_flush = time(NULL);

    while (1) {
        ssize_t n = mq_receive(ctx->log_mq,
                               (char *)&msg,
                               sizeof(msg), NULL);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            perror("logger: mq_receive");
            break;
        }

        if (msg.message[0] == '\0')
            break;

        time_t now = time(NULL);
        struct tm tm_now;
        char timestamp[64];

        localtime_r(&now, &tm_now);

        strftime(timestamp, sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S", &tm_now);

        fprintf(fp, "[%s] [%llu] %s\n",
                timestamp,
                (unsigned long long)msg.timestamp_ms,
                msg.message);

        ctx->messages_written++;

        long size = ftell(fp);

        if (size > SMS_MAX_LOG_SIZE) {
            fflush(fp);

            if (rotate_log(&fp) != 0) {
                perror("logger: rotate");
                break;
            }

            ctx->log_rotations++;
        }

        time_t current = time(NULL);

        if (current - last_flush >= 5) {
            fflush(fp);
            last_flush = current;
        }
    }

    if (fp != NULL) {
        fflush(fp);
        fclose(fp);
    }

    return NULL;
}
