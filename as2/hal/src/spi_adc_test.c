#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

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


int main(void) {
    const char *dev = "/dev/spidev0.0";

    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed =250000;

    int fd = open(dev, O_RDWR);

    if (fd < 0) {
        perror("open");
        return 1;
    } 
    
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
        perror("mode");
        return 1;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        perror("bpw");
        return 1;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        perror("speed");
        return 1;
    }

    int ch0 = read_ch(fd, 0, speed);
    int ch1 = read_ch(fd, 1, speed);

    printf("CH0=%d   CH1=%d\n", ch0, ch1);

    close(fd);
    return 0;
}