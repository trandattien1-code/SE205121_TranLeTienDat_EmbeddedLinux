CC := gcc
CROSS_CC := arm-linux-gnueabihf-gcc

CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -g -pthread
LDLIBS := -lrt -lm

SOURCES := sms_app.c \
           sensor_thread.c \
           processor_thread.c \
           alert_thread.c \
           logger_thread.c \
           circular_buffer.c

.PHONY: all arm x86 clean

all: arm x86

arm:
	$(CROSS_CC) $(CFLAGS) $(SOURCES) -o sms_app $(LDLIBS)

x86:
	$(CC) $(CFLAGS) $(SOURCES) -o sms_app_x86 $(LDLIBS)

clean:
	rm -f sms_app sms_app_x86
