// hal/light_sensor.h
// Hardware abstraction for the light sensor (photoresistor voltage divider)
// read through the MCP3208 12-bit ADC over SPI (/dev/spidev0.0, channel 0).
//
// No thread: each call does one blocking SPI transfer (~100 us).
// Call LightSensor_init() once before reading, LightSensor_cleanup() at exit.

#ifndef HAL_LIGHT_SENSOR_H
#define HAL_LIGHT_SENSOR_H

// Open and configure the SPI device. Exits the program on failure.
void LightSensor_init(void);

// Close the SPI device.
void LightSensor_cleanup(void);

// Read the raw 12-bit ADC value (0..4095).
int LightSensor_getRawValue(void);

// Read the light level converted to volts (0..ADC reference voltage).
double LightSensor_getVoltage(void);

#endif // HAL_LIGHT_SENSOR_H