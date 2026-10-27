#ifndef HAL_LIGHT_SENSOR_H
#define HAL_LIGHT_SENSOR_H

// Initialize the light sensor
void LightSensor_init(void);

// Clean up the light sensor
void LightSensor_cleanup(void);

// Get the current raw ADC value (int)
int LightSensor_getRawValue(void);

// Get the current light level converted to Volts (dbl)
double LightSensor_getVoltage(void);

#endif // HAL_LIGHT_SENSOR_H