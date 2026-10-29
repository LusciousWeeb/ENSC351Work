// hal/pwm_led.h
// Hardware abstraction for the LED that is flashed using PWM.
//
// The LED flashes at a chosen frequency with a fixed 50% duty cycle.
// How the PWM hardware is driven (sysfs pwmchip, pin config, ...) is hidden
// inside pwm_led.c; the rest of the program only deals in hertz.
//
// Typical use (from the code that reacts to the rotary encoder):
//   PwmLed_init();                   // once at startup; starts at 0 Hz (off)
//   PwmLed_setFrequency(10);         // flash at 10 Hz
//   ...
//   PwmLed_cleanup();                // once at shutdown; LED is turned off
 
#ifndef HAL_PWM_LED_H
#define HAL_PWM_LED_H
 
// Highest frequency the program supports (assignment: up to 500 Hz).
// If the hardware can't reach this, lower it here; callers should clamp to it.
#define PWM_LED_MAX_FREQUENCY_HZ 500
 
// Set up the PWM hardware. The LED starts off (0 Hz).
// Exits the program with an error message if the PWM can't be configured.
void PwmLed_init(void);
 
// Turn the LED off and release the PWM hardware.
void PwmLed_cleanup(void);
 
// Set the flash frequency in hertz.
//   0 Hz                     -> LED off (PWM disabled)
//   1..PWM_LED_MAX_FREQUENCY_HZ -> flash at that rate, 50% duty cycle
// Values outside 0..PWM_LED_MAX_FREQUENCY_HZ are clamped into that range.
//
// If the LED is already set to this frequency this does NOTHING, so the LED's
// flashing is not briefly interrupted by a needless re-configure
// (assignment section 1.5).
void PwmLed_setFrequency(int hz);
 
// Get the frequency the LED is currently set to (0 when off).
int PwmLed_getFrequency(void);
 
#endif // HAL_PWM_LED_H
