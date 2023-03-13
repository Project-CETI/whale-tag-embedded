#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "gpio.h"

int gpio_export(const char * gpio_number) {
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open /sys/class/gpio/export\n");
        return -1;
    }
    if (write(fd, gpio_number, strlen(gpio_number)) != strlen(gpio_number)) {
        fprintf(stderr, "Unable to write to /sys/class/gpio/export\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int gpio_unexport(const char * gpio_number) {
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open /sys/class/gpio/unexport\n");
        return -1;
    }
    if (write(fd, gpio_number, strlen(gpio_number)) != strlen(gpio_number)) {
        fprintf(stderr, "Unable to write to /sys/class/gpio/unexport\n");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int gpio_set_direction(const char * gpio_direction, const char * in_or_out) {
    int fd = open(gpio_direction, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error writing to %s\n", gpio_direction);
        return -1;
    }
    if (write(fd, in_or_out, strlen(in_or_out)) != strlen(in_or_out)) {
        fprintf(stderr, "Error writing to %s\n", gpio_direction);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int gpio_set_value(const char * gpio_value, const char * gpio_1_or_0) {
    int fd = open(gpio_value, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open %s\n", gpio_value);
        return -1;
    }

    if (write(fd, gpio_1_or_0, 1) != 1) {
        fprintf(stderr, "Unable to write to %s\n", gpio_value);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int gpio_get_value(const char * gpio_value) {
    int fd = open(gpio_value, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open %s\n", gpio_value);
        return -1;
    }

    char value_str[3] = {0};
    if (read(fd, value_str, 3) == 0) {
        fprintf(stderr, "Unable to read from %s\n", gpio_value);
        close(fd);
        return -1;
    }
    close(fd);
    return (value_str[0] == '1');
}