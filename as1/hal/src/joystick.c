#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "hal/joystick.h"
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED 250000

// Threshold Values - Tested using spi_adc_test.c
#define X_LOW_THRESH 800 // lowest = 0
#define X_HIGH_THRESH 3300 // max = 4092
#define Y_LOW_THRESH 800 // lowest = 0
#define Y_HIGH_THRESH 3300 // max = 4095
#define X_MID_VAL 2054
#define Y_MID_VAL 2019

static int spiFd = -1;
static bool initialized = false;

static int read_ch(int fd, int ch, uint32_t speed_hz) {

    uint8_t tx[3] = {
        (uint8_t)(0x06 | ((ch & 0x04) >> 2)),
        (uint8_t)((ch & 0x03) << 6),
        0x00
    };
    uint8_t rx[3] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len =3,
        .speed_hz = speed_hz,
        .bits_per_word = 8,
        .cs_change = 0
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1)  {
        return -1;
    }

    return ((rx[1] & 0x0F) << 8) | rx[2]; //12-bit result
}

void Joystick_initialize(void) {
    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;

    spiFd = open(SPI_DEVICE, O_RDWR);

    if (spiFd < 0) {
        perror("Unable to open SPI device");
        exit(EXIT_FAILURE);
    }

    if (ioctl(spiFd, SPI_IOC_WR_MODE, &mode) == -1) {
        perror("Unable to set SPI mode");
        exit(EXIT_FAILURE);
    }
    
    if (ioctl(spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        perror("Unable to set SPI bits per word");
        exit(EXIT_FAILURE);
    }

    if (ioctl(spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("Unable to set SPI speed");
        exit(EXIT_FAILURE);
    }

    initialized = true;
}

JoystickDirection Joystick_getDirection(void) {

    assert(initialized);

    int x = read_ch(spiFd, 0, SPI_SPEED);
    int y = read_ch(spiFd, 1, SPI_SPEED);

    if (x < X_LOW_THRESH) {
        return JOYSTICK_LEFT;
    }

    if (x > X_HIGH_THRESH) {
        return JOYSTICK_RIGHT;
    }

    if (y < Y_LOW_THRESH) {
        return JOYSTICK_DOWN;
    }

    if (y > Y_HIGH_THRESH) {
        return JOYSTICK_UP;
    }

    return JOYSTICK_NONE;
}

void Joystick_cleanup(void) {
    assert(initialized);

    close(spiFd);
    spiFd = -1;
    initialized = false;
}