// hal/src/pwm_led.c
// Flashes the LED emitter (GPIO12 / header pin 32) using the BeagleY-AI PWM.
//
// The PWM is driven through sysfs files in /dev/hat/pwm/GPIO12/
// (period and duty_cycle are in nanoseconds, enable is 0 or 1).
//
// One-time board setup (see the course PWM guide):
//   1. In /boot/firmware/extlinux/extlinux.conf add, under the default label:
//        fdtoverlays /overlays/k3-am67a-beagley-ai-pwm-epwm0-gpio12.dtbo
//      then reboot.
//   2. After every reboot the pin must be exported:
//        sudo beagle-pwm-export --pin hat-32
//      PwmLed_init() tries to do this itself if the folder is missing
//      (it needs to be run as root for that to work).
//
// The hardware can only produce a period up to 469,754,879 ns (about 2.1 Hz),
// so 1 Hz and 2 Hz are produced in software: the PWM is held at a fixed period
// and a small thread switches the output fully on / fully off at the right
// rate. Everything 3 Hz and above is pure hardware PWM at 50% duty.

#include "hal/pwm_led.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef PWM_LED_SYSFS_DIR
#define PWM_LED_SYSFS_DIR "/dev/hat/pwm/GPIO12"
#endif
#define PWM_LED_EXPORT_COMMAND "beagle-pwm-export --pin hat-32"

#define NS_PER_SEC            1000000000LL
#define MAX_HW_PERIOD_NS      469754879LL       // hardware limit (course guide)
#define SLOW_MODE_PERIOD_NS   100000000LL       // fixed hw period while blinking in software

static pthread_t s_thread;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_cond;                   // wakes the slow-blink thread
static bool s_isInitialized = false;
static bool s_isRunning = false;                // protected by s_lock
static int s_hz = 0;                            // current frequency, protected by s_lock
static int s_slowHz = 0;                        // > 0 while blinking in software
static bool s_slowLedOn = false;                // state of the software blink


// Write a number to one of the PWM sysfs files. sysfs reports bad values
// (e.g. period too large) when the file is closed, so check fclose() too.
static bool writeValue(const char *fileName, long long value)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", PWM_LED_SYSFS_DIR, fileName);

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "PwmLed: unable to open %s: %s\n", path, strerror(errno));
        return false;
    }
    bool ok = (fprintf(file, "%lld\n", value) > 0);
    if (fclose(file) != 0) {
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "PwmLed: unable to write %lld to %s: %s\n",
                value, path, strerror(errno));
    }
    return ok;
}

// Program a new period/duty and (re)enable the output.
// duty_cycle is zeroed first, as the course guide recommends before changing
// the period.
static void programPwm(long long periodNs, long long dutyNs)
{
    writeValue("duty_cycle", 0);
    writeValue("period", periodNs);
    writeValue("duty_cycle", dutyNs);
    writeValue("enable", 1);
}

// Apply a new frequency. Caller holds s_lock.
static void applyFrequency(int hz)
{
    if (hz == 0) {
        s_slowHz = 0;
        writeValue("enable", 0);
        return;
    }

    long long periodNs = NS_PER_SEC / hz;
    if (periodNs > MAX_HW_PERIOD_NS) {
        // Too slow for the hardware: hold the output on, and let the
        // slow-blink thread switch it on/off.
        s_slowHz = hz;
        s_slowLedOn = true;
        programPwm(SLOW_MODE_PERIOD_NS, SLOW_MODE_PERIOD_NS);
        pthread_cond_signal(&s_cond);
    } else {
        s_slowHz = 0;
        programPwm(periodNs, periodNs / 2);
        pthread_cond_signal(&s_cond);           // slow-blink thread goes idle
    }
}

// Thread which blinks the LED for the slow (1-2 Hz) frequencies.
static void *slowBlinkThread(void *arg)
{
    (void)arg;

    pthread_mutex_lock(&s_lock);
    while (s_isRunning) {
        if (s_slowHz == 0) {
            pthread_cond_wait(&s_cond, &s_lock);    // nothing to do; sleep
            continue;
        }

        // Wait half a blink period, or until woken because something changed.
        struct timespec until;
        clock_gettime(CLOCK_MONOTONIC, &until);
        long long halfPeriodNs = (NS_PER_SEC / 2) / s_slowHz;
        long long nsec = until.tv_nsec + halfPeriodNs;
        until.tv_sec += (time_t)(nsec / NS_PER_SEC);
        until.tv_nsec = (long)(nsec % NS_PER_SEC);

        int result = pthread_cond_timedwait(&s_cond, &s_lock, &until);
        if (result == ETIMEDOUT && s_isRunning && s_slowHz > 0) {
            s_slowLedOn = !s_slowLedOn;
            writeValue("duty_cycle", s_slowLedOn ? SLOW_MODE_PERIOD_NS : 0);
        }
    }
    pthread_mutex_unlock(&s_lock);
    return NULL;
}


void PwmLed_init(void)
{
    // The sysfs folder only exists once the overlay is loaded AND the pin has
    // been exported. Try the export once if it isn't there.
    if (access(PWM_LED_SYSFS_DIR, F_OK) != 0) {
        if (system(PWM_LED_EXPORT_COMMAND) != 0) {
            fprintf(stderr, "PwmLed: export command failed (are you root?)\n");
        }
    }
    if (access(PWM_LED_SYSFS_DIR, F_OK) != 0) {
        fprintf(stderr,
                "ERROR: PWM folder %s not found.\n"
                "  1. Check extlinux.conf has the k3-am67a-beagley-ai-pwm-epwm0-gpio12.dtbo overlay and reboot.\n"
                "  2. Run: sudo beagle-pwm-export --pin hat-32\n",
                PWM_LED_SYSFS_DIR);
        exit(EXIT_FAILURE);
    }

    // Start with the LED off. A freshly exported PWM has period 0, and the
    // kernel rejects every write until the period is non-zero, so set it
    // first. Using the largest period means any leftover duty_cycle from a
    // previous run still fits under it.
    if (!writeValue("period", MAX_HW_PERIOD_NS)
        || !writeValue("duty_cycle", 0)
        || !writeValue("enable", 0)) {

        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        pthread_cond_init(&s_cond, &attr);
        pthread_condattr_destroy(&attr);

        s_hz = 0;
        s_slowHz = 0;
        s_isRunning = true;
        s_isInitialized = true;

        if (pthread_create(&s_thread, NULL, slowBlinkThread, NULL) != 0) {
            perror("ERROR: PwmLed: unable to create thread");
            exit(EXIT_FAILURE);
        }
    }
}
void PwmLed_cleanup(void)
{
    if (!s_isInitialized) {
        return;
    }

    pthread_mutex_lock(&s_lock);
    s_isRunning = false;
    s_slowHz = 0;
    pthread_cond_signal(&s_cond);
    pthread_mutex_unlock(&s_lock);
    pthread_join(s_thread, NULL);

    writeValue("enable", 0);            // LED off
    pthread_cond_destroy(&s_cond);
    s_hz = 0;
    s_isInitialized = false;
}

void PwmLed_setFrequency(int hz)
{
    if (hz < 0) {
        hz = 0;
    } else if (hz > PWM_LED_MAX_FREQUENCY_HZ) {
        hz = PWM_LED_MAX_FREQUENCY_HZ;
    }

    pthread_mutex_lock(&s_lock);
    // Already at this frequency: do nothing, so the LED isn't interrupted.
    if (hz != s_hz) {
        applyFrequency(hz);
        s_hz = hz;
    }
    pthread_mutex_unlock(&s_lock);
}

int PwmLed_getFrequency(void)
{
    pthread_mutex_lock(&s_lock);
    int hz = s_hz;
    pthread_mutex_unlock(&s_lock);
    return hz;
}