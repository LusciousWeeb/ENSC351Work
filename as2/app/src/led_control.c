#include "led_control.h"

#include <pthread.h>

#include "hal/pwm_led.h"
#include "hal/rotary_encoder.h"
#include "terminal_out.h"

#define START_FREQUENCY_HZ 10

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_hz = 0;

static void setFrequency(int hz) {
    if (hz < 0) {
        hz = 0;
    } else if (hz > PWM_LED_MAX_FREQUENCY_HZ) {
        hz = PWM_LED_MAX_FREQUENCY_HZ;
    }
    s_hz = hz;
    PwmLed_setFrequency(hz);
    TerminalOut_setFlashHz(hz);
}


// RUns on the rotary encoder's thread
static void onStep(int direction) {
    pthread_mutex_lock(&s_lock);
    setFrequency(s_hz + direction);
    pthread_mutex_unlock(&s_lock);
}

void LedControl_init(void) {
    PwmLed_init();

    pthread_mutex_lock(&s_lock);
    setFrequency(START_FREQUENCY_HZ);
    pthread_mutex_unlock(&s_lock);

    RotaryEncoder_setStepCallback(onStep);
    RotaryEncoder_init();
}

void LedControl_cleanup(void) {
    RotaryEncoder_cleanup();    // stop steps first...
    RotaryEncoder_setStepCallback(NULL);
    PwmLed_cleanup();           // ...then turn the LED off
}