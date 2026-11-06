#ifndef SMS_COMMON_H
#define SMS_COMMON_H

#include <pthread.h>
#include <mqueue.h>
#include <stdint.h>
#include <signal.h>

#define SMS_DEVICE_PATH "/dev/sms_sensor"
#define SMS_LOG_PATH "/tmp/sms.log"

#define SMS_BUFFER_CAPACITY 64
#define SMS_ROLLING_WINDOW 10

#define SMS_ALARM_QUEUE "/sms_alarm_queue"
#define SMS_LOG_QUEUE "/sms_log_queue"

typedef struct {
    uint64_t timestamp_ms;
    float temperature;
    float humidity;
} SensorData;

typedef enum {
    TEMP_HIGH = 1,
    HUMID_HIGH = 2,
    TEMP_SPIKE = 3
} AlarmType;

typedef struct {
    uint64_t timestamp_ms;
    AlarmType type;
    float value;
    float threshold;
} AlarmMessage;

typedef struct {
    uint64_t timestamp_ms;
    char message[256];
} LogMessage;

typedef struct {
    unsigned int interval_ms;
    float temp_threshold;
    float humidity_threshold;
    unsigned int duration_sec;
    int verbose;
} SmsConfig;

#endif
