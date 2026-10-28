#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "hal/light_sensor.h"

static void sleepForMs(long long delayMs) {
    struct timespec delay;
    delay.tv_sec = delayMs / 1000;
    delay.tv_nsec = (delayMs % 1000) * 1000000;

    nanosleep(&delay, NULL);
}

int main() {
    printf("Starting Light Sensor Test...\n");
    while(1) {
        printf("Voltage: %.3f\n", LightSensor_getVoltage());
        sleepForMs(500);

    }
    return 0;
}