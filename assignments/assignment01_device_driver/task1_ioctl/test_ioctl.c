
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

#define DEVICE_PATH "/dev/asgn1"

static int failures = 0;

static void check_result(const char *name, int condition)
{
    if (condition) {
        printf("%s PASS\n", name);
    } else {
        printf("%s FAIL\n", name);
        failures++;
    }
}

int main(void)
{
    int fd;
    int mode;
    struct asgn1_stats stats;
    char version[ASGN1_VERSION_SIZE] = {0};
    char buffer[1024] = {0};
    ssize_t n;

    fd = open(DEVICE_PATH, O_RDWR);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("===== ASSIGNMENT 01 IOCTL TEST =====\n");

    /* TEST 1: GET_VERSION */
    printf("\n[TEST 1] GET_VERSION\n");

    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0) {
        perror("GET_VERSION");
        goto error;
    }

    printf("Driver version: %s\n", version);
    check_result("GET_VERSION", version[0] != '\0');

    /* TEST 2: KERNEL MODE */
    printf("\n[TEST 2] KERNEL MODE\n");

    mode = 0;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE kernel");
        goto error;
    }

    if (write(fd, "hello", 5) != 5) {
        perror("write kernel");
        goto error;
    }

    n = read(fd, buffer, sizeof(buffer) - 1);

    if (n < 0) {
        perror("read kernel");
        goto error;
    }

    buffer[n] = '\0';
    printf("Read: %s\n", buffer);

    check_result("KERNEL MODE",
                 strcmp(buffer, "[KERNEL]hello") == 0);

    /* TEST 3: ECHO MODE */
    printf("\n[TEST 3] ECHO MODE\n");

    mode = 1;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE echo");
        goto error;
    }

    if (write(fd, "world", 5) != 5) {
        perror("write echo");
        goto error;
    }

    /*
     * Reopen the device to reset its file offset.
     * The previous read advanced the offset to 13.
     */
    close(fd);

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("reopen");
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));

    n = read(fd, buffer, sizeof(buffer) - 1);

    if (n < 0) {
        perror("read echo");
        goto error;
    }

    buffer[n] = '\0';
    printf("Read: %s\n", buffer);

    check_result("ECHO MODE",
                 strcmp(buffer, "world") == 0);

    /* TEST 4: GET_STATS */
    printf("\n[TEST 4] GET_STATS\n");

    memset(&stats, 0, sizeof(stats));

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS");
        goto error;
    }

    printf("open_count      = %d\n", stats.open_count);
    printf("write_count     = %d\n", stats.write_count);
    printf("read_count      = %d\n", stats.read_count);
    printf("buffer_len      = %d\n", stats.buffer_len);
    printf("last_write_size = %d\n", stats.last_write_size);

    check_result("GET_STATS",
                 stats.open_count >= 2 &&
                 stats.write_count == 2 &&
                 stats.read_count == 2 &&
                 stats.buffer_len == 5 &&
                 stats.last_write_size == 5);

    /* TEST 5: RESET_BUFFER */
    printf("\n[TEST 5] RESET_BUFFER\n");

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("RESET_BUFFER");
        goto error;
    }

    memset(&stats, 0, sizeof(stats));

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS after reset");
        goto error;
    }

    printf("After reset:\n");
    printf("write_count = %d\n", stats.write_count);
    printf("read_count  = %d\n", stats.read_count);
    printf("buffer_len  = %d\n", stats.buffer_len);

    check_result("RESET STATS",
                 stats.write_count == 0 &&
                 stats.read_count == 0 &&
                 stats.buffer_len == 0 &&
                 stats.last_write_size == 0);

    /*
     * Reopen before reading to ensure offset = 0.
     */
    close(fd);

    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("reopen after reset");
        return 1;
    }

    n = read(fd, buffer, sizeof(buffer));

    if (n < 0) {
        perror("read after reset");
        goto error;
    }

    check_result("RESET BUFFER", n == 0);

    close(fd);

    printf("\n===== TEST SUMMARY =====\n");

    if (failures == 0) {
        printf("ALL TESTS PASS\n");
        return 0;
    }

    printf("FAILED TESTS: %d\n", failures);
    return 1;

error:
    close(fd);
    printf("\nTEST ABORTED DUE TO ERROR\n");
    return 1;
}
