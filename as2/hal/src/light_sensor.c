#include "hal/light_sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


#define ADC_PATH "/sys/bus/iio/devices/iio::device0/in_voltage0_raw"
#define ADC_MAX_RAW 4095.0
#define ADC_REF_VOLTAGE 1.8

void LightSensor_init(void) {
    // Driver loads automatically
}

void LightSensor_cleanup(void) {
    // None for now
}

int LightSensor_getRawValue(void) {
    // Required to keep the most up-to-date reading
    FILE *file = fopen(ADC_PATH, "r");
    if (!file) {
        perror("ERROR: Unable to open light sensor ADC file. Is the pin configured?");
        exit(EXIT_FAILURE);
    }

    int raw_value = 0;
    if (fscanf(file, "%d", &raw_value) != 1) { 
        perror("ERROR: Failed to read raw ADC value");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return raw_value;
}

double LightSensor_getVoltage(void) {
    int raw = LightSensor_getRawValue();
    // Formula: (raw_value / max_resolution) * ref_volt
    return ((double)raw / ADC_MAX_RAW) * ADC_REF_VOLTAGE;
}