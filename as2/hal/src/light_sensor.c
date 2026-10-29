// hal/light_sensor.c
// Reads the photoresistor divider through an MCP3208 (12-bit ADC) over SPI.
// The SPI device is opened once in init and kept open, so each read is a
// single ~100 us transfer (fast enough for the 1 ms sampling period).

#include "hal/light_sensor.h"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPI_DEVICE      "/dev/spidev0.0"
#define SPI_MODE        0
#define SPI_BITS        8
#define SPI_SPEED_HZ    250000      // MCP3208 is fine up to ~1 MHz at 3.3 V

#define ADC_CHANNEL     0           // MCP3208 channel the divider is wired to
#define ADC_MAX_RAW     4095.0      // 12-bit
// Must match the voltage on the MCP3208 VREF pin (3.3 V if tied to the 3.3 V rail).
#define ADC_REF_VOLTAGE 3.3

static int s_spiFd = -1;

void LightSensor_init(void)
{
    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS;
    uint32_t speed = SPI_SPEED_HZ;

    s_spiFd = open(SPI_DEVICE, O_RDWR);
    if (s_spiFd < 0) {
        perror("ERROR: Unable to open SPI device");
        exit(EXIT_FAILURE);
    }
    if (ioctl(s_spiFd, SPI_IOC_WR_MODE, &mode) == -1
        || ioctl(s_spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1
        || ioctl(s_spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("ERROR: Unable to configure SPI device");
        close(s_spiFd);
        exit(EXIT_FAILURE);
    }
}

void LightSensor_cleanup(void)
{
    if (s_spiFd >= 0) {
        close(s_spiFd);
        s_spiFd = -1;
    }
}

int LightSensor_getRawValue(void)
{
    // MCP3208 single-ended read: start bit, SGL=1, D2..D0 = channel
    uint8_t tx[3] = {
        (uint8_t)(0x06 | ((ADC_CHANNEL & 0x04) >> 2)),
        (uint8_t)((ADC_CHANNEL & 0x03) << 6),
        0x00
    };
    uint8_t rx[3] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = SPI_BITS,
        .cs_change = 0
    };

    if (ioctl(s_spiFd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("ERROR: SPI transfer failed");
        exit(EXIT_FAILURE);
    }

    return ((rx[1] & 0x0F) << 8) | rx[2];
}

double LightSensor_getVoltage(void)
{
    return ((double)LightSensor_getRawValue() / ADC_MAX_RAW) * ADC_REF_VOLTAGE;
}