#define _POSIX_C_SOURCE 200809L

#include "hal/led.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// LED trigger file paths
#define GREEN_TRIGGER "/sys/class/leds/ACT/trigger"
#define RED_TRIGGER "/sys/class/leds/PWR/trigger"

// LED brightness file paths
#define GREEN_BRIGHTNESS "/sys/class/leds/ACT/brightness"
#define RED_BRIGHTNESS "/sys/class/leds/PWR/brightness"

// Helper function to write to LED files
static void writeLedFile(const char *filePath, const char *value) {
    FILE *file = fopen(filePath, "w");

    if (file == NULL) {
        perror("Error opening LED file");
        exit(EXIT_FAILURE);
    }

    if (fprintf(file, "%s\n", value) < 0 ) {
        perror("Error writing to LED file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fclose(file) != 0) {
        perror("Error closing LED file");
        exit(EXIT_FAILURE);
    }
}


// Initialize both LEDs
void Led_initialize(void) {

    writeLedFile(GREEN_TRIGGER, "none");
    writeLedFile(RED_TRIGGER, "none");

    // Wait 100ms
    struct timespec delay = {0, 100000000};
    nanosleep(&delay, NULL);

    // Turn off
    writeLedFile(GREEN_BRIGHTNESS, "0");
    writeLedFile(RED_BRIGHTNESS, "0");
}

void Led_setGreen(bool on) {
    writeLedFile(GREEN_BRIGHTNESS, on ? "1" : "0");
}

void Led_setRed(bool on) {
    writeLedFile(RED_BRIGHTNESS, on ? "1" : "0");
}

void Led_cleanup(void) {
    Led_setGreen(false);
    Led_setRed(false);
}